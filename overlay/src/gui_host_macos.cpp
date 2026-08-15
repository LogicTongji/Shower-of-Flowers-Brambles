#include "gui_host_macos.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gui_behavior.h"
#include "gui_application_bus.h"
#include "gui_control_renderer.h"
#include "gui_data.h"
#include "gui_indexed_map_macos.h"
#include "gui_interpreter.h"
#include "gui_localization.h"
#include "gui_marker_layer_macos.h"
#include "gui_render_queue.h"
#include "gui_text_renderer_macos.h"
#include "gui_texture_loader_macos.h"
#include "gui_tick.h"
#include "gui_window_session.h"

namespace fs = std::filesystem;

namespace
{

struct GuiMacImageSet
{
    std::unordered_map<std::string, SDL_Texture*> bySpriteName;
};

struct GuiMacListView
{
    GuiListModel model;
    SDL_Texture* labels = nullptr;
    std::vector<uint8_t> labelPixels;
};

struct GuiMacTextView
{
    SDL_Texture* texture = nullptr;
    std::vector<uint8_t> pixels;
};

std::string Lower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        }
    );
    return value;
}

SDL_Texture* FindTexture(
    const GuiMacImageSet& images,
    std::string_view name
)
{
    const auto iterator = images.bySpriteName.find(std::string(name));
    return iterator == images.bySpriteName.end()
        ? nullptr
        : iterator->second;
}

bool LoadSprite(
    SDL_Renderer* renderer,
    const fs::path& root,
    const gui::GuiInterpreter& interpreter,
    const std::string& resourceName,
    GuiMacImageSet& images
)
{
    if (resourceName.empty()
        || images.bySpriteName.find(resourceName)
            != images.bySpriteName.end())
    {
        return true;
    }

    const fs::path path = interpreter.ResolveTexture(
        resourceName,
        root
    );
    if (path.empty())
    {
        std::cerr << "GUI sprite is not registered: "
                  << resourceName << '\n';
        return false;
    }

    SDL_Texture* texture = LoadGuiMacTexture(renderer, path);
    if (!texture)
    {
        std::cerr << "Failed to load GUI sprite: "
                  << path << '\n';
        return false;
    }

    images.bySpriteName[resourceName] = texture;
    return true;
}

bool LoadWidgetSprites(
    SDL_Renderer* renderer,
    const fs::path& root,
    const gui::GuiInterpreter& interpreter,
    const gui::WidgetDefinition& widget,
    GuiMacImageSet& images
)
{
    bool complete = LoadSprite(
        renderer,
        root,
        interpreter,
        widget.spriteName,
        images
    );
    complete = LoadSprite(
        renderer,
        root,
        interpreter,
        widget.pressedSpriteName,
        images
    ) && complete;

    if (widget.type == gui::WidgetType::ScrollBar)
    {
        complete = LoadSprite(
            renderer,
            root,
            interpreter,
            widget.sliderName,
            images
        ) && complete;
        complete = LoadSprite(
            renderer,
            root,
            interpreter,
            widget.trackName,
            images
        ) && complete;
    }

    for (const gui::WidgetDefinition& child : widget.children)
    {
        complete = LoadWidgetSprites(
            renderer,
            root,
            interpreter,
            child,
            images
        ) && complete;
    }
    return complete;
}

bool LoadWindowSprites(
    SDL_Renderer* renderer,
    const fs::path& root,
    const GuiWindowRuntime& runtime,
    GuiMacImageSet& images
)
{
    const gui::WindowDefinition* window = runtime.Definition();
    if (!window)
    {
        return false;
    }

    bool complete = LoadSprite(
        renderer,
        root,
        runtime.Interpreter(),
        window->frameSpriteName,
        images
    );
    for (const gui::WidgetDefinition& child : window->children)
    {
        complete = LoadWidgetSprites(
            renderer,
            root,
            runtime.Interpreter(),
            child,
            images
        ) && complete;
    }
    return complete;
}

void DestroyImages(GuiMacImageSet& images)
{
    for (const auto& entry : images.bySpriteName)
    {
        SDL_DestroyTexture(entry.second);
    }
    images.bySpriteName.clear();
}

uint64_t TextureBytes(SDL_Texture* texture)
{
    if (!texture)
    {
        return 0;
    }
    Uint32 format = SDL_PIXELFORMAT_UNKNOWN;
    int width = 0;
    int height = 0;
    if (SDL_QueryTexture(
            texture,
            &format,
            nullptr,
            &width,
            &height
        ) != 0
        || width <= 0
        || height <= 0)
    {
        return 0;
    }
    const int bytesPerPixel = std::max(
        1,
        static_cast<int>(SDL_BYTESPERPIXEL(format))
    );
    return static_cast<uint64_t>(width)
        * static_cast<uint64_t>(height)
        * static_cast<uint64_t>(bytesPerPixel);
}

