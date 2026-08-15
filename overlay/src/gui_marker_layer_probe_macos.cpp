#include <SDL.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

#include "gui_data.h"
#include "gui_declarative_data.h"
#include "gui_indexed_map_macos.h"
#include "gui_interpreter.h"
#include "gui_localization.h"
#include "gui_marker_layer_macos.h"
#include "gui_text_renderer_macos.h"
#include "gui_texture_loader_macos.h"

namespace
{

const gui::GuiResolvedWidget* FindResolved(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    std::string_view name
)
{
    for (const gui::GuiResolvedWidget& widget : widgets)
    {
        if (widget.definition && widget.definition->name == name)
        {
            return &widget;
        }
    }
    return nullptr;
}

}

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    const fs::path root = argc > 1
        ? fs::path(argv[1])
        : fs::current_path();
    std::string error;

    gui::GuiInterpreter interpreter;
    if (!interpreter.LoadFile(
            root / "interface" / "china_anti_jap.sgfx",
            error
        )
        || !interpreter.LoadFile(
            root / "interface" / "china_anti_jap.sgui",
            error
        ))
    {
        std::cerr << error << '\n';
        return 1;
    }
    const gui::WindowDefinition* windowDefinition =
        interpreter.FindWindow("china_anti_jap");
    if (!windowDefinition)
    {
        std::cerr << "warmap window missing\n";
        return 1;
    }

    GuiDeclarativeDataStore dataStore;
    if (!dataStore.LoadFiles(
            {
                root / "script_gui" / "data"
                    / "china_anti_jap_common.txt",
                root / "snapshots" / "china_war_gui"
                    / "01_static.txt"
            },
            error
        ))
    {
        std::cerr << error << '\n';
        return 1;
    }
    const GuiListModel* candidates =
        dataStore.Registry()->FindList("leader_candidate_list");
    if (!candidates || candidates->items.size() < 2)
    {
        std::cerr << "leader candidate missing\n";
        return 1;
    }
    GuiListModel assigned;
    GuiListItem leader = candidates->items.front();
    leader.fields["regionid"] = int64_t{24};
	leader.fields["leadertype"] = std::string("military");
	leader.fields["assignmentorder"] = int64_t{1};
    leader.fields["x"] = int64_t{-1};
    leader.fields["y"] = int64_t{-1};
    assigned.items.push_back(std::move(leader));
	GuiListItem administrative = candidates->items[1];
	administrative.fields["regionid"] = int64_t{24};
	administrative.fields["assignmentorder"] = int64_t{2};
	administrative.fields["x"] = int64_t{-1};
	administrative.fields["y"] = int64_t{-1};
	assigned.items.push_back(std::move(administrative));
    assigned.revision = 2;
    dataStore.Registry()->SetList(
        "assigned_leader_list",
        std::move(assigned)
    );

    GuiLocalizationRegistry localization;
    if (!localization.LoadFile(
            root / "localisation" / "warmap_interface.csv",
            error
        ))
    {
        std::cerr << error << '\n';
        return 1;
    }
    GuiMacFontSet fonts;
    if (!LoadGuiMacFontDirectory(root / "font", fonts, error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << SDL_GetError() << '\n';
        DestroyGuiMacFontSet(fonts);
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow(
        "marker probe",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        1600,
        800,
        SDL_WINDOW_HIDDEN
    );
    SDL_Renderer* renderer = window
        ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE)
        : nullptr;
    if (!window || !renderer)
    {
        std::cerr << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        DestroyGuiMacFontSet(fonts);
        return 1;
    }

    GuiIndexedMapMacRuntime indexedMaps;
    if (!indexedMaps.Initialize(
            root,
            renderer,
            interpreter,
            *windowDefinition,
            error
        ))
    {
        std::cerr << error << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        DestroyGuiMacFontSet(fonts);
        return 1;
    }

    std::unordered_map<std::string, SDL_Texture*> textures;
    auto resolveTexture = [&](std::string_view name) -> SDL_Texture*
    {
        const std::string key(name);
        const auto existing = textures.find(key);
        if (existing != textures.end())
        {
            return existing->second;
        }
        const fs::path path = interpreter.ResolveTexture(key, root);
        SDL_Texture* texture = LoadGuiMacTexture(renderer, path);
        if (texture)
        {
            textures[key] = texture;
        }
        return texture;
    };

    GuiMarkerLayerMacRuntime markers;
    markers.Initialize(renderer, fonts, localization, resolveTexture);
    markers.SetData(dataStore.Registry());
    gui::GuiLayoutContext context =
        dataStore.Registry()->MakeLayoutContext();
    context.localizationResolver = [&localization](std::string_view key)
    {
        return localization.Resolve(key);
    };
    const std::vector<gui::GuiResolvedWidget> widgets =
        interpreter.ResolveWindowLayout("china_anti_jap", context);
    const gui::GuiResolvedWidget* map = FindResolved(
        widgets,
        "china_region_map"
    );
    const gui::GuiResolvedWidget* layer = FindResolved(
        widgets,
        "china_war_leader_markers"
    );
    int anchorX = 0;
    int anchorY = 0;
    gui::GuiRect mapRect;
    if (!map
        || !layer
        || !indexedMaps.ResolveDrawRect(*map, mapRect)
        || !indexedMaps.ResolveItemAnchor(*map, 24, anchorX, anchorY))
    {
        std::cerr << "marker anchor resolution failed\n";
        markers.Shutdown();
        indexedMaps.Shutdown();
        for (auto& texture : textures) SDL_DestroyTexture(texture.second);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        DestroyGuiMacFontSet(fonts);
        return 1;
    }

    indexedMaps.Refresh(context);
    indexedMaps.DrawWidget(*map);
    if (!markers.DrawWidget(*layer, widgets, indexedMaps))
    {
        std::cerr << "marker drawing failed\n";
        return 1;
    }
    const int markerX = std::clamp(
        anchorX - 34,
        mapRect.x,
        mapRect.x + mapRect.width - 68
    );
    const int markerY = std::clamp(
        anchorY - 92,
        mapRect.y,
        mapRect.y + mapRect.height - 84
    );
    const int clickX = markerX + 34;
    const int clickY = markerY + 42;
	const int stackedClickX = clickX;
	const int stackedClickY = clickY + 88;
	const GuiMarkerLayerInputResult stackedHover = markers.HandleMove(
		widgets,
		indexedMaps,
		stackedClickX,
		stackedClickY
	);
    const GuiMarkerLayerInputResult hover = markers.HandleMove(
        widgets,
        indexedMaps,
        clickX,
        clickY
    );
    const GuiMarkerLayerInputResult press = markers.HandlePress(
        widgets,
        indexedMaps,
        clickX,
        clickY
    );
    markers.HandleMove(
        widgets,
        indexedMaps,
        clickX + 30,
        clickY + 20
    );
    const GuiMarkerLayerInputResult release = markers.HandleRelease(
        widgets,
        indexedMaps,
        clickX + 30,
        clickY + 20
    );
    const bool dragEvent = std::any_of(
        release.events.begin(),
        release.events.end(),
        [](const GuiActionEvent& event)
        {
            return event.phase == GuiActionPhase::DragEnd
                && event.action == "move_war_map_leader"
                && event.parameters.find("normalizedx")
                    != event.parameters.end()
                && event.parameters.find("normalizedy")
                    != event.parameters.end();
        }
    );
    const GuiMarkerLayerInputResult clickPress = markers.HandlePress(
        widgets,
        indexedMaps,
        clickX + 30,
        clickY + 20
    );
    const GuiMarkerLayerInputResult clickRelease = markers.HandleRelease(
        widgets,
        indexedMaps,
        clickX + 30,
        clickY + 20
    );
    const int actionX = markerX + 30 - 40;
    const int actionY = markerY + 20 + 7;
    const GuiMarkerLayerInputResult actionPress = markers.HandlePress(
        widgets,
        indexedMaps,
        actionX,
        actionY
    );
    const GuiMarkerLayerInputResult actionRelease = markers.HandleRelease(
        widgets,
        indexedMaps,
        actionX,
        actionY
    );
    const bool stepDownEvent = std::any_of(
        actionRelease.events.begin(),
        actionRelease.events.end(),
        [](const GuiActionEvent& event)
        {
            return event.phase == GuiActionPhase::Click
                && event.action == "step_down_war_map_leader"
                && event.hasItemId
                && event.itemId == 1;
        }
    );
    markers.HandleMove(widgets, indexedMaps, 0, 0);
    markers.DrawWidget(*layer, widgets, indexedMaps);
    SDL_RenderPresent(renderer);
	if (!stackedHover.consumed
		|| !hover.consumed || !press.consumed || !release.consumed
        || !dragEvent || !clickPress.consumed || !clickRelease.consumed
        || !actionPress.consumed || !actionRelease.consumed
        || !stepDownEvent
        || markers.ResourceStats().textureCount == 0)
    {
        std::cerr << "marker input routing failed\n";
        return 1;
    }

    std::cout << "Region anchor: " << anchorX << ',' << anchorY << '\n';
    std::cout << "Marker drag event: yes\n";
	std::cout << "Vertical stacked marker hit-test: yes\n";
    std::cout << "Marker pinned tooltip: yes\n";
    std::cout << "Marker step-down event: yes\n";

    markers.Shutdown();
    indexedMaps.Shutdown();
    for (auto& texture : textures)
    {
        SDL_DestroyTexture(texture.second);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    DestroyGuiMacFontSet(fonts);
    return 0;
}
