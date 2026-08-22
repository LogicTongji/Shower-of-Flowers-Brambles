#include "gui_window_session.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace
{

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
        CollectListDefinitions(child, listNames, templateNames);
    }
}

void CollectTemplateDefinitions(
    const gui::WidgetDefinition& widget,
    const std::unordered_set<std::string>& templateNames,
    std::unordered_set<const gui::WidgetDefinition*>& definitions,
    bool insideTemplate = false
)
{
    insideTemplate = insideTemplate
        || templateNames.find(widget.name) != templateNames.end();
    if (insideTemplate)
    {
        definitions.insert(&widget);
    }
    for (const gui::WidgetDefinition& child : widget.children)
    {
        CollectTemplateDefinitions(
            child,
            templateNames,
            definitions,
            insideTemplate
        );
    }
}

bool PointInside(const gui::GuiRect& rect, int x, int y)
{
    return x >= rect.x
        && y >= rect.y
        && x < rect.x + rect.width
        && y < rect.y + rect.height;
}

bool IntersectRects(
    const gui::GuiRect& first,
    const gui::GuiRect& second,
    gui::GuiRect& output
)
{
    const int left = std::max(first.x, second.x);
    const int top = std::max(first.y, second.y);
    const int right = std::min(
        first.x + first.width,
        second.x + second.width
    );
    const int bottom = std::min(
        first.y + first.height,
        second.y + second.height
    );
    output = {
        left,
        top,
        std::max(0, right - left),
        std::max(0, bottom - top)
    };
    return output.width > 0 && output.height > 0;
}

std::string ReplaceItemId(std::string value, uint64_t itemId)
{
    constexpr std::string_view placeholder = "{id}";
    const std::string replacement = std::to_string(itemId);
    std::size_t position = value.find(placeholder);
    while (position != std::string::npos)
    {
        value.replace(position, placeholder.size(), replacement);
        position = value.find(
            placeholder,
            position + replacement.size()
        );
    }
    return value;
}

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

std::string FindParameter(
    const GuiActionContext& context,
    std::string_view name
)
{
    const auto found = context.parameters.find(
        Lower(std::string(name))
    );
    return found == context.parameters.end()
        ? std::string{}
        : found->second;
}

bool ParseBoolean(std::string value)
{
    value = Lower(std::move(value));
    return !value.empty()
        && value != "no"
        && value != "false"
        && value != "off"
        && value != "0";
}

void ApplyStaticWindowData(
    const gui::WindowDefinition* definition,
    GuiDataRegistry& registry
)
{
    if (!definition)
    {
        return;
    }
    for (const gui::StaticDataValueDefinition& value
        : definition->staticValues)
    {
        registry.Set(value.name, value.value);
    }
    for (const gui::StaticDataListDefinition& list
        : definition->staticLists)
    {
        registry.SetList(list.name, list.model);
    }
}

}

GuiWindowSessionController::GuiWindowSessionController(
    std::filesystem::path root,
    const GuiPluginLaunch& launch,
    const gui::GuiInterpreter& interpreter,
    const GuiBehaviorRegistry* behaviorRegistry
)
    : root_(std::move(root)),
      id_(launch.id),
      visibleWhen_(launch.visibleWhen),
      initiallyOpen_(launch.openInitially),
      plugin_(launch.plugin),
      interpreter_(interpreter),
      behaviorRegistry_(behaviorRegistry)
{
}

GuiWindowSessionController::~GuiWindowSessionController()
{
    Shutdown();
}

void GuiWindowSessionController::SetApplicationActionInvoker(
    ApplicationActionInvoker invoker
)
{
    applicationActionInvoker_ = std::move(invoker);
}

void GuiWindowSessionController::SetLocalizationResolver(
    LocalizationResolver resolver
)
{
    localizationResolver_ = std::move(resolver);
}

void GuiWindowSessionController::SetDataChangedCallback(
    DataChangedCallback callback
)
{
    dataChangedCallback_ = std::move(callback);
}

void GuiWindowSessionController::SetSessionChangedCallback(
    SessionChangedCallback callback
)
{
    sessionChangedCallback_ = std::move(callback);
}

void GuiWindowSessionController::SetPersistenceStore(
    std::shared_ptr<GuiPersistenceStore> store
)
{
    persistenceStore_ = std::move(store);
    persistenceLoaded_ = false;
}

void GuiWindowSessionController::SetVisibilityChangedCallback(
    VisibilityChangedCallback callback
)
{
    visibilityChangedCallback_ = std::move(callback);
}

void GuiWindowSessionController::SetEventResolver(
    EventResolver resolver
)
{
    eventResolver_ = std::move(resolver);
}

bool GuiWindowSessionController::Bind(std::string& error)
{
    if (!plugin_)
    {
        error = "GUI plugin launch has no plugin instance: " + id_;
        return false;
    }
    if (!windowRuntime_.Bind(interpreter_, plugin_->WindowName()))
    {
        error = "GUI window not found: "
            + std::string(plugin_->WindowName());
        return false;
    }

    listNames_.clear();
    listTemplateNames_.clear();
    listTemplateDefinitions_.clear();
    if (const gui::WindowDefinition* definition =
            windowRuntime_.Definition())
    {
        CollectListDefinitions(
            *definition,
            listNames_,
            listTemplateNames_
        );
        CollectTemplateDefinitions(
            *definition,
            listTemplateNames_,
            listTemplateDefinitions_
        );
    }
    return true;
}