GuiImageScaleMode ResolveScaleMode(
    const std::string& value
)
{
    const std::string normalized = Lower(value);
    if (normalized == "center" || normalized == "none")
    {
        return GuiImageScaleMode::Center;
    }
    if (normalized == "contain"
        || normalized == "preserveaspect"
        || normalized == "aspect")
    {
        return GuiImageScaleMode::PreserveAspect;
    }
    return GuiImageScaleMode::Stretch;
}

SDL_Color ToColor(const float color[3])
{
    return {
        static_cast<Uint8>(
            std::clamp(color[0], 0.0f, 1.0f) * 255.0f
        ),
        static_cast<Uint8>(
            std::clamp(color[1], 0.0f, 1.0f) * 255.0f
        ),
        static_cast<Uint8>(
            std::clamp(color[2], 0.0f, 1.0f) * 255.0f
        ),
        SDL_ALPHA_OPAQUE
    };
}

const gui::WidgetDefinition* FindWidgetDefinition(
    const gui::WidgetDefinition& root,
    std::string_view name
)
{
    if (root.name == name)
    {
        return &root;
    }
    for (const gui::WidgetDefinition& child : root.children)
    {
        if (const gui::WidgetDefinition* found =
            FindWidgetDefinition(child, name))
        {
            return found;
        }
    }
    return nullptr;
}

const gui::WidgetDefinition* FindFirstDescendant(
    const gui::WidgetDefinition& root,
    gui::WidgetType type
)
{
    for (const gui::WidgetDefinition& child : root.children)
    {
        if (child.type == type)
        {
            return &child;
        }
        if (const gui::WidgetDefinition* found =
            FindFirstDescendant(child, type))
        {
            return found;
        }
    }
    return nullptr;
}

uint32_t EventWindowId(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_WINDOWEVENT:
        return event.window.windowID;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        return event.key.windowID;
    case SDL_TEXTEDITING:
    case SDL_TEXTINPUT:
        return event.text.windowID;
    case SDL_MOUSEMOTION:
        return event.motion.windowID;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        return event.button.windowID;
    case SDL_MOUSEWHEEL:
        return event.wheel.windowID;
    default:
        return 0;
    }
}

class GuiMacWindowSession final
{
public:
    using ApplicationActionInvoker =
        GuiWindowSessionController::ApplicationActionInvoker;

    GuiMacWindowSession(
        fs::path root,
        const GuiPluginLaunch& launch,
        const gui::GuiInterpreter& interpreter,
        const GuiBehaviorRegistry* behaviorRegistry,
        const GuiMacFontSet& fonts,
        const GuiLocalizationRegistry& localization,
        ApplicationActionInvoker applicationActionInvoker
    )
        : fonts_(fonts),
          localization_(localization),
          controller_(
              std::move(root),
              launch,
              interpreter,
              behaviorRegistry
          )
    {
        controller_.SetLocalizationResolver(
            [this](std::string_view key)
            {
                return localization_.Resolve(key);
            }
        );
        controller_.SetApplicationActionInvoker(
            std::move(applicationActionInvoker)
        );
        controller_.SetDataChangedCallback(
            [this]()
            {
                RefreshPlatformData();
            }
        );
        controller_.SetVisibilityChangedCallback(
            [this](bool visible)
            {
                if (!window_)
                {
                    return;
                }
                if (visible)
                {
                    SDL_ShowWindow(window_);
                }
                else
                {
                    SDL_HideWindow(window_);
                }
            }
        );
        controller_.SetEventResolver(
            [this](std::vector<GuiActionEvent>& events)
            {
                indexedMaps_.AttachItemIds(events);
            }
        );
    }

    ~GuiMacWindowSession()
    {
        Shutdown();
    }

    bool Initialize(std::string& error)
    {
        if (!controller_.Bind(error))
        {
            return false;
        }

        const gui::WindowDefinition* definition =
            controller_.Runtime().Definition();
        const int windowWidth = definition
            && definition->rect.width > 0
            ? definition->rect.width
            : 1280;
        const int windowHeight = definition
            && definition->rect.height > 0
            ? definition->rect.height
            : 720;

        const Uint32 windowFlags = !controller_.HasVisibilityCondition()
            ? SDL_WINDOW_SHOWN
            : SDL_WINDOW_HIDDEN;
        window_ = SDL_CreateWindow(
            std::string(controller_.Plugin().WindowTitle()).c_str(),
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            windowWidth,
            windowHeight,
            windowFlags
        );
        if (!window_)
        {
            error = "SDL_CreateWindow failed: "
                + std::string(SDL_GetError());
            return false;
        }
        renderer_ = SDL_CreateRenderer(
            window_,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
        );
        if (!renderer_)
        {
            renderer_ = SDL_CreateRenderer(
                window_,
                -1,
                SDL_RENDERER_SOFTWARE
            );
            if (!renderer_)
            {
                error = "SDL_CreateRenderer failed: "
                    + std::string(SDL_GetError());
                return false;
            }
        }

        LoadWindowSprites(
            renderer_,
            controller_.Root(),
            controller_.Runtime(),
            images_
        );

        if (definition
            && !indexedMaps_.Initialize(
                controller_.Root(),
                renderer_,
                controller_.Interpreter(),
                *definition,
                error
            ))
        {
            return false;
        }
        markerLayers_.Initialize(
            renderer_,
            fonts_,
            localization_,
            [this](std::string_view spriteName)
            {
                return ResolveSprite(spriteName);
            }
        );

        return controller_.Initialize(renderer_, error);
    }

