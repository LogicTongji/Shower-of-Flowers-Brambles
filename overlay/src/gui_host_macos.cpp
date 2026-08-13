#include "gui_host_macos.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <optional>
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

void CollectListDefinitions(
    const gui::WidgetDefinition& widget,
    std::vector<std::string>& listNames,
    std::unordered_set<std::string>& templateNames
)
{
    if (widget.type == gui::WidgetType::ListBox)
    {
        if (!widget.name.empty())
        {
            listNames.push_back(widget.name);
        }
        if (!widget.templateName.empty())
        {
            templateNames.insert(widget.templateName);
        }
    }

    for (const gui::WidgetDefinition& child : widget.children)
    {
        CollectListDefinitions(
            child,
            listNames,
            templateNames
        );
    }
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

bool PointInside(
    const gui::GuiRect& rect,
    int x,
    int y
)
{
    return x >= rect.x
        && x < rect.x + rect.width
        && y >= rect.y
        && y < rect.y + rect.height;
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

class GuiMacWindowSession final : public IGuiApplicationEndpoint
{
public:
    using ApplicationActionInvoker = std::function<bool(
        std::string_view,
        const GuiActionContext&
    )>;

    GuiMacWindowSession(
        fs::path root,
        const GuiMacPluginLaunch& launch,
        const gui::GuiInterpreter& interpreter,
        const GuiBehaviorRegistry* behaviorRegistry,
        const GuiMacFontSet& fonts,
        const GuiLocalizationRegistry& localization,
        ApplicationActionInvoker applicationActionInvoker
    )
        : root_(std::move(root)),
          id_(launch.id),
          visibleWhen_(launch.visibleWhen),
          plugin_(*launch.plugin),
          interpreter_(interpreter),
          behaviorRegistry_(behaviorRegistry),
          fonts_(fonts),
          localization_(localization),
          applicationActionInvoker_(
              std::move(applicationActionInvoker)
          )
    {
    }

    ~GuiMacWindowSession()
    {
        Shutdown();
    }

    bool Initialize(std::string& error)
    {
        if (!windowRuntime_.Bind(
            interpreter_,
            plugin_.WindowName()
        ))
        {
            error = "GUI window not found: "
                + std::string(plugin_.WindowName());
            return false;
        }

        const gui::WindowDefinition* definition =
            windowRuntime_.Definition();
        const int windowWidth = definition
            && definition->rect.width > 0
            ? definition->rect.width
            : 1280;
        const int windowHeight = definition
            && definition->rect.height > 0
            ? definition->rect.height
            : 720;

        const Uint32 windowFlags = visibleWhen_.empty()
            ? SDL_WINDOW_SHOWN
            : SDL_WINDOW_HIDDEN;
        window_ = SDL_CreateWindow(
            std::string(plugin_.WindowTitle()).c_str(),
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
        open_ = true;
        visible_ = visibleWhen_.empty();

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
            root_,
            windowRuntime_,
            images_
        );

        if (!plugin_.Initialize(
            GuiMacPluginInitContext{
                root_,
                renderer_,
                interpreter_,
                windowRuntime_
            },
            error
        ))
        {
            return false;
        }
        pluginInitialized_ = true;
        plugin_.RegisterCustomWidgets(customWidgets_);

        if (definition
            && !indexedMaps_.Initialize(
                root_,
                renderer_,
                interpreter_,
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

        if (definition)
        {
            CollectListDefinitions(
                *definition,
                listNames_,
                listTemplateNames_
            );
        }

        SetupActionBridge();
        tickScheduler_.SetInterval(
            plugin_.TickIntervalMilliseconds()
        );
        tickScheduler_.Register(
            "plugin",
            [this](const GuiTickContext& context)
            {
                return plugin_.Tick(context.nowMilliseconds);
            }
        );
        RefreshData();
        return true;
    }

    void Tick(uint64_t nowMilliseconds)
    {
        if (!open_)
        {
            return;
        }
        const GuiTickResult tickResult = tickScheduler_.Tick(
            nowMilliseconds
        );
        if (tickResult.changed)
        {
            RefreshData();
        }
    }

    bool HandleEvent(const SDL_Event& event)
    {
        if (!open_)
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
        if (!open_ || !visible_)
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
        return open_;
    }

    std::string_view PluginId() const override
    {
        return id_;
    }

    std::string_view WindowName() const override
    {
        return plugin_.WindowName();
    }

    bool IsVisible() const override
    {
        return visible_;
    }

    void SetVisibilityMode(
        GuiWindowVisibilityMode mode
    ) override
    {
        if (mode == GuiWindowVisibilityMode::Automatic)
        {
            visibilityOverride_.reset();
        }
        else
        {
            visibilityOverride_ =
                mode == GuiWindowVisibilityMode::Shown;
        }
        UpdateWindowVisibility();
    }

    void CloseWindow() override
    {
        open_ = false;
    }

    bool DispatchPluginAction(
        const GuiActionContext& context
    ) override
    {
        const bool handled = plugin_.HandleAction(context);
        if (handled)
        {
            RefreshData();
        }
        return handled;
    }

    void Close()
    {
        CloseWindow();
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

        const GuiMacPluginResourceStats pluginStats =
            plugin_.ResourceStats();
        std::cout
            << "[GUI resources] plugin=" << id_
            << " window=" << plugin_.WindowName()
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

        if (pluginInitialized_)
        {
            plugin_.Shutdown();
            pluginInitialized_ = false;
        }

        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        open_ = false;
        visible_ = false;
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
            root_,
            interpreter_,
            std::string(name),
            images_
        );
        return FindTexture(images_, name);
    }

    void SetupActionBridge()
    {
        if (behaviorRegistry_)
        {
            actionBridge_.SetBehaviorRegistry(behaviorRegistry_);
        }
        actionBridge_.SetConditionEvaluator(
            [this](std::string_view expression)
            {
                return !layoutContext_.conditionEvaluator
                    || layoutContext_.conditionEvaluator(expression);
            }
        );
        actionBridge_.SetListItemIdResolver(
            [this](
                std::string_view listName,
                int listIndex,
                uint64_t& itemId
            )
            {
                const auto iterator = listViews_.find(
                    std::string(listName)
                );
                if (iterator == listViews_.end()
                    || listIndex < 0
                    || listIndex >= static_cast<int>(
                        iterator->second.model.size()
                    ))
                {
                    return false;
                }
                itemId = iterator->second.model.items[listIndex].id;
                return true;
            }
        );
        actionBridge_.SetFallbackInvoker(
            [this](const GuiActionContext& context)
            {
                bool handled = false;
                if ((context.fallbackOperation == "select_list_item"
                    || context.fallbackOperation == "select_item")
                    && context.hasListItemId
                    && !context.listName.empty())
                {
                    listRuntimeStore_.Get(
                        context.listName
                    ).selectedItemId = context.listItemId;
                    handled = true;
                }
                if (applicationActionInvoker_
                    && applicationActionInvoker_(id_, context))
                {
                    return true;
                }
                return plugin_.HandleAction(context) || handled;
            }
        );
    }

    void RefreshData()
    {
        dataRegistry_ = plugin_.BuildDataRegistry();
        if (!dataRegistry_)
        {
            dataRegistry_ = std::make_shared<GuiDataRegistry>();
        }
        layoutContext_ = dataRegistry_->MakeLayoutContext();
        layoutContext_.localizationResolver = [this](
            std::string_view key
        )
        {
            return localization_.Resolve(key);
        };
        markerLayers_.SetData(dataRegistry_);
        indexedMaps_.Refresh(layoutContext_);

        for (const std::string& listName : listNames_)
        {
            GuiMacListView& view = listViews_[listName];
            const GuiListModel* model = dataRegistry_->FindList(listName);
            view.model = model ? *model : GuiListModel{};

            GuiListRuntimeState& runtime =
                listRuntimeStore_.Get(listName);
            const auto selected = std::find_if(
                view.model.items.begin(),
                view.model.items.end(),
                [&runtime](const GuiListItem& item)
                {
                    return item.id == runtime.selectedItemId;
                }
            );
            if (selected == view.model.items.end())
            {
                runtime.selectedItemId = 0;
            }

            UpdateListText(listName, view);
        }
        textDirty_ = true;
        UpdateWindowVisibility();
    }

    void UpdateWindowVisibility()
    {
        const bool conditionVisible = visibleWhen_.empty()
            || (dataRegistry_
                && dataRegistry_->EvaluateCondition(visibleWhen_));
        const bool shouldBeVisible = visibilityOverride_.value_or(
            conditionVisible
        );
        if (shouldBeVisible == visible_ || !window_)
        {
            return;
        }
        visible_ = shouldBeVisible;
        if (visible_)
        {
            SDL_ShowWindow(window_);
        }
        else
        {
            SDL_HideWindow(window_);
        }
    }

    void UpdateListText(
        const std::string& listName,
        GuiMacListView& view
    )
    {
        GuiListRuntimeState& runtime =
            listRuntimeStore_.Get(listName);
        const GuiListRuntimeLayout layout =
            windowRuntime_.BuildListRuntimeLayout(
                listName,
                view.model,
                runtime,
                inputState_,
                layoutContext_
            );
        listRuntimeStore_.ScrollBy(
            listName,
            0,
            layout.maximumScroll
        );

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
            windowRuntime_,
            listName,
            layout,
            fonts_,
            layoutContext_
        );
    }

    std::vector<gui::GuiResolvedWidget> ResolveInteractiveWidgets()
    {
        std::vector<gui::GuiResolvedWidget> widgets =
            windowRuntime_.ResolveLayout(layoutContext_);
        for (const std::string& listName : listNames_)
        {
            const auto view = listViews_.find(listName);
            if (view == listViews_.end())
            {
                continue;
            }
            const GuiListRuntimeState& runtime =
                listRuntimeStore_.Get(listName);
			const GuiListRuntimeLayout listLayout =
				windowRuntime_.BuildListRuntimeLayout(
					listName,
					view->second.model,
					runtime,
					inputState_,
					layoutContext_
				);
			std::vector<gui::GuiResolvedWidget> listWidgets;
			listWidgets.reserve(listLayout.items.size());
			for (const GuiListItemRuntimeLayout& item : listLayout.items)
			{
				gui::GuiResolvedWidget widget;
				widget.definition = item.definition;
				widget.rect = item.rect;
				widget.rect.y -= listLayout.scrollOffset;
				widget.visible = item.visible;
				widget.enabled = item.enabled;
				widget.zOrder = item.definition
					? item.definition->zOrder : 0;
				widget.order = item.itemIndex;
				widget.listName = listName;
				widget.listIndex = static_cast<int>(item.itemIndex);
				listWidgets.push_back(std::move(widget));
			}
            widgets.insert(
                widgets.end(),
                listWidgets.begin(),
                listWidgets.end()
            );
        }
        return widgets;
    }

    std::size_t DispatchEvents(
        const std::vector<GuiActionEvent>& events,
        int mouseX,
        int mouseY
    )
    {
        std::vector<GuiActionEvent> resolvedEvents = events;
        for (GuiActionEvent& event : resolvedEvents)
        {
            if (!event.widget
                || event.widget->listName.empty()
                || event.widget->listIndex < 0)
            {
                continue;
            }
            const auto view = listViews_.find(
                event.widget->listName
            );
            if (view == listViews_.end()
                || event.widget->listIndex >= static_cast<int>(
                    view->second.model.items.size()
                ))
            {
                continue;
            }
            const GuiListItem& item = view->second.model.items[
                event.widget->listIndex
            ];
            for (const auto& field : item.fields)
            {
                event.parameters[field.first] =
                    GuiDataValueToText(field.second);
            }
        }
        indexedMaps_.AttachItemIds(resolvedEvents);
        const std::size_t dispatched = actionBridge_.DispatchEvents(
            windowRuntime_.Name(),
            resolvedEvents,
            mouseX,
            mouseY
        );
        if (dispatched > 0)
        {
            RefreshData();
        }
        return dispatched;
    }

    bool HandleCustomInput(
        gui::GuiCustomInputPhase phase,
        int mouseX,
        int mouseY
    )
    {
        const gui::GuiCustomWidgetContext context{
            renderer_,
            plugin_.CustomWidgetContext()
        };
        return customWidgets_.HandleInput(
            windowRuntime_.ResolveLayout(layoutContext_),
            context,
            phase,
            mouseX,
            mouseY
        );
    }

    bool PressTargetsCustomInput() const
    {
        const gui::WidgetDefinition* definition =
            inputState_.pressedSnapshot.definition;
        return inputState_.pressedKey.empty()
            || (definition
                && (definition->type == gui::WidgetType::Custom
                    || definition->type == gui::WidgetType::Window));
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
			if (!inputState_.pressedKey.empty()
				&& inputState_.pressedSnapshot.definition
				&& inputState_.pressedSnapshot.definition->draggable)
			{
				DispatchEvents(
					eventRouter_.ProcessDragMove(
						widgets,
						inputState_,
						event.motion.x,
						event.motion.y
					),
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
                DispatchEvents(
                    eventRouter_.ProcessMove(
                        widgets,
                        inputState_,
                        -1,
                        -1
                    ),
                    event.motion.x,
                    event.motion.y
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
            DispatchEvents(
                eventRouter_.ProcessMove(
                    widgets,
                    inputState_,
                    event.motion.x,
                    event.motion.y
                ),
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
            for (const std::string& listName : listNames_)
            {
                const auto view = listViews_.find(listName);
                if (view == listViews_.end())
                {
                    continue;
                }
                const GuiListRuntimeLayout layout =
                    windowRuntime_.BuildListRuntimeLayout(
                        listName,
                        view->second.model,
                        listRuntimeStore_.Get(listName),
                        inputState_,
                        layoutContext_
                    );
                if (!PointInside(layout.viewport, mouseX, mouseY))
                {
                    continue;
                }
                listRuntimeStore_.ScrollBy(
                    listName,
                    -event.wheel.y * layout.rowStep,
                    layout.maximumScroll
                );
                break;
            }
            return true;
        }

        if (event.type == SDL_MOUSEBUTTONDOWN
            && event.button.button == SDL_BUTTON_LEFT)
        {
			const std::vector<gui::GuiResolvedWidget> widgets =
				ResolveInteractiveWidgets();
			const gui::GuiResolvedWidget* directTarget =
				gui::HitTestGuiWidgets(
					widgets,
					event.button.x,
					event.button.y
				);
			const bool targetsDraggableControl = directTarget
				&& directTarget->definition
				&& directTarget->definition->draggable;
            const gui::WindowDefinition* definition =
                windowRuntime_.Definition();
            if (definition
                && definition->moveable
                && definition->dragHeight > 0
                && event.button.y < definition->dragHeight
				&& !targetsDraggableControl)
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
            DispatchEvents(
                eventRouter_.ProcessPress(
                    widgets,
                    inputState_,
                    event.button.x,
                    event.button.y
                ),
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
            DispatchEvents(
                eventRouter_.ProcessRelease(
                    widgets,
                    inputState_,
                    event.button.x,
                    event.button.y
                ),
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
                RefreshData();
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
        const bool pressed = !inputState_.pressedKey.empty()
            && inputState_.pressedSnapshot.definition == definition;
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
            interpreter_.FindProgressBar(
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
                layoutContext_.valueResolver
                ? layoutContext_.valueResolver(definition->valueSource)
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
            windowRuntime_.Definition();
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
            windowRuntime_.BuildListRuntimeLayout(
                listName,
                view->second.model,
                listRuntimeStore_.Get(listName),
                inputState_,
                layoutContext_
            ),
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
                        && layoutContext_.textResolver)
                    {
                        spriteName = layoutContext_.textResolver(source);
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
            windowRuntime_.BuildTextCommands(layoutContext_);
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
            windowRuntime_.Definition();
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
            windowRuntime_.ResolveLayout(layoutContext_);
        const std::vector<GuiRenderCommand> queue =
            BuildGuiRenderQueue(widgets, listTemplateNames_);

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
            plugin_.CustomWidgetContext()
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
                customWidgets_.DrawWidget(
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

    fs::path root_;
    std::string id_;
    std::string visibleWhen_;
    IGuiMacPlugin& plugin_;
    const gui::GuiInterpreter& interpreter_;
    const GuiBehaviorRegistry* behaviorRegistry_ = nullptr;
    const GuiMacFontSet& fonts_;
    const GuiLocalizationRegistry& localization_;
    ApplicationActionInvoker applicationActionInvoker_;
    GuiWindowRuntime windowRuntime_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool pluginInitialized_ = false;
    bool open_ = false;
    bool visible_ = false;
    GuiMacImageSet images_;
    std::unordered_map<
        const gui::WidgetDefinition*,
        GuiMacTextView
    > textViews_;
    std::shared_ptr<GuiDataRegistry> dataRegistry_;
    gui::GuiLayoutContext layoutContext_;
    std::vector<std::string> listNames_;
    std::unordered_set<std::string> listTemplateNames_;
    std::unordered_map<std::string, GuiMacListView> listViews_;
    GuiListRuntimeStore listRuntimeStore_;
    GuiEventRouter eventRouter_;
    GuiRuntimeInputState inputState_;
    GuiLuaActionBridge actionBridge_;
    GuiIndexedMapMacRuntime indexedMaps_;
    GuiMarkerLayerMacRuntime markerLayers_;
    gui::GuiCustomWidgetRegistry customWidgets_;
    GuiTickScheduler tickScheduler_;
    bool textDirty_ = true;
    bool draggingWindow_ = false;
    std::optional<bool> visibilityOverride_;
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
        std::vector<GuiMacPluginLaunch> launches,
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
        for (const GuiMacPluginLaunch& launch : launches_)
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
            endpoints.push_back(session.get());
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
    std::vector<GuiMacPluginLaunch> launches_;
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
    const std::vector<GuiMacPluginLaunch>& launches,
    const GuiMacHostOptions& options
)
{
    GuiMacHostApplication application(root, launches, options);
    return application.Run();
}

int RunGuiMacHost(
    const std::filesystem::path& root,
    IGuiMacPlugin& plugin
)
{
    return RunGuiMacHostApplication(
        root,
        {GuiMacPluginLaunch{"", "", &plugin}}
    );
}