bool GuiWindowSessionController::Initialize(
    void* graphicsContext,
    std::string& error
)
{
    if (!plugin_ || !windowRuntime_.IsBound())
    {
        error = "GUI session must be bound before initialization";
        return false;
    }
    if (!plugin_->Initialize(
            GuiPluginInitContext{
                root_,
                graphicsContext,
                interpreter_,
                windowRuntime_
            },
            error
        ))
    {
        return false;
    }

    pluginInitialized_ = true;
    open_ = initiallyOpen_;
    plugin_->RegisterCustomWidgets(customWidgets_);
    SetupActionBridge();
    tickScheduler_.SetInterval(plugin_->TickIntervalMilliseconds());
    tickScheduler_.Register(
        "plugin",
        [this](const GuiTickContext& context)
        {
            return plugin_->Tick(context.nowMilliseconds);
        }
    );
    RefreshData();
    return true;
}

void GuiWindowSessionController::Shutdown()
{
    SavePersistentState();
    tickScheduler_.Clear();
    if (pluginInitialized_ && plugin_)
    {
        plugin_->Shutdown();
    }
    pluginInitialized_ = false;
    open_ = false;
    visible_ = false;
    dataRegistry_.reset();
    layoutContext_ = {};
    listModels_.clear();
    listRuntimeStore_.Clear();
    persistentValues_.clear();
    persistentLists_.clear();
    inputState_ = {};
    sessionObserved_ = false;
    sessionId_.clear();
    persistenceKey_.clear();
    persistenceError_.clear();
    persistenceLoaded_ = false;
    persistentStateDirty_ = false;
}

bool GuiWindowSessionController::Tick(uint64_t nowMilliseconds)
{
    if (!pluginInitialized_)
    {
        return false;
    }
    const GuiTickResult result = tickScheduler_.Tick(nowMilliseconds);
    if (result.changed)
    {
        RefreshData();
    }
    return result.changed;
}

void GuiWindowSessionController::RefreshData()
{
    if (!plugin_)
    {
        return;
    }
    dataRegistry_ = plugin_->BuildDataRegistry();
    if (!dataRegistry_)
    {
        dataRegistry_ = std::make_shared<GuiDataRegistry>();
    }
    ApplySessionBoundary(dataRegistry_);
    const bool authoritativePersistence =
        dataRegistry_->ResolveBool("state.persistenceavailable");
    if (authoritativePersistence)
    {
        if (!persistentValues_.empty() || !persistentLists_.empty())
        {
            persistentValues_.clear();
            persistentLists_.clear();
            persistentStateDirty_ = true;
        }
    }
    else
    {
        for (const auto& entry : persistentValues_)
        {
            dataRegistry_->Set(entry.first, entry.second);
        }
        for (const auto& entry : persistentLists_)
        {
            dataRegistry_->SetList(entry.first, entry.second);
        }
    }
    ApplyStaticWindowData(windowRuntime_.Definition(), *dataRegistry_);
    optimisticDataStore_.SetRegistry(dataRegistry_);
    layoutContext_ = dataRegistry_->MakeLayoutContext();
    layoutContext_.localizationResolver = localizationResolver_;

    for (const std::string& listName : listNames_)
    {
        const GuiListModel* source = dataRegistry_->FindList(listName);
        GuiListModel& model = listModels_[listName];
        model = source ? *source : GuiListModel{};

        GuiListRuntimeState& runtime = listRuntimeStore_.Get(listName);
        const auto selected = std::find_if(
            model.items.begin(),
            model.items.end(),
            [&runtime](const GuiListItem& item)
            {
                return item.id == runtime.selectedItemId;
            }
        );
        if (selected == model.items.end())
        {
            runtime.selectedItemId = 0;
        }

        const GuiListRuntimeLayout layout =
            BuildListRuntimeLayout(listName);
        listRuntimeStore_.ScrollBy(
            listName,
            0,
            layout.maximumScroll
        );
        if (!persistenceKey_.empty())
        {
            persistentStateDirty_ = true;
            SavePersistentState();
        }
    }

    if (dataChangedCallback_)
    {
        dataChangedCallback_();
    }
    UpdateVisibility();
}

bool GuiWindowSessionController::IsOpen() const
{
    return open_;
}

bool GuiWindowSessionController::HasVisibilityCondition() const
{
    return !visibleWhen_.empty();
}

std::string_view GuiWindowSessionController::PluginId() const
{
    return id_;
}

std::string_view GuiWindowSessionController::WindowName() const
{
    return plugin_ ? plugin_->WindowName() : std::string_view{};
}

bool GuiWindowSessionController::IsVisible() const
{
    return visible_;
}

void GuiWindowSessionController::OpenWindow()
{
    if (!pluginInitialized_ || open_)
    {
        return;
    }
    open_ = true;
    RefreshData();
}