    void Tick(uint64_t nowMilliseconds)
    {
        controller_.Tick(nowMilliseconds);
    }

    bool HandleEvent(const SDL_Event& event)
    {
        if (!controller_.IsOpen())
        {
            return false;
        }
        if (event.type == SDL_WINDOWEVENT
            && event.window.event == SDL_WINDOWEVENT_CLOSE)
        {
            Close();
            return false;
        }
        if (!ProcessEvent(event))
        {
            Close();
            return false;
        }
        return true;
    }

    bool Draw()
    {
        if (!controller_.IsOpen() || !controller_.IsVisible())
        {
            return false;
        }
        Render();
        return true;
    }

    uint32_t WindowId() const
    {
        return window_ ? SDL_GetWindowID(window_) : 0;
    }

    bool IsOpen() const
    {
        return controller_.IsOpen();
    }

    IGuiApplicationEndpoint* Endpoint()
    {
        return &controller_;
    }

    void Close()
    {
        controller_.CloseWindow();
    }

    void SetCascadeOffset(int offset)
    {
        if (!window_ || offset == 0)
        {
            return;
        }
        int x = 0;
        int y = 0;
        SDL_GetWindowPosition(window_, &x, &y);
        SDL_SetWindowPosition(window_, x + offset, y + offset);
    }

    void PrintResourceStats(std::size_t sharedFontCount) const
    {
        std::size_t hostTextureCount = 0;
        uint64_t hostTextureBytes = 0;
        uint64_t hostCpuBytes = 0;
        for (const auto& image : images_.bySpriteName)
        {
            if (image.second)
            {
                ++hostTextureCount;
                hostTextureBytes += TextureBytes(image.second);
            }
        }
        for (const auto& text : textViews_)
        {
            hostCpuBytes += text.second.pixels.capacity();
            if (text.second.texture)
            {
                ++hostTextureCount;
                hostTextureBytes += TextureBytes(text.second.texture);
            }
        }
        for (const auto& list : listViews_)
        {
            hostCpuBytes += list.second.labelPixels.capacity();
            if (list.second.labels)
            {
                ++hostTextureCount;
                hostTextureBytes += TextureBytes(list.second.labels);
            }
        }

        const GuiIndexedMapMacResourceStats indexedMapStats =
            indexedMaps_.ResourceStats();
        hostTextureCount += indexedMapStats.textureCount;
        hostTextureBytes += indexedMapStats.textureBytes;
        hostCpuBytes += indexedMapStats.cpuBytes;
        const GuiMarkerLayerMacResourceStats markerStats =
            markerLayers_.ResourceStats();
        hostTextureCount += markerStats.textureCount;
        hostTextureBytes += markerStats.textureBytes;
        hostCpuBytes += markerStats.cpuBytes;

        const GuiPluginResourceStats pluginStats =
            controller_.Plugin().ResourceStats();
        std::cout
            << "[GUI resources] plugin=" << controller_.PluginId()
            << " window=" << controller_.WindowName()
            << " host_textures=" << hostTextureCount
            << " plugin_textures=" << pluginStats.textureCount
            << " texture_bytes_approx="
            << hostTextureBytes + pluginStats.textureBytes
            << " cpu_bytes_approx="
            << hostCpuBytes + pluginStats.cpuBytes
            << " shared_fonts=" << sharedFontCount
            << '\n';
    }

private:

