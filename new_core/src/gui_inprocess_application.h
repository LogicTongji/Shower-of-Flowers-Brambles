#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "gui_behavior.h"
#include "gui_data_bridge.h"
#include "gui_data_provider.h"
#include "gui_interpreter.h"
#include "gui_plugin.h"
#include "gui_plugin_registry.h"
#include "gui_persistence.h"

struct GuiConfigurationIssue
{
    std::string pluginId;
    std::string stage;
    std::string message;
};

class GuiInProcessApplication
{
public:
    ~GuiInProcessApplication();

    bool Initialize(
        const std::filesystem::path& root,
        std::string& error
    );

    void Shutdown();

    bool IsInitialized() const;
    const std::filesystem::path& Root() const;
    const gui::GuiInterpreter& Interpreter() const;
    const GuiBehaviorRegistry* Behaviors() const;
    const std::vector<GuiPluginLaunch>& Launches() const;
    const std::vector<GuiConfigurationIssue>& Issues() const;
    const std::shared_ptr<GuiPersistenceStore>& PersistenceStore() const;

private:
    std::filesystem::path root_;
    GuiDataBridgeChannelRegistry bridgeChannels_;
    GuiDataProviderRegistry dataProviders_;
    GuiPluginRegistry pluginRegistry_;
    gui::GuiInterpreter interpreter_;
    GuiBehaviorRegistry behaviors_;
    std::vector<std::unique_ptr<IGuiPlugin>> plugins_;
    std::vector<GuiPluginLaunch> launches_;
    std::vector<GuiConfigurationIssue> issues_;
    std::shared_ptr<GuiPersistenceStore> persistenceStore_;
    bool behaviorsLoaded_ = false;
    bool initialized_ = false;
};