void GuiWindowSessionController::SetVisibilityMode(
    GuiWindowVisibilityMode mode
)
{
    hasVisibilityOverride_ =
        mode != GuiWindowVisibilityMode::Automatic;
    visibilityOverride_ = mode == GuiWindowVisibilityMode::Shown;
    UpdateVisibility();
}

void GuiWindowSessionController::CloseWindow()
{
    if (!open_)
    {
        return;
    }
    open_ = false;
    UpdateVisibility();
}

bool GuiWindowSessionController::DispatchPluginAction(
    const GuiActionContext& context
)
{
    if (!plugin_)
    {
        return false;
    }
    const bool handled = plugin_->HandleAction(context);
    if (handled)
    {
        RefreshData();
    }
    return handled;
}

const std::filesystem::path& GuiWindowSessionController::Root() const
{
    return root_;
}

IGuiPlugin& GuiWindowSessionController::Plugin() const
{
    return *plugin_;
}

const gui::GuiInterpreter&
GuiWindowSessionController::Interpreter() const
{
    return interpreter_;
}

GuiWindowRuntime& GuiWindowSessionController::Runtime()
{
    return windowRuntime_;
}

const GuiWindowRuntime& GuiWindowSessionController::Runtime() const
{
    return windowRuntime_;
}

const std::shared_ptr<GuiDataRegistry>&
GuiWindowSessionController::DataRegistry() const
{
    return dataRegistry_;
}

std::string_view GuiWindowSessionController::SessionId() const
{
    return sessionId_;
}

std::string_view GuiWindowSessionController::PersistenceKey() const
{
    return persistenceKey_;
}

std::string_view GuiWindowSessionController::PersistenceError() const
{
    return persistenceError_;
}

gui::GuiLayoutContext& GuiWindowSessionController::LayoutContext()
{
    return layoutContext_;
}

const gui::GuiLayoutContext&
GuiWindowSessionController::LayoutContext() const
{
    return layoutContext_;
}

GuiListRuntimeStore& GuiWindowSessionController::ListRuntimeStore()
{
    return listRuntimeStore_;
}

const GuiListRuntimeStore&
GuiWindowSessionController::ListRuntimeStore() const
{
    return listRuntimeStore_;
}

GuiRuntimeInputState& GuiWindowSessionController::InputState()
{
    return inputState_;
}

const GuiRuntimeInputState&
GuiWindowSessionController::InputState() const
{
    return inputState_;
}

gui::GuiCustomWidgetRegistry&
GuiWindowSessionController::CustomWidgets()
{
    return customWidgets_;
}

const gui::GuiCustomWidgetRegistry&
GuiWindowSessionController::CustomWidgets() const
{
    return customWidgets_;
}

const std::vector<std::string>&
GuiWindowSessionController::ListNames() const
{
    return listNames_;
}

const std::unordered_set<std::string>&
GuiWindowSessionController::ListTemplateNames() const
{
    return listTemplateNames_;
}

const GuiListModel* GuiWindowSessionController::FindListModel(
    std::string_view name
) const
{
    const auto found = listModels_.find(std::string(name));
    return found == listModels_.end() ? nullptr : &found->second;
}

GuiListRuntimeLayout
GuiWindowSessionController::BuildListRuntimeLayout(
    std::string_view listName
) const
{
    const GuiListModel* model = FindListModel(listName);
    const GuiListRuntimeState* runtime =
        listRuntimeStore_.Find(listName);
    static const GuiListModel emptyModel;
    static const GuiListRuntimeState emptyRuntime;
    return windowRuntime_.BuildListRuntimeLayout(
        listName,
        model ? *model : emptyModel,
        runtime ? *runtime : emptyRuntime,
        inputState_,
        layoutContext_
    );
}