    void Shutdown()
    {
        for (auto& entry : listViews_)
        {
            SDL_DestroyTexture(entry.second.labels);
        }
        listViews_.clear();

        for (auto& entry : textViews_)
        {
            SDL_DestroyTexture(entry.second.texture);
        }
        textViews_.clear();
        markerLayers_.Shutdown();
        indexedMaps_.Shutdown();
        DestroyImages(images_);

        controller_.Shutdown();

        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Texture* CreateStreamingTexture(
        int width,
        int height
    ) const
    {
        if (!renderer_ || width <= 0 || height <= 0)
        {
            return nullptr;
        }
        SDL_Texture* texture = SDL_CreateTexture(
            renderer_,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            width,
            height
        );
        if (texture)
        {
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        }
        return texture;
    }

    SDL_Texture* ResolveSprite(std::string_view name)
    {
        if (name.empty())
        {
            return nullptr;
        }
        const auto existing = images_.bySpriteName.find(
            std::string(name)
        );
        if (existing != images_.bySpriteName.end())
        {
            return existing->second;
        }
        LoadSprite(
            renderer_,
            controller_.Root(),
            controller_.Interpreter(),
            std::string(name),
            images_
        );
        return FindTexture(images_, name);
    }

    void RefreshPlatformData()
    {
        markerLayers_.SetData(controller_.DataRegistry());
        indexedMaps_.Refresh(controller_.LayoutContext());

        for (const std::string& listName : controller_.ListNames())
        {
            GuiMacListView& view = listViews_[listName];
            const GuiListModel* model = controller_.FindListModel(listName);
            view.model = model ? *model : GuiListModel{};
            UpdateListText(listName, view);
        }
        textDirty_ = true;
    }

    void UpdateListText(
        const std::string& listName,
        GuiMacListView& view
    )
    {
        const GuiListRuntimeLayout layout =
            controller_.BuildListRuntimeLayout(listName);

        const int width = layout.viewport.width;
        const int height = std::max(
            layout.viewport.height,
            layout.contentHeight
        );
        if (width <= 0 || height <= 0)
        {
            SDL_DestroyTexture(view.labels);
            view.labels = nullptr;
            view.labelPixels.clear();
            return;
        }

        int currentWidth = 0;
        int currentHeight = 0;
        if (view.labels)
        {
            SDL_QueryTexture(
                view.labels,
                nullptr,
                nullptr,
                &currentWidth,
                &currentHeight
            );
        }
        if (!view.labels
            || currentWidth != width
            || currentHeight != height)
        {
            SDL_DestroyTexture(view.labels);
            view.labels = CreateStreamingTexture(width, height);
        }

        UpdateGuiListTextTexture(
            view.labels,
            view.labelPixels,
            controller_.Runtime(),
            listName,
            layout,
            fonts_,
            controller_.LayoutContext()
        );
    }

    std::vector<gui::GuiResolvedWidget> ResolveInteractiveWidgets()
    {
        return controller_.ResolveInteractiveWidgets();
    }

    std::size_t DispatchEvents(
        const std::vector<GuiActionEvent>& events,
        int mouseX,
        int mouseY
    )
    {
        return controller_.DispatchEvents(
            events,
            mouseX,
            mouseY
        );
    }

    bool HandleCustomInput(
        gui::GuiCustomInputPhase phase,
        int mouseX,
        int mouseY
    )
    {
        const gui::GuiCustomWidgetContext context{
            renderer_,
            controller_.Plugin().CustomWidgetContext()
        };
        return controller_.CustomWidgets().HandleInput(
            controller_.Runtime().ResolveLayout(
                controller_.LayoutContext()
            ),
            context,
            phase,
            mouseX,
            mouseY
        );
    }

    bool PressTargetsCustomInput() const
    {
        return controller_.PressTargetsCustomInput();
    }

    bool ProcessEvent(const SDL_Event& event)
    {
        if (event.type == SDL_KEYDOWN
            && event.key.keysym.sym == SDLK_ESCAPE)
        {
            return false;
        }

        if (event.type == SDL_MOUSEMOTION)
        {
            if (draggingWindow_)
            {
                int globalX = 0;
                int globalY = 0;
                SDL_GetGlobalMouseState(&globalX, &globalY);
                SDL_SetWindowPosition(
                    window_,
                    dragStartWindowX_ + globalX - dragStartGlobalX_,
                    dragStartWindowY_ + globalY - dragStartGlobalY_
                );
                return true;
            }

            const std::vector<gui::GuiResolvedWidget> widgets =
                ResolveInteractiveWidgets();
			if (!controller_.InputState().pressedKey.empty()
				&& controller_.InputState().pressedSnapshot.definition
				&& controller_.InputState()
					.pressedSnapshot.definition->draggable)
			{
				controller_.DispatchDragMove(
					widgets,
					event.motion.x,
					event.motion.y
				);
				return true;
			}
            GuiMarkerLayerInputResult markerResult =
                markerLayers_.HandleMove(
                    widgets,
                    indexedMaps_,
                    event.motion.x,
                    event.motion.y
                );
            if (!markerResult.events.empty())
            {
                DispatchEvents(
                    markerResult.events,
                    event.motion.x,
                    event.motion.y
                );
            }
            if (markerResult.consumed)
            {
                indexedMaps_.HandleMove(widgets, -1, -1);
                controller_.DispatchMove(
                    widgets,
                    -1,
                    -1
                );
                return true;
            }
            indexedMaps_.HandleMove(
                widgets,
                event.motion.x,
                event.motion.y
            );
            HandleCustomInput(
                gui::GuiCustomInputPhase::Move,
                event.motion.x,
                event.motion.y
            );
            controller_.DispatchMove(
                widgets,
                event.motion.x,
                event.motion.y
            );
            return true;
        }

        if (event.type == SDL_MOUSEWHEEL)
        {
            int mouseX = 0;
            int mouseY = 0;
            SDL_GetMouseState(&mouseX, &mouseY);
            controller_.ScrollListAt(
                mouseX,
                mouseY,
                -event.wheel.y
            );
            return true;
        }

        if (event.type == SDL_MOUSEBUTTONDOWN
            && event.button.button == SDL_BUTTON_LEFT)
        {
			const std::vector<gui::GuiResolvedWidget> widgets =
				ResolveInteractiveWidgets();
            if (controller_.IsWindowDragRegion(
                    event.button.x,
                    event.button.y
                ))
            {
                SDL_GetGlobalMouseState(
                    &dragStartGlobalX_,
                    &dragStartGlobalY_
                );
                SDL_GetWindowPosition(
                    window_,
                    &dragStartWindowX_,
                    &dragStartWindowY_
                );
                draggingWindow_ = true;
                return true;
            }

            GuiMarkerLayerInputResult markerResult =
                markerLayers_.HandlePress(
                    widgets,
                    indexedMaps_,
                    event.button.x,
                    event.button.y
                );
            if (!markerResult.events.empty())
            {
                DispatchEvents(
                    markerResult.events,
                    event.button.x,
                    event.button.y
                );
            }
            if (markerResult.consumed)
            {
                return true;
            }
            indexedMaps_.HandlePress(
                widgets,
                event.button.x,
                event.button.y
            );
            controller_.DispatchPress(
                widgets,
                event.button.x,
                event.button.y
            );
            if (PressTargetsCustomInput())
            {
                HandleCustomInput(
                    gui::GuiCustomInputPhase::Press,
                    event.button.x,
                    event.button.y
                );
            }
            return true;
        }

        if (event.type == SDL_MOUSEBUTTONUP
            && event.button.button == SDL_BUTTON_LEFT)
        {
            if (draggingWindow_)
            {
                draggingWindow_ = false;
                return true;
            }

            const bool routeReleaseToCustom =
                PressTargetsCustomInput();
            const std::vector<gui::GuiResolvedWidget> widgets =
                ResolveInteractiveWidgets();
            GuiMarkerLayerInputResult markerResult =
                markerLayers_.HandleRelease(
                    widgets,
                    indexedMaps_,
                    event.button.x,
                    event.button.y
                );
            if (!markerResult.events.empty())
            {
                DispatchEvents(
                    markerResult.events,
                    event.button.x,
                    event.button.y
                );
            }
            if (markerResult.consumed)
            {
                return true;
            }
            indexedMaps_.HandleRelease(
                widgets,
                event.button.x,
                event.button.y
            );
            controller_.DispatchRelease(
                widgets,
                event.button.x,
                event.button.y
            );
            if (routeReleaseToCustom
                && HandleCustomInput(
                    gui::GuiCustomInputPhase::Release,
                    event.button.x,
                    event.button.y
                ))
            {
                controller_.RefreshData();
            }
            return true;
        }

        return true;
    }

    void DrawImageWidget(const gui::GuiResolvedWidget& resolved)
    {
        const gui::WidgetDefinition* definition = resolved.definition;
        SDL_Texture* texture = definition
            ? FindTexture(images_, definition->spriteName)
            : nullptr;
        if (!texture)
        {
            return;
        }

        SDL_Rect destination{
            resolved.rect.x,
            resolved.rect.y,
            resolved.rect.width,
            resolved.rect.height
        };
        if (destination.w <= 0 || destination.h <= 0)
        {
            SDL_QueryTexture(
                texture,
                nullptr,
                nullptr,
                &destination.w,
                &destination.h
            );
        }
        DrawGuiImage(
            renderer_,
            texture,
            destination,
            ResolveScaleMode(definition->scaleMode)
        );
    }

    void DrawButtonWidget(const gui::GuiResolvedWidget& resolved)
    {
        const gui::WidgetDefinition* definition = resolved.definition;
        if (!definition)
        {
            return;
        }
        const bool pressed =
            !controller_.InputState().pressedKey.empty()
            && controller_.InputState()
                .pressedSnapshot.definition == definition;
        DrawGuiButton(
            renderer_,
            SDL_Rect{
                resolved.rect.x,
                resolved.rect.y,
                resolved.rect.width,
                resolved.rect.height
            },
            GuiButtonVisual{
                FindTexture(images_, definition->spriteName),
                FindTexture(images_, definition->pressedSpriteName)
            },
            pressed,
			resolved.enabled
        );
    }

    void DrawColorBoxWidget(const gui::GuiResolvedWidget& resolved)
    {
        if (!resolved.definition)
        {
            return;
        }
        DrawGuiColorBoxes(
            renderer_,
            std::vector<GuiColorBoxDrawCommand>{
                {
                    SDL_Rect{
                        resolved.rect.x,
                        resolved.rect.y,
                        resolved.rect.width,
                        resolved.rect.height
                    },
                    ToColor(resolved.definition->textColor)
                }
            }
        );
    }

    void DrawProgressBarWidget(const gui::GuiResolvedWidget& resolved)
    {
        const gui::WidgetDefinition* definition = resolved.definition;
        if (!definition)
        {
            return;
        }
        const gui::ProgressBarResource* resource =
            controller_.Interpreter().FindProgressBar(
                definition->progressResourceName
            );
        if (!resource)
        {
            return;
        }

        const float* fillColor = definition->progressColorIndex == 1
            ? resource->secondColor
            : resource->color;
        const float value = definition->valueSource.empty()
            ? definition->value
            : static_cast<float>(
                controller_.LayoutContext().valueResolver
                ? controller_.LayoutContext().valueResolver(
                    definition->valueSource
                )
                : 0.0
            );
        DrawGuiProgressBar(
            renderer_,
            SDL_Rect{
                resolved.rect.x,
                resolved.rect.y,
                resolved.rect.width,
                resolved.rect.height
            },
            value,
            GuiProgressVisual{
                nullptr,
                nullptr,
                SDL_Color{0, 0, 0, 0},
                ToColor(fillColor),
                resource->horizontal,
                definition->fillFromEnd,
                definition->drawBackground
            }
        );
    }

    void DrawListWidget(const gui::GuiResolvedWidget& resolved)
    {
        if (!resolved.definition)
        {
            return;
        }
        const std::string& listName = resolved.definition->name;
        const auto view = listViews_.find(listName);
        if (view == listViews_.end())
        {
            return;
        }

        const gui::WindowDefinition* window =
            controller_.Runtime().Definition();
        const gui::WidgetDefinition* listDefinition = window
            ? FindWidgetDefinition(*window, listName)
            : nullptr;
        const gui::WidgetDefinition* templateDefinition =
            listDefinition && window
            ? FindWidgetDefinition(
                *window,
                listDefinition->templateName
            )
            : nullptr;
        const gui::WidgetDefinition* imageDefinition =
            templateDefinition
            ? FindFirstDescendant(
                *templateDefinition,
                gui::WidgetType::Image
            )
            : nullptr;

        DrawGuiListWidget(
            renderer_,
            controller_.BuildListRuntimeLayout(listName),
            GuiListWidgetResources{
                [this](std::string_view spriteName) -> SDL_Texture*
                {
					return ResolveSprite(spriteName);
                },
                view->second.labels,
                [this, &view, imageDefinition](
                    std::size_t itemIndex,
                    const SDL_Rect& itemRect,
                    std::vector<
                        GuiListWidgetDrawData::ItemImage
                    >& output
                )
                {
                    if (!imageDefinition
                        || itemIndex >= view->second.model.items.size())
                    {
                        return;
                    }
                    std::string spriteName = imageDefinition->spriteName;
                    std::string source = imageDefinition->spriteSource;
                    constexpr std::string_view prefix = "item.";
                    if (source.rfind(prefix, 0) == 0)
                    {
                        const GuiDataValue* value =
                            view->second.model.items[itemIndex].Find(
                                source.substr(prefix.size())
                            );
                        spriteName = value
                            ? GuiDataValueToText(*value)
                            : std::string{};
                    }
                    else if (!source.empty()
                        && controller_.LayoutContext().textResolver)
                    {
                        spriteName = controller_.LayoutContext()
                            .textResolver(source);
                    }
                    SDL_Texture* texture = ResolveSprite(spriteName);
                    if (!texture)
                    {
                        return;
                    }
                    GuiImageScaleMode scaleMode =
                        GuiImageScaleMode::Stretch;
                    const std::string mode = Lower(
                        imageDefinition->scaleMode
                    );
                    if (mode == "preserve"
                        || mode == "preserveaspect"
                        || mode == "contain")
                    {
                        scaleMode = GuiImageScaleMode::PreserveAspect;
                    }
                    else if (mode == "center")
                    {
                        scaleMode = GuiImageScaleMode::Center;
                    }
                    output.push_back({
                        texture,
                        SDL_Rect{
                            itemRect.x + imageDefinition->rect.x,
                            itemRect.y + imageDefinition->rect.y,
                            imageDefinition->rect.width,
                            imageDefinition->rect.height
                        },
                        scaleMode
                    });
                }
            }
        );
    }

    void UpdateTextTextures()
    {
        if (!textDirty_)
        {
            return;
        }

        std::unordered_set<const gui::WidgetDefinition*> active;
        const std::vector<gui::GuiTextCommand> commands =
            controller_.Runtime().BuildTextCommands(
                controller_.LayoutContext()
            );
        for (const gui::GuiTextCommand& command : commands)
        {
            if (!command.definition
                || command.rect.width <= 0
                || command.rect.height <= 0)
            {
                continue;
            }
            active.insert(command.definition);
            GuiMacTextView& view = textViews_[command.definition];
            int width = 0;
            int height = 0;
            if (view.texture)
            {
                SDL_QueryTexture(
                    view.texture,
                    nullptr,
                    nullptr,
                    &width,
                    &height
                );
            }
            if (!view.texture
                || width != command.rect.width
                || height != command.rect.height)
            {
                SDL_DestroyTexture(view.texture);
                view.texture = CreateStreamingTexture(
                    command.rect.width,
                    command.rect.height
                );
            }
            UpdateGuiTextCommandTexture(
                view.texture,
                view.pixels,
                command,
                fonts_
            );
        }

        for (auto iterator = textViews_.begin();
            iterator != textViews_.end();)
        {
            if (active.find(iterator->first) != active.end())
            {
                ++iterator;
                continue;
            }
            SDL_DestroyTexture(iterator->second.texture);
            iterator = textViews_.erase(iterator);
        }
        textDirty_ = false;
    }

    void DrawTextWidget(const gui::GuiResolvedWidget& resolved)
    {
        const auto view = textViews_.find(resolved.definition);
        if (view == textViews_.end() || !view->second.texture)
        {
            return;
        }
        DrawGuiImage(
            renderer_,
            view->second.texture,
            SDL_Rect{
                resolved.rect.x,
                resolved.rect.y,
                resolved.rect.width,
                resolved.rect.height
            },
            GuiImageScaleMode::Stretch
        );
    }

    void DrawWindowFrame()
    {
        const gui::WindowDefinition* definition =
            controller_.Runtime().Definition();
        if (!definition || definition->frameSpriteName.empty())
        {
            return;
        }
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window_, &width, &height);
        DrawGuiImage(
            renderer_,
            FindTexture(images_, definition->frameSpriteName),
            SDL_Rect{0, 0, width, height},
            GuiImageScaleMode::Stretch
        );
    }

