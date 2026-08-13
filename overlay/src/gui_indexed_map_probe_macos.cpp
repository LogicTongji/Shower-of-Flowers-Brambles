#include "gui_indexed_map_macos.h"

#include <SDL.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

#include "gui_interpreter.h"
#include "gui_indexed_map_core.h"

int main(int argc, char** argv)
{
    const std::filesystem::path root = argc >= 2
        ? std::filesystem::path(argv[1])
        : std::filesystem::current_path();

    gui::GuiInterpreter interpreter;
    std::string error;
    if (!interpreter.LoadDirectory(root / "interface", error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    const gui::WindowDefinition* window = nullptr;
    std::string windowName;
    for (const gui::WindowDefinition& candidate : interpreter.Windows())
    {
        const std::vector<gui::GuiResolvedWidget> widgets =
            interpreter.ResolveWindowLayout(candidate.name);
        const bool hasIndexedMap = std::any_of(
            widgets.begin(),
            widgets.end(),
            [](const gui::GuiResolvedWidget& widget)
            {
                return widget.definition
                    && widget.definition->type
                        == gui::WidgetType::IndexedMap;
            }
        );
        if (hasIndexedMap)
        {
            window = &candidate;
            windowName = candidate.name;
            break;
        }
    }
    if (!window)
    {
        std::cerr << "No window with an indexed map was found\n";
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0,
        std::max(1, window->rect.width),
        std::max(1, window->rect.height),
        32,
        SDL_PIXELFORMAT_RGBA32
    );
    SDL_Renderer* renderer = surface
        ? SDL_CreateSoftwareRenderer(surface)
        : nullptr;
    if (!surface || !renderer)
    {
        std::cerr << "Failed to create probe renderer: "
                  << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_FreeSurface(surface);
        SDL_Quit();
        return 1;
    }

    GuiIndexedMapMacRuntime indexedMaps;
    if (!indexedMaps.Initialize(
            root,
            renderer,
            interpreter,
            *window,
            error
        ))
    {
        std::cerr << error << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_FreeSurface(surface);
        SDL_Quit();
        return 1;
    }

    const gui::GuiLayoutContext context{
        [](std::string_view)
        {
            return true;
        },
        {},
        {},
        [](std::string_view source)
        {
            const size_t firstDot = source.find('.');
            if (firstDot == std::string_view::npos)
            {
                return 0.0;
            }
            const size_t secondDot = source.find('.', firstDot + 1);
            if (secondDot == std::string_view::npos)
            {
                return 0.0;
            }
            try
            {
                const int itemId = std::stoi(std::string(
                    source.substr(
                        firstDot + 1,
                        secondDot - firstDot - 1
                    )
                ));
                return static_cast<double>((itemId * 17) % 101);
            }
            catch (...)
            {
                return 0.0;
            }
        }
    };
    indexedMaps.Refresh(context);
    const std::vector<gui::GuiResolvedWidget> widgets =
        interpreter.ResolveWindowLayout(windowName, context);
    indexedMaps.Draw(widgets);
    SDL_RenderPresent(renderer);

    const gui::GuiResolvedWidget* mapWidget = nullptr;
    for (const gui::GuiResolvedWidget& widget : widgets)
    {
        if (widget.definition
            && widget.definition->type == gui::WidgetType::IndexedMap)
        {
            mapWidget = &widget;
            break;
        }
    }
    IndexedMapData idMap;
    if (!mapWidget
        || !LoadIndexedMapData(
            interpreter.ResolveIndexedMapIndex(
                mapWidget->definition->indexedMapResourceName,
                root
            ),
            idMap
        ))
    {
        std::cerr << "Failed to prepare indexed-map click probe\n";
        indexedMaps.Shutdown();
        SDL_DestroyRenderer(renderer);
        SDL_FreeSurface(surface);
        SDL_Quit();
        return 1;
    }

    const double scale = std::min(
        static_cast<double>(mapWidget->rect.width) / idMap.width,
        static_cast<double>(mapWidget->rect.height) / idMap.height
    );
    const int mapWidth = std::max(
        1,
        static_cast<int>(idMap.width * scale)
    );
    const int mapHeight = std::max(
        1,
        static_cast<int>(idMap.height * scale)
    );
    const int mapX = mapWidget->rect.x
        + (mapWidget->rect.width - mapWidth) / 2;
    const int mapY = mapWidget->rect.y
        + (mapWidget->rect.height - mapHeight) / 2;
    int clickX = -1;
    int clickY = -1;
    uint16_t expectedItemId = 0;
    for (int y = 0; y < mapHeight && expectedItemId == 0; ++y)
    {
        for (int x = 0; x < mapWidth; ++x)
        {
            const int textureX = x * static_cast<int>(idMap.width)
                / mapWidth;
            const int textureY = y * static_cast<int>(idMap.height)
                / mapHeight;
            const uint16_t itemId = idMap.itemIds[
                static_cast<size_t>(textureY) * idMap.width
                + static_cast<size_t>(textureX)
            ];
            if (itemId != 0)
            {
                clickX = mapX + x;
                clickY = mapY + y;
                expectedItemId = itemId;
                break;
            }
        }
    }

    GuiEventRouter eventRouter;
    GuiRuntimeInputState inputState;
    indexedMaps.HandlePress(widgets, clickX, clickY);
    eventRouter.ProcessPress(
        widgets,
        inputState,
        clickX,
        clickY
    );
    indexedMaps.HandleRelease(widgets, clickX, clickY);
    std::vector<GuiActionEvent> clickEvents =
        eventRouter.ProcessRelease(
            widgets,
            inputState,
            clickX,
            clickY
        );
    indexedMaps.AttachItemIds(clickEvents);
    bool clickValid = false;
    for (const GuiActionEvent& event : clickEvents)
    {
        if (event.phase == GuiActionPhase::Click
            && event.hasItemId
            && event.itemId == expectedItemId)
        {
            clickValid = true;
            break;
        }
    }

    const GuiIndexedMapMacResourceStats stats =
        indexedMaps.ResourceStats();
    std::cout << "Indexed-map probe: textures="
              << stats.textureCount
              << " texture_bytes=" << stats.textureBytes
              << " cpu_bytes=" << stats.cpuBytes
              << '\n';

    const bool valid = stats.textureCount >= 3
        && stats.textureBytes > 0
        && stats.cpuBytes > 0
        && clickValid;
    std::cout << "Indexed-map click: item="
              << expectedItemId
              << " dispatched=" << (clickValid ? "yes" : "no")
              << '\n';
    indexedMaps.Shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    SDL_Quit();
    return valid ? 0 : 1;
}