std::vector<gui::GuiResolvedWidget>
GuiWindowSessionController::ResolveSceneWidgets() const
{
    const std::vector<gui::GuiResolvedWidget> baseWidgets =
        windowRuntime_.ResolveLayout(layoutContext_);
    std::vector<gui::GuiResolvedWidget> widgets;
    widgets.reserve(baseWidgets.size());
    std::size_t nextOrder = 0;
    for (const gui::GuiResolvedWidget& widget : baseWidgets)
    {
        nextOrder = std::max(nextOrder, widget.order + 1);
        if (widget.definition
            && listTemplateDefinitions_.find(widget.definition)
                != listTemplateDefinitions_.end())
        {
            continue;
        }
        widgets.push_back(widget);
    }

    for (const std::string& listName : listNames_)
    {
        const GuiListRuntimeLayout layout =
            BuildListRuntimeLayout(listName);
        const GuiListModel* model = FindListModel(listName);
        const gui::GuiResolvedWidget* listWidget = nullptr;
        for (const gui::GuiResolvedWidget& widget : baseWidgets)
        {
            if (widget.definition
                && widget.definition->type == gui::WidgetType::ListBox
                && widget.definition->name == listName)
            {
                listWidget = &widget;
                break;
            }
        }
        if (!model || !listWidget || !listWidget->visible)
        {
            continue;
        }

        gui::GuiRect listClip = layout.viewport;
        if (listWidget->hasClipRect
            && !IntersectRects(
                listClip,
                listWidget->clipRect,
                listClip
            ))
        {
            continue;
        }

        for (const GuiListItemRuntimeLayout& item : layout.items)
        {
            if (!item.definition
                || item.itemIndex >= model->items.size())
            {
                continue;
            }
            gui::GuiResolvedWidget root;
            root.definition = item.definition;
            root.rect = item.rect;
            root.rect.y -= layout.scrollOffset;
            root.visible = item.visible;
            root.enabled = item.enabled;
            root.opacity = std::clamp(
                 listWidget->opacity * item.definition->opacity,
                 0.0f,
                 1.0f
            );
			root.transform = item.definition->transform;
            root.depth = listWidget->depth + 1;
            root.zOrder = item.zOrder;
            root.order = nextOrder++;
            root.clipRect = listClip;
            root.hasClipRect = true;
            root.listName = listName;
            root.listIndex = static_cast<int>(item.itemIndex);
            root.listItemId = item.itemId;

            gui::GuiRect visibleItem;
            if (!root.visible
                || !IntersectRects(root.rect, listClip, visibleItem))
            {
                continue;
            }
            widgets.push_back(root);

            std::function<void(
                const gui::WidgetDefinition&,
                const gui::GuiResolvedWidget&
            )> appendChildren;
            appendChildren = [&](const gui::WidgetDefinition& parent,
                                 const gui::GuiResolvedWidget& resolvedParent)
            {
                for (const gui::WidgetDefinition& child : parent.children)
                {
                    gui::GuiResolvedWidget resolved;
                    resolved.definition = &child;
					resolved.rect = interpreter_.ResolveChildRect(
						resolvedParent.rect,
						child
					);
                    resolved.visible = resolvedParent.visible
                        && child.visible
                        && (child.visibleWhen.empty()
                            || !layoutContext_.conditionEvaluator
                            || layoutContext_.conditionEvaluator(
                                ReplaceItemId(
                                    child.visibleWhen,
                                    root.listItemId
                                )
                            ));
                    resolved.enabled = resolvedParent.enabled
                        && child.enabled
                        && (child.enabledWhen.empty()
                            || !layoutContext_.conditionEvaluator
                            || layoutContext_.conditionEvaluator(
                                ReplaceItemId(
                                    child.enabledWhen,
                                    root.listItemId
                                )
                            ));
                    resolved.opacity = std::clamp(
                             resolvedParent.opacity * child.opacity,
                             0.0f,
                             1.0f
                           );
					resolved.transform = child.transform;
                    resolved.depth = resolvedParent.depth + 1;
                    resolved.zOrder = resolvedParent.zOrder
                        + child.zOrder;
                    resolved.order = nextOrder++;
                    resolved.clipRect = resolvedParent.clipRect;
                    resolved.hasClipRect = resolvedParent.hasClipRect;
                    if (parent.clipChildren)
                    {
                        if (resolved.hasClipRect)
                        {
                            IntersectRects(
                                resolved.clipRect,
                                resolvedParent.rect,
                                resolved.clipRect
                            );
                        }
                        else
                        {
                            resolved.clipRect = resolvedParent.rect;
                            resolved.hasClipRect = true;
                        }
                    }
                    resolved.listName = listName;
                    resolved.listIndex = root.listIndex;
                    resolved.listItemId = root.listItemId;
                    if (resolved.visible)
                    {
                        widgets.push_back(resolved);
                    }
                    appendChildren(child, resolved);
                }
            };
            appendChildren(*item.definition, root);
        }
    }

	for (gui::GuiResolvedWidget& widget : widgets)
	{
		if (!widget.definition)
		{
			continue;
		}
		const gui::WidgetDefinition& definition = *widget.definition;
		widget.transform = definition.transform;
		if (!definition.rotationSource.empty())
		{
			widget.transform.rotationDegrees = static_cast<float>(
				ResolveWidgetNumber(
					widget,
					definition.rotationSource,
					widget.transform.rotationDegrees
				)
			);
		}
		if (!definition.transformScaleSource.empty())
		{
			const float scale = static_cast<float>(ResolveWidgetNumber(
				widget,
				definition.transformScaleSource,
				1.0
			));
			widget.transform.scaleX = scale;
			widget.transform.scaleY = scale;
		}
		if (!definition.transformScaleXSource.empty())
		{
			widget.transform.scaleX = static_cast<float>(ResolveWidgetNumber(
				widget,
				definition.transformScaleXSource,
				widget.transform.scaleX
			));
		}
		if (!definition.transformScaleYSource.empty())
		{
			widget.transform.scaleY = static_cast<float>(ResolveWidgetNumber(
				widget,
				definition.transformScaleYSource,
				widget.transform.scaleY
			));
		}
		widget.transform.scaleX = std::clamp(
			widget.transform.scaleX,
			0.001f,
			100.0f
		);
		widget.transform.scaleY = std::clamp(
			widget.transform.scaleY,
			0.001f,
			100.0f
		);
	}

    std::stable_sort(
        widgets.begin(),
        widgets.end(),
        [](const gui::GuiResolvedWidget& first,
           const gui::GuiResolvedWidget& second)
        {
            return first.zOrder != second.zOrder
                ? first.zOrder < second.zOrder
                : first.order < second.order;
        }
    );
    return widgets;
}

