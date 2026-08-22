#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gui_plugin_manifest.h"
#include "gui_plugin_registry.h"

namespace
{

class ProbePlugin final : public IGuiPlugin
{
public:
    ProbePlugin(
        std::string windowName,
        std::string windowTitle,
        std::string providerType,
        std::string dataPath
    )
        : windowName_(std::move(windowName)),
          windowTitle_(std::move(windowTitle)),
          providerType_(std::move(providerType)),
          dataPath_(std::move(dataPath))
    {
    }

    std::string_view WindowName() const override
    {
        return windowName_;
    }

    std::string_view WindowTitle() const override
    {
        return windowTitle_;
    }

    bool Initialize(const GuiPluginInitContext&, std::string&) override
    {
        return true;
    }

    void Shutdown() override
    {
    }

    void RegisterCustomWidgets(gui::GuiCustomWidgetRegistry&) override
    {
    }

    std::shared_ptr<GuiDataRegistry> BuildDataRegistry() const override
    {
        return std::make_shared<GuiDataRegistry>();
    }

    bool Tick(uint64_t) override
    {
        return false;
    }

    bool HandleAction(const GuiActionContext&) override
    {
        return false;
    }

    const std::string& ProviderType() const
    {
        return providerType_;
    }

    const std::string& DataPath() const
    {
        return dataPath_;
    }

private:
    std::string windowName_;
    std::string windowTitle_;
    std::string providerType_;
    std::string dataPath_;
};

}

int main()
{
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path()
        / ("gui_plugin_manifest_probe_"
            + std::to_string(
                std::chrono::steady_clock::now()
                    .time_since_epoch()
                    .count()
            ));
    const fs::path manifestRoot = root / "interface" / "gui_plugins";
    std::error_code fileError;
    fs::create_directories(manifestRoot, fileError);
    if (fileError)
    {
        std::cerr << "Failed to create manifest probe directory\n";
        return 1;
    }

    {
        std::ofstream output(manifestRoot / "probe.txt");
        output
            << "guiPlugins = {\n"
            << "  guiPlugin = {\n"
            << "    id = \"probe_plugin\"\n"
            << "    displayName = \"Probe Plugin\"\n"
            << "    factory = \"declarative_gui\"\n"
            << "    startup = yes\n"
            << "    windowZOrder = 40\n"
            << "    modal = yes\n"
            << "    maxViewportWidthRatio = 0.85\n"
            << "    maxViewportHeightRatio = 0.80\n"
            << "    cascadeOffsetX = 14\n"
            << "    cascadeOffsetY = 18\n"
            << "    visibleWhen = \"state.visible\"\n"
            << "    options = {\n"
            << "      window = \"probe_window\"\n"
            << "      title = \"Probe Window\"\n"
            << "      data_provider = \"sequence\"\n"
            << "      data = \"probe_data\"\n"
            << "    }\n"
            << "  }\n"
            << "}\n";
    }
    {
        std::ofstream output(manifestRoot / "broken.txt");
        output << "guiPlugins = { guiPlugin = { id = \"broken\"\n";
    }

    GuiPluginRegistry registry;
    if (!registry.RegisterFactory(
            "declarative_gui",
            [](const GuiPluginCreateContext& context)
            {
                return std::make_unique<ProbePlugin>(
                    context.Option("window"),
                    context.Option("title"),
                    context.Option("data_provider"),
                    context.Option("data")
                );
            }
        ))
    {
        std::cerr << "Failed to register probe factory\n";
        fs::remove_all(root, fileError);
        return 1;
    }

    std::size_t loadedCount = 0;
    std::string error;
    std::vector<std::string> diagnostics;
    if (!LoadGuiPluginManifestDirectory(
            manifestRoot,
            registry,
            loadedCount,
            error,
            &diagnostics
        ))
    {
        std::cerr << error << '\n';
        fs::remove_all(root, fileError);
        return 1;
    }

    GuiPluginCreateContext context;
    context.root = root;
    context.options["DATA"] = "override_data";
    std::unique_ptr<IGuiPlugin> plugin = registry.Create(
        "PROBE_PLUGIN",
        context
    );
    ProbePlugin* probe = dynamic_cast<ProbePlugin*>(plugin.get());
    const GuiPluginDescriptor* descriptor = registry.Find(
        "probe_plugin"
    );
    const bool valid = probe
        && descriptor
        && probe->WindowName() == "probe_window"
        && probe->WindowTitle() == "Probe Window"
        && probe->ProviderType() == "sequence"
        && probe->DataPath() == "override_data"
        && loadedCount == 1
        && registry.DefaultPluginId() == "probe_plugin"
        && descriptor->startup
        && descriptor->visibleWhen == "state.visible"
        && descriptor->windowZOrder == 40
        && descriptor->modal
        && descriptor->maxViewportWidthRatio == 0.85
        && descriptor->maxViewportHeightRatio == 0.80
        && descriptor->cascadeOffsetX == 14
        && descriptor->cascadeOffsetY == 18
        && !diagnostics.empty();

    if (!valid)
    {
        std::cerr << "Plugin manifest probe failed\n";
        fs::remove_all(root, fileError);
        return 1;
    }

    std::cout
        << "Loaded plugin manifests: " << loadedCount << '\n'
        << "Plugin: " << registry.DefaultPluginId() << '\n'
        << "Window: " << probe->WindowName() << '\n'
        << "Provider: " << probe->ProviderType() << '\n'
        << "Runtime data override: " << probe->DataPath() << '\n';
    fs::remove_all(root, fileError);
    return 0;
}