    void Render()
    {
        UpdateTextTextures();
        const std::vector<gui::GuiResolvedWidget> widgets =
            controller_.Runtime().ResolveLayout(
                controller_.LayoutContext()
            );
        const std::vector<GuiRenderCommand> queue =
            BuildGuiRenderQueue(
                widgets,
                controller_.ListTemplateNames()
            );

        SDL_SetRenderDrawColor(
            renderer_,
            0,
            0,
            0,
            SDL_ALPHA_OPAQUE
        );
        SDL_RenderClear(renderer_);

        const gui::GuiCustomWidgetContext customContext{
            renderer_,
            controller_.Plugin().CustomWidgetContext()
        };
        for (const GuiRenderCommand& command : queue)
        {
            if (!command.widget)
            {
                continue;
            }
            switch (command.type)
            {
            case GuiRenderCommandType::IndexedMap:
                indexedMaps_.DrawWidget(*command.widget);
                break;
            case GuiRenderCommandType::MarkerLayer:
                markerLayers_.DrawWidget(
                    *command.widget,
                    widgets,
                    indexedMaps_
                );
                break;
            case GuiRenderCommandType::Custom:
                controller_.CustomWidgets().DrawWidget(
                    *command.widget,
                    customContext
                );
                break;
            case GuiRenderCommandType::Image:
                DrawImageWidget(*command.widget);
                break;
            case GuiRenderCommandType::Button:
                DrawButtonWidget(*command.widget);
                break;
            case GuiRenderCommandType::ColorBox:
                DrawColorBoxWidget(*command.widget);
                break;
            case GuiRenderCommandType::ProgressBar:
                DrawProgressBarWidget(*command.widget);
                break;
            case GuiRenderCommandType::List:
                DrawListWidget(*command.widget);
                break;
            case GuiRenderCommandType::Text:
                DrawTextWidget(*command.widget);
                break;
            case GuiRenderCommandType::WindowFrame:
                DrawWindowFrame();
                break;
            }
        }
        SDL_RenderPresent(renderer_);
    }