std::vector<gui::GuiResolvedWidget>
GuiWindowSessionController::ResolveInteractiveWidgets() const
{
    return ResolveSceneWidgets();
}

std::string GuiWindowSessionController::ResolveWidgetEffect(
	const gui::GuiResolvedWidget& widget
) const
{
	if (!widget.definition)
	{
		return {};
	}
	const gui::WidgetDefinition& definition = *widget.definition;
	if (definition.effectSource.empty())
	{
		return definition.effectResourceName;
	}

	constexpr std::string_view itemPrefix = "item.";
	if (!widget.listName.empty()
		&& definition.effectSource.rfind(itemPrefix, 0) == 0)
	{
		const GuiListModel* model = FindListModel(widget.listName);
		if (model
			&& widget.listIndex >= 0
			&& widget.listIndex < static_cast<int>(model->items.size()))
		{
			const GuiDataValue* value = model->items[widget.listIndex].Find(
				definition.effectSource.substr(itemPrefix.size())
			);
			if (value)
			{
				return GuiDataValueToText(*value);
			}
		}
		return definition.effectResourceName;
	}

	const std::string source = ReplaceItemId(
		definition.effectSource,
		widget.listItemId
	);
	if (layoutContext_.textResolver)
	{
		const std::string resolved = layoutContext_.textResolver(source);
		if (!resolved.empty())
		{
			return resolved;
		}
	}
	return definition.effectResourceName;
}

std::string GuiWindowSessionController::ResolveWidgetSprite(
    const gui::GuiResolvedWidget& widget,
    bool pressed
) const
{
    if (!widget.definition)
    {
        return {};
    }
    const gui::WidgetDefinition& definition = *widget.definition;
    std::string source = pressed
        ? definition.pressedSpriteSource
        : definition.spriteSource;
    std::string fallback = pressed
        ? definition.pressedSpriteName
        : definition.spriteName;
    if (pressed && source.empty() && fallback.empty())
    {
        source = definition.spriteSource;
        fallback = definition.spriteName;
    }

    const GuiListItem* item = nullptr;
    if (!widget.listName.empty())
    {
        const GuiListModel* model = FindListModel(widget.listName);
        if (model
            && widget.listIndex >= 0
            && widget.listIndex < static_cast<int>(model->items.size()))
        {
            item = &model->items[widget.listIndex];
        }
    }

    bool usedSource = !source.empty();
    constexpr std::string_view itemPrefix = "item.";
    if (item && source.rfind(itemPrefix, 0) == 0)
    {
        const GuiDataValue* value = item->Find(
            source.substr(itemPrefix.size())
        );
        source = value ? GuiDataValueToText(*value) : std::string{};
    }
    else if (!source.empty())
    {
        source = ReplaceItemId(source, widget.listItemId);
        if (layoutContext_.textResolver)
        {
            const std::string resolved =
                layoutContext_.textResolver(source);
            if (!resolved.empty())
            {
                source = resolved;
            }
        }
    }
    if (source.empty())
    {
        source = fallback;
        usedSource = false;
    }
    return usedSource && !definition.spriteValuePrefix.empty()
        ? definition.spriteValuePrefix + source
        : source;
}

double GuiWindowSessionController::ResolveWidgetNumber(
    const gui::GuiResolvedWidget& widget,
    std::string_view source,
    double fallback
) const
{
    if (source.empty())
    {
        return fallback;
    }
    constexpr std::string_view itemPrefix = "item.";
    if (!widget.listName.empty()
        && source.rfind(itemPrefix, 0) == 0)
    {
        const GuiListModel* model = FindListModel(widget.listName);
        if (!model
            || widget.listIndex < 0
            || widget.listIndex >= static_cast<int>(model->items.size()))
        {
            return fallback;
        }
        const GuiDataValue* value = model->items[widget.listIndex].Find(
            source.substr(itemPrefix.size())
        );
        return value ? GuiDataValueToNumber(*value) : fallback;
    }
    if (!layoutContext_.valueResolver)
    {
        return fallback;
    }
    const std::string resolvedSource = ReplaceItemId(
        std::string(source),
        widget.listItemId
    );
    if (dataRegistry_)
    {
        const GuiDataValue* value = dataRegistry_->Find(resolvedSource);
        return value ? GuiDataValueToNumber(*value) : fallback;
    }
    return layoutContext_.valueResolver(resolvedSource);
}

