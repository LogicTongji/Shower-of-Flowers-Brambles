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

bool PointInside(const gui::GuiRect& rect, int x, int y)
{
    return x >= rect.x
        && y >= rect.y
        && x < rect.x + rect.width
        && y < rect.y + rect.height;
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
    if (const gui::WindowDefinition* definition =
            windowRuntime_.Definition())
    {
        CollectListDefinitions(
            *definition,
            listNames_,
            listTemplateNames_
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
    open_ = true;
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
}

bool GuiWindowSessionController::Tick(uint64_t nowMilliseconds)
{
    if (!open_)
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
    for (const auto& entry : persistentValues_)
    {
        dataRegistry_->Set(entry.first, entry.second);
    }
    for (const auto& entry : persistentLists_)
    {
        dataRegistry_->SetList(entry.first, entry.second);
    }
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
    open_ = false;
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
GuiWindowSessionController::ResolveInteractiveWidgets() const
{
    std::vector<gui::GuiResolvedWidget> widgets =
        windowRuntime_.ResolveLayout(layoutContext_);
    for (const std::string& listName : listNames_)
    {
        const GuiListRuntimeLayout layout =
            BuildListRuntimeLayout(listName);
        for (const GuiListItemRuntimeLayout& item : layout.items)
        {
            gui::GuiResolvedWidget widget;
            widget.definition = item.definition;
            widget.rect = item.rect;
            widget.rect.y -= layout.scrollOffset;
            widget.visible = item.visible;
            widget.enabled = item.enabled;
            widget.zOrder = item.definition
                ? item.definition->zOrder : 0;
            widget.order = item.itemIndex;
            widget.listName = listName;
            widget.listIndex = static_cast<int>(item.itemIndex);
            widgets.push_back(std::move(widget));
        }
    }
    return widgets;
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
            || definition.type == gui::WidgetType::MarkerLayer
            || definition.type == gui::WidgetType::Custom
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

void GuiWindowSessionController::UpdateVisibility()
{
    const bool conditionVisible = visibleWhen_.empty()
        || (dataRegistry_
            && dataRegistry_->EvaluateCondition(visibleWhen_));
    const bool shouldBeVisible = hasVisibilityOverride_
        ? visibilityOverride_
        : conditionVisible;
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