    const GuiMacFontSet& fonts_;
    const GuiLocalizationRegistry& localization_;
    GuiWindowSessionController controller_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    GuiMacImageSet images_;
    std::unordered_map<
        const gui::WidgetDefinition*,
        GuiMacTextView
    > textViews_;
    std::unordered_map<std::string, GuiMacListView> listViews_;
    GuiIndexedMapMacRuntime indexedMaps_;
    GuiMarkerLayerMacRuntime markerLayers_;
    bool textDirty_ = true;
    bool draggingWindow_ = false;
    int dragStartGlobalX_ = 0;
    int dragStartGlobalY_ = 0;
    int dragStartWindowX_ = 0;
    int dragStartWindowY_ = 0;
};

class GuiMacHostApplication
{
public:
    GuiMacHostApplication(
        fs::path root,
        std::vector<GuiPluginLaunch> launches,
        GuiMacHostOptions options
    )
        : root_(std::move(root)),
          launches_(std::move(launches)),
          options_(options)
    {
    }

    ~GuiMacHostApplication()
    {
        Shutdown();
    }

    int Run()
    {
        std::string error;
        if (!Initialize(error))
        {
            std::cerr << error << '\n';
            return 1;
        }

        while (!sessions_.empty())
        {
            SDL_Event event{};
            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    for (const auto& session : sessions_)
                    {
                        session->Close();
                    }
                    break;
                }

                const uint32_t windowId = EventWindowId(event);
                if (windowId == 0)
                {
                    continue;
                }
                for (const auto& session : sessions_)
                {
                    if (session->IsOpen()
                        && session->WindowId() == windowId)
                    {
                        session->HandleEvent(event);
                        break;
                    }
                }
            }