bool GuiWindowSessionController::ResolveWidgetText(
    const gui::GuiResolvedWidget& widget,
    gui::GuiTextCommand& command
) const
{
    if (!widget.definition)
    {
        return false;
    }
    const gui::WidgetDefinition& definition = *widget.definition;
    const GuiListItem* item = nullptr;
    if (!widget.listName.empty())
    {
        const GuiListModel* model = FindListModel(widget.listName);
        if (model
            && widget.listIndex >= 0
            && widget.listIndex < static_cast<int>(model->items.size()))
        {
            item = &model->items[widget.listIndex];
        }
    }

    std::string text = definition.text;
    constexpr std::string_view itemPrefix = "item.";
    if (item && definition.textSource.rfind(itemPrefix, 0) == 0)
    {
        const GuiDataValue* value = item->Find(
            definition.textSource.substr(itemPrefix.size())
        );
        text = value ? GuiDataValueToText(*value) : std::string{};
    }
    else if (!definition.textSource.empty()
        && layoutContext_.textResolver)
    {
        text = layoutContext_.textResolver(ReplaceItemId(
            definition.textSource,
            widget.listItemId
        ));
    }
    else if (item && text.empty())
    {
        text = item->text;
    }
    if (!definition.localizationKey.empty()
        && layoutContext_.localizationResolver)
    {
        text = layoutContext_.localizationResolver(
            ReplaceItemId(
                definition.localizationKey,
                widget.listItemId
            )
        );
    }
    else if (definition.localized
        && layoutContext_.localizationResolver)
    {
        text = layoutContext_.localizationResolver(text);
    }
    if (text.empty())
    {
        return false;
    }

    command = {};
    command.definition = &definition;
    command.rect = widget.rect;
    command.text = std::move(text);
    command.font = definition.font;
    const std::string alignment = Lower(definition.alignment);
    if (alignment == "center" || alignment == "centre")
    {
        command.alignment = gui::GuiTextAlignment::Center;
    }
    else if (alignment == "right")
    {
        command.alignment = gui::GuiTextAlignment::Right;
    }
    command.fontSize = definition.fontSize > 0
        ? definition.fontSize
        : std::max(12, widget.rect.height * 2 / 3);
    command.color[0] = definition.textColor[0];
    command.color[1] = definition.textColor[1];
    command.color[2] = definition.textColor[2];
    command.zOrder = widget.zOrder;
    command.lineSpacing = definition.lineSpacing;
    command.wrap = definition.wrap;
    return true;
}

bool GuiWindowSessionController::ResolveWidgetTooltip(
    const gui::GuiResolvedWidget& widget,
    gui::GuiTextCommand& command
) const
{
    if (!widget.definition
        || widget.definition->type == gui::WidgetType::MarkerLayer)
    {
        return false;
    }
    const gui::WidgetDefinition& definition = *widget.definition;
    const GuiListItem* item = nullptr;
    if (!widget.listName.empty())
    {
        const GuiListModel* model = FindListModel(widget.listName);
        if (model
            && widget.listIndex >= 0
            && widget.listIndex < static_cast<int>(model->items.size()))
        {
            item = &model->items[widget.listIndex];
        }
    }

    std::string text = definition.tooltip;
    constexpr std::string_view itemPrefix = "item.";
    if (item && definition.tooltipSource.rfind(itemPrefix, 0) == 0)
    {
        const GuiDataValue* value = item->Find(
            definition.tooltipSource.substr(itemPrefix.size())
        );
        const std::string resolved = value
            ? GuiDataValueToText(*value)
            : std::string{};
        if (!resolved.empty())
        {
            text = resolved;
        }
    }
    else if (!definition.tooltipSource.empty()
        && layoutContext_.textResolver)
    {
        const std::string resolved = layoutContext_.textResolver(
            ReplaceItemId(
                definition.tooltipSource,
                widget.listItemId
            )
        );
        if (!resolved.empty())
        {
            text = resolved;
        }
    }
    if (!definition.tooltipLocalizationKey.empty()
        && layoutContext_.localizationResolver)
    {
        text = layoutContext_.localizationResolver(
            ReplaceItemId(
                definition.tooltipLocalizationKey,
                widget.listItemId
            )
        );
    }
    else if (definition.localizeTooltip
        && layoutContext_.localizationResolver)
    {
        text = layoutContext_.localizationResolver(text);
    }
    if (text.empty()
        || definition.tooltipRect.width <= 0
        || definition.tooltipRect.height <= 0)
    {
        return false;
    }

    command = {};
    command.definition = &definition;
    command.rect.width = definition.tooltipRect.width;
    command.rect.height = definition.tooltipRect.height;
    command.text = std::move(text);
    command.font = definition.tooltipFont.empty()
        ? definition.font
        : definition.tooltipFont;
    command.fontSize = definition.tooltipFontSize > 0
        ? definition.tooltipFontSize
        : definition.fontSize;
    if (command.fontSize <= 0)
    {
        return false;
    }
    command.color[0] = definition.tooltipTextColor[0];
    command.color[1] = definition.tooltipTextColor[1];
    command.color[2] = definition.tooltipTextColor[2];
    command.zOrder = widget.zOrder;
    command.lineSpacing = definition.tooltipLineSpacing > 0
        ? definition.tooltipLineSpacing
        : definition.lineSpacing;
    command.wrap = definition.tooltipWrap;
    return true;
}

bool GuiWindowSessionController::IsWidgetPressed(
    const gui::GuiResolvedWidget& widget
) const
{
    if (inputState_.pressedKey.empty()
        || !inputState_.pressedSnapshot.definition)
    {
        return false;
    }
    const gui::GuiResolvedWidget& pressed = inputState_.pressedSnapshot;
    return pressed.definition == widget.definition
        && pressed.listName == widget.listName
        && pressed.listIndex == widget.listIndex;
}

