#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "gui_application_bus.h"
#include "gui_behavior.h"
#include "gui_declarative_data.h"
#include "gui_plugin.h"
#include "gui_persistence.h"
#include "gui_tick.h"

class GuiWindowSessionController final : public IGuiApplicationEndpoint
{
public:
    using ApplicationActionInvoker = std::function<bool(
        std::string_view,
        const GuiActionContext&
    )>;
    using LocalizationResolver = std::function<std::string(
        std::string_view
    )>;
    using DataChangedCallback = std::function<void()>;
    using SessionChangedCallback = std::function<void(
        std::string_view,
        std::string_view
    )>;
    using VisibilityChangedCallback = std::function<void(bool)>;
    using EventResolver = std::function<void(
        std::vector<GuiActionEvent>&
    )>;

    GuiWindowSessionController(
        std::filesystem::path root,
        const GuiPluginLaunch& launch,
        const gui::GuiInterpreter& interpreter,
        const GuiBehaviorRegistry* behaviorRegistry
    );

    ~GuiWindowSessionController();

    void SetApplicationActionInvoker(
        ApplicationActionInvoker invoker
    );
    void SetLocalizationResolver(
        LocalizationResolver resolver
    );
    void SetDataChangedCallback(
        DataChangedCallback callback
    );
    void SetSessionChangedCallback(
        SessionChangedCallback callback
    );
    void SetPersistenceStore(
        std::shared_ptr<GuiPersistenceStore> store
    );
    void SetVisibilityChangedCallback(
        VisibilityChangedCallback callback
    );
    void SetEventResolver(EventResolver resolver);

    bool Bind(std::string& error);
    bool Initialize(void* graphicsContext, std::string& error);
    void Shutdown();

    bool Tick(uint64_t nowMilliseconds);
    void RefreshData();

    bool IsOpen() const override;
    bool HasVisibilityCondition() const;

    std::string_view PluginId() const override;
    std::string_view WindowName() const override;
    bool IsVisible() const override;
    void OpenWindow() override;
    void SetVisibilityMode(GuiWindowVisibilityMode mode) override;
    void CloseWindow() override;
    bool DispatchPluginAction(
        const GuiActionContext& context
    ) override;

    const std::filesystem::path& Root() const;
    IGuiPlugin& Plugin() const;
    const gui::GuiInterpreter& Interpreter() const;
    GuiWindowRuntime& Runtime();
    const GuiWindowRuntime& Runtime() const;
    const std::shared_ptr<GuiDataRegistry>& DataRegistry() const;
    std::string_view SessionId() const;
    std::string_view PersistenceKey() const;
    std::string_view PersistenceError() const;
    gui::GuiLayoutContext& LayoutContext();
    const gui::GuiLayoutContext& LayoutContext() const;
    GuiListRuntimeStore& ListRuntimeStore();
    const GuiListRuntimeStore& ListRuntimeStore() const;
    GuiRuntimeInputState& InputState();
    const GuiRuntimeInputState& InputState() const;
    gui::GuiCustomWidgetRegistry& CustomWidgets();
    const gui::GuiCustomWidgetRegistry& CustomWidgets() const;
    const std::vector<std::string>& ListNames() const;
    const std::unordered_set<std::string>& ListTemplateNames() const;
    const GuiListModel* FindListModel(std::string_view name) const;

    GuiListRuntimeLayout BuildListRuntimeLayout(
        std::string_view listName
    ) const;
    std::vector<gui::GuiResolvedWidget> ResolveSceneWidgets() const;
    std::vector<gui::GuiResolvedWidget> ResolveInteractiveWidgets() const;
    std::string ResolveWidgetSprite(
        const gui::GuiResolvedWidget& widget,
        bool pressed = false
    ) const;
	std::string ResolveWidgetEffect(
		const gui::GuiResolvedWidget& widget
	) const;
    double ResolveWidgetNumber(
        const gui::GuiResolvedWidget& widget,
        std::string_view source,
        double fallback = 0.0
    ) const;
    bool ResolveWidgetText(
        const gui::GuiResolvedWidget& widget,
        gui::GuiTextCommand& command
    ) const;
    bool ResolveWidgetTooltip(
        const gui::GuiResolvedWidget& widget,
        gui::GuiTextCommand& command
    ) const;
    bool IsWidgetPressed(
        const gui::GuiResolvedWidget& widget
    ) const;

    std::size_t DispatchEvents(
        const std::vector<GuiActionEvent>& events,
        int mouseX,
        int mouseY
    );
    std::size_t DispatchMove(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        int mouseX,
        int mouseY
    );
    std::size_t DispatchDragMove(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        int mouseX,
        int mouseY
    );
    std::size_t DispatchPress(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        int mouseX,
        int mouseY
    );
    std::size_t DispatchRelease(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        int mouseX,
        int mouseY
    );

    bool ScrollListAt(int mouseX, int mouseY, int delta);
    bool IsWindowDragRegion(int mouseX, int mouseY) const;
    bool PressTargetsCustomInput() const;

private:
    void SetupActionBridge();
    void ApplySessionBoundary(
        const std::shared_ptr<GuiDataRegistry>& registry
    );
    void LoadPersistentState();
    void SavePersistentState();
    void ResetTransientState();
    void UpdateVisibility();

    std::filesystem::path root_;
    std::string id_;
    std::string visibleWhen_;
    bool initiallyOpen_ = true;
    IGuiPlugin* plugin_ = nullptr;
    const gui::GuiInterpreter& interpreter_;
    const GuiBehaviorRegistry* behaviorRegistry_ = nullptr;
    ApplicationActionInvoker applicationActionInvoker_;
    LocalizationResolver localizationResolver_;
    DataChangedCallback dataChangedCallback_;
    SessionChangedCallback sessionChangedCallback_;
    std::shared_ptr<GuiPersistenceStore> persistenceStore_;
    VisibilityChangedCallback visibilityChangedCallback_;
    EventResolver eventResolver_;
    GuiWindowRuntime windowRuntime_;
    std::shared_ptr<GuiDataRegistry> dataRegistry_;
    gui::GuiLayoutContext layoutContext_;
    std::vector<std::string> listNames_;
    std::unordered_set<std::string> listTemplateNames_;
    std::unordered_set<
        const gui::WidgetDefinition*
    > listTemplateDefinitions_;
    std::unordered_map<std::string, GuiListModel> listModels_;
    GuiListRuntimeStore listRuntimeStore_;
    GuiEventRouter eventRouter_;
    GuiRuntimeInputState inputState_;
    GuiLuaActionBridge actionBridge_;
    GuiDeclarativeDataStore optimisticDataStore_;
    std::unordered_map<std::string, GuiDataValue> persistentValues_;
    std::unordered_map<std::string, GuiListModel> persistentLists_;
    gui::GuiCustomWidgetRegistry customWidgets_;
    GuiTickScheduler tickScheduler_;
    bool pluginInitialized_ = false;
    bool open_ = false;
    bool visible_ = false;
    bool hasVisibilityOverride_ = false;
    bool visibilityOverride_ = false;
    bool sessionObserved_ = false;
    std::string sessionId_;
    std::string persistenceKey_;
    std::string persistenceError_;
    bool persistenceLoaded_ = false;
    bool persistentStateDirty_ = false;
};