            const std::size_t sessionCountBeforeCleanup =
                sessions_.size();
            sessions_.erase(
                std::remove_if(
                    sessions_.begin(),
                    sessions_.end(),
                    [](const auto& session)
                    {
                        return !session->IsOpen();
                    }
                ),
                sessions_.end()
            );
            if (sessions_.size() != sessionCountBeforeCleanup)
            {
                RebuildActionBus();
            }
            if (sessions_.empty())
            {
                break;
            }

            const uint64_t nowMilliseconds = SDL_GetTicks();
            for (const auto& session : sessions_)
            {
                session->Tick(nowMilliseconds);
            }

            bool rendered = false;
            for (const auto& session : sessions_)
            {
                rendered = session->Draw() || rendered;
            }
            if (!rendered)
            {
                SDL_Delay(10);
            }
        }
        return 0;
    }

private:
    bool Initialize(std::string& error)
    {
        if (launches_.empty())
        {
            error = "No GUI plugins were selected for launch";
            return false;
        }
        if (!interpreter_.LoadDirectory(root_ / "interface", error))
        {
            error = "Failed to load GUI definitions: " + error;
            return false;
        }

        const fs::path behaviorRoot = fs::exists(root_ / "script_gui")
            ? root_ / "script_gui"
            : root_ / "scripted_guis";
        if (fs::exists(behaviorRoot))
        {
            std::string behaviorError;
            if (behaviorRegistry_.LoadDirectory(
                    behaviorRoot,
                    behaviorError
                ))
            {
                behaviorRegistryLoaded_ = true;
            }
            else
            {
                std::cerr << "Behavior definition warning: "
                          << behaviorError << '\n';
            }
        }

        std::string fontError;
        if (!LoadGuiMacFontDirectory(
                root_ / "font",
                fonts_,
                fontError
            ))
        {
            std::cerr << "Font loading warning: "
                      << fontError << '\n';
        }

        std::string localizationError;
        if (!localization_.LoadDirectory(
                root_ / "localisation",
                localizationError
            ))
        {
            std::cerr << "Localization loading warning: "
                      << localizationError << '\n';
        }

        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            error = "SDL_Init failed: "
                + std::string(SDL_GetError());
            return false;
        }
        sdlInitialized_ = true;
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

        int sessionIndex = 0;
        for (const GuiPluginLaunch& launch : launches_)
        {
            if (!launch.plugin)
            {
                error = "GUI plugin launch has no plugin instance: "
                    + launch.id;
                return false;
            }

            auto session = std::make_unique<GuiMacWindowSession>(
                root_,
                launch,
                interpreter_,
                behaviorRegistryLoaded_ ? &behaviorRegistry_ : nullptr,
                fonts_,
                localization_,
                [this](
                    std::string_view sourcePluginId,
                    const GuiActionContext& context
                )
                {
                    return actionBus_.Dispatch(
                        sourcePluginId,
                        context
                    );
                }
            );
            std::string sessionError;
            if (!session->Initialize(sessionError))
            {
                error = "Failed to initialize GUI plugin "
                    + launch.id + ": " + sessionError;
                return false;
            }
            session->SetCascadeOffset(sessionIndex * 28);
            sessions_.push_back(std::move(session));
            ++sessionIndex;
        }
        RebuildActionBus();
        if (options_.printResourceStats)
        {
            for (const auto& session : sessions_)
            {
                session->PrintResourceStats(fonts_.byName.size());
            }
        }
        return true;
    }

    void RebuildActionBus()
    {
        std::vector<IGuiApplicationEndpoint*> endpoints;
        endpoints.reserve(sessions_.size());
        for (const auto& session : sessions_)
        {
            endpoints.push_back(session->Endpoint());
        }
        actionBus_.SetEndpoints(std::move(endpoints));
    }

    void Shutdown()
    {
        actionBus_.SetEndpoints({});
        sessions_.clear();
        DestroyGuiMacFontSet(fonts_);
        if (sdlInitialized_)
        {
            SDL_Quit();
            sdlInitialized_ = false;
        }
    }

    fs::path root_;
    std::vector<GuiPluginLaunch> launches_;
    GuiMacHostOptions options_;
    gui::GuiInterpreter interpreter_;
    GuiBehaviorRegistry behaviorRegistry_;
    GuiApplicationActionBus actionBus_;
    GuiMacFontSet fonts_;
    GuiLocalizationRegistry localization_;
    bool behaviorRegistryLoaded_ = false;
    bool sdlInitialized_ = false;
    std::vector<std::unique_ptr<GuiMacWindowSession>> sessions_;
};

}

int RunGuiMacHostApplication(
    const std::filesystem::path& root,
    const std::vector<GuiPluginLaunch>& launches,
    const GuiMacHostOptions& options
)
{
    GuiMacHostApplication application(root, launches, options);
    return application.Run();
}

int RunGuiMacHost(
    const std::filesystem::path& root,
    IGuiPlugin& plugin
)
{
    return RunGuiMacHostApplication(
        root,
        {GuiPluginLaunch{"", "", &plugin}}
    );
}