std::size_t GuiWindowSessionController::DispatchEvents(
    const std::vector<GuiActionEvent>& events,
    int mouseX,
    int mouseY
)
{
    std::vector<GuiActionEvent> resolvedEvents = events;
    if (eventResolver_ && !resolvedEvents.empty())
    {
        eventResolver_(resolvedEvents);
    }
    for (GuiActionEvent& event : resolvedEvents)
    {
        if (!event.widget
            || event.widget->listName.empty()
            || event.widget->listIndex < 0)
        {
            continue;
        }
        const GuiListModel* model = FindListModel(
            event.widget->listName
        );
        if (!model
            || event.widget->listIndex >=
                static_cast<int>(model->items.size()))
        {
            continue;
        }
        const GuiListItem& item =
            model->items[event.widget->listIndex];
        for (const auto& field : item.fields)
        {
            event.parameters[field.first] =
                GuiDataValueToText(field.second);
        }
    }

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

std::size_t GuiWindowSessionController::DispatchMove(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    int mouseX,
    int mouseY
)
{
    return DispatchEvents(
        eventRouter_.ProcessMove(
            widgets,
            inputState_,
            mouseX,
            mouseY
        ),
        mouseX,
        mouseY
    );
}

std::size_t GuiWindowSessionController::DispatchDragMove(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    int mouseX,
    int mouseY
)
{
    return DispatchEvents(
        eventRouter_.ProcessDragMove(
            widgets,
            inputState_,
            mouseX,
            mouseY
        ),
        mouseX,
        mouseY
    );
}

std::size_t GuiWindowSessionController::DispatchPress(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    int mouseX,
    int mouseY
)
{
    return DispatchEvents(
        eventRouter_.ProcessPress(
            widgets,
            inputState_,
            mouseX,
            mouseY
        ),
        mouseX,
        mouseY
    );
}

std::size_t GuiWindowSessionController::DispatchRelease(
    const std::vector<gui::GuiResolvedWidget>& widgets,
    int mouseX,
    int mouseY
)
{
    return DispatchEvents(
        eventRouter_.ProcessRelease(
            widgets,
            inputState_,
            mouseX,
            mouseY
        ),
        mouseX,
        mouseY
    );
}

bool GuiWindowSessionController::ScrollListAt(
    int mouseX,
    int mouseY,
    int delta
)
{
    for (const std::string& listName : listNames_)
    {
        const GuiListRuntimeLayout layout =
            BuildListRuntimeLayout(listName);
        if (!PointInside(layout.viewport, mouseX, mouseY))
        {
            continue;
        }
        listRuntimeStore_.ScrollBy(
            listName,
            delta * layout.rowStep,
            layout.maximumScroll
        );
        if (dataChangedCallback_)
        {
            dataChangedCallback_();
        }
        return true;
    }
    return false;
}

bool GuiWindowSessionController::IsWindowDragRegion(
    int mouseX,
    int mouseY
) const
{
    const std::vector<gui::GuiResolvedWidget> widgets =
        ResolveInteractiveWidgets();
    const gui::GuiResolvedWidget* target = gui::HitTestGuiWidgets(
        widgets,
        mouseX,
        mouseY
    );
    if (target && target->definition)
    {
        const gui::WidgetDefinition& definition = *target->definition;
        const gui::GuiActionBinding& actions = definition.actions;
        if (definition.type == gui::WidgetType::Button
            || definition.type == gui::WidgetType::IndexedMap
            || customWidgets_.CanHandle(*target)
            || (definition.draggable
                && definition.type != gui::WidgetType::Window)
            || !actions.onClick.empty()
            || !actions.onPress.empty()
            || !actions.onRelease.empty())
        {
            return false;
        }
    }

    for (auto iterator = widgets.rbegin();
        iterator != widgets.rend();
        ++iterator)
    {
        if (!iterator->visible
            || !iterator->definition
            || iterator->definition->type != gui::WidgetType::Window
            || !iterator->definition->moveable
            || iterator->definition->dragHeight <= 0
            || !PointInside(iterator->rect, mouseX, mouseY))
        {
            continue;
        }
        return mouseY < iterator->rect.y
            + iterator->definition->dragHeight;
    }
    return false;
}

bool GuiWindowSessionController::PressTargetsCustomInput() const
{
    const gui::WidgetDefinition* definition =
        inputState_.pressedSnapshot.definition;
    return inputState_.pressedKey.empty()
        || (definition
            && (definition->type == gui::WidgetType::Custom
                || definition->type == gui::WidgetType::Window));
}

void GuiWindowSessionController::SetupActionBridge()
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
            const GuiListModel* model = FindListModel(listName);
            if (!model
                || listIndex < 0
                || listIndex >= static_cast<int>(model->items.size()))
            {
                return false;
            }
            itemId = model->items[listIndex].id;
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
            const bool dataChanged =
                optimisticDataStore_.ApplyAction(context);
            if (dataChanged)
            {
                handled = true;
            }
            if (dataChanged
                && ParseBoolean(FindParameter(context, "persist")))
            {
                const bool authoritativePersistence =
                    dataRegistry_->ResolveBool(
                        "state.persistenceavailable"
                    );
                if (!authoritativePersistence)
                {
                    const std::string target = FindParameter(
                        context,
                        "target"
                    );
                    if (!target.empty())
                    {
                        if (const GuiDataValue* value =
                            dataRegistry_->Find(target))
                        {
                            persistentValues_[Lower(target)] = *value;
                        }
                        if (const GuiListModel* list =
                            dataRegistry_->FindList(target))
                        {
                            persistentLists_[Lower(target)] = *list;
                        }
                    }
                    for (const auto& parameter : context.parameters)
                    {
                        constexpr std::string_view prefix = "set.";
                        if (parameter.first.rfind(prefix, 0) != 0)
                        {
                            continue;
                        }
                        const std::string name = parameter.first.substr(
                            prefix.size()
                        );
                        if (const GuiDataValue* value =
                            dataRegistry_->Find(name))
                        {
                            persistentValues_[Lower(name)] = *value;
                        }
                    }
                }
                else
                {
                    persistentValues_.clear();
                    persistentLists_.clear();
                }
                persistentStateDirty_ = true;
                SavePersistentState();
            }
            if (applicationActionInvoker_
                && applicationActionInvoker_(id_, context))
            {
                return true;
            }
            return (plugin_ && plugin_->HandleAction(context))
                || handled;
        }
    );
}

void GuiWindowSessionController::ApplySessionBoundary(
    const std::shared_ptr<GuiDataRegistry>& registry
)
{
    if (!registry)
    {
        return;
    }
    const std::string nextSessionId = registry->ResolveText(
        "state.sessionid"
    );
    bool sessionChanged = false;
    std::string previousSessionId;
    if (!nextSessionId.empty())
    {
        if (!sessionObserved_)
        {
            sessionObserved_ = true;
            sessionId_ = nextSessionId;
        }
        else if (nextSessionId != sessionId_)
        {
            previousSessionId = sessionId_;
            sessionId_ = nextSessionId;
            sessionChanged = true;
        }
    }

    const std::string nextPersistenceKey = registry->ResolveText(
        "state.persistencekey"
    );
    const bool persistenceChanged =
        nextPersistenceKey != persistenceKey_;
    if (sessionChanged || persistenceChanged)
    {
        SavePersistentState();
        ResetTransientState();
        persistenceKey_ = nextPersistenceKey;
        LoadPersistentState();
    }
    else if (!persistenceKey_.empty()
        && persistenceStore_
        && !persistenceLoaded_)
    {
        LoadPersistentState();
    }

    if (sessionChanged && sessionChangedCallback_)
    {
        sessionChangedCallback_(previousSessionId, sessionId_);
    }
}

void GuiWindowSessionController::LoadPersistentState()
{
    persistenceLoaded_ = true;
    persistentStateDirty_ = false;
    persistenceError_.clear();
    if (!persistenceStore_ || persistenceKey_.empty())
    {
        return;
    }
    GuiPersistentState state;
    if (!persistenceStore_->Load(
            persistenceKey_,
            id_,
            state,
            persistenceError_
        ))
    {
        return;
    }
    if (!dataRegistry_
        || !dataRegistry_->ResolveBool("state.persistenceavailable"))
    {
        persistentValues_ = std::move(state.values);
        persistentLists_ = std::move(state.lists);
    }
    for (const auto& entry : state.listRuntime)
    {
        GuiListRuntimeState& runtime = listRuntimeStore_.Get(entry.first);
        runtime.scrollOffset = entry.second.scrollOffset;
        runtime.selectedItemId = entry.second.selectedItemId;
    }
}

void GuiWindowSessionController::SavePersistentState()
{
    if (!persistentStateDirty_
        || !persistenceStore_
        || persistenceKey_.empty())
    {
        return;
    }
    GuiPersistentState state;
    state.values = persistentValues_;
    state.lists = persistentLists_;
    for (const std::string& listName : listNames_)
    {
        if (const GuiListRuntimeState* runtime =
            listRuntimeStore_.Find(listName))
        {
            state.listRuntime[listName] = {
                runtime->scrollOffset,
                runtime->selectedItemId
            };
        }
    }
    if (persistenceStore_->Save(
            persistenceKey_,
            id_,
            state,
            persistenceError_
        ))
    {
        persistentStateDirty_ = false;
    }
}

void GuiWindowSessionController::ResetTransientState()
{
    persistentValues_.clear();
    persistentLists_.clear();
    persistentStateDirty_ = false;
    listModels_.clear();
    listRuntimeStore_.Clear();
    inputState_ = {};
    hasVisibilityOverride_ = false;
    visibilityOverride_ = false;
    open_ = initiallyOpen_;
}

void GuiWindowSessionController::UpdateVisibility()
{
    const bool conditionVisible = visibleWhen_.empty()
        || (dataRegistry_
            && dataRegistry_->EvaluateCondition(visibleWhen_));
    const bool shouldBeVisible = open_
        && (hasVisibilityOverride_
            ? visibilityOverride_
            : conditionVisible);
    if (shouldBeVisible == visible_)
    {
        return;
    }
    visible_ = shouldBeVisible;
    if (visibilityChangedCallback_)
    {
        visibilityChangedCallback_(visible_);
    }
}
