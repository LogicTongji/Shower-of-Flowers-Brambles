#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "gui_sequence_data_provider.h"

int main()
{
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path()
        / ("gui_sequence_data_provider_probe_"
            + std::to_string(
                std::chrono::steady_clock::now()
                    .time_since_epoch()
                    .count()
            ));
    const fs::path frames = root / "frames";
    std::error_code fileError;
    fs::create_directories(frames, fileError);
    if (fileError)
    {
        std::cerr << "Failed to create sequence probe directory\n";
        return 1;
    }

    {
        std::ofstream output(root / "common.txt");
        output
            << "guiData = {\n"
            << "  selected.id = 0\n"
            << "  items.3.name = \"third item\"\n"
            << "}\n";
    }
    {
        std::ofstream output(frames / "01.txt");
        output
            << "guiData = {\n"
            << "  frame = 1\n"
            << "  items.3.value = 15\n"
            << "}\n";
    }
    {
        std::ofstream output(frames / "02.txt");
        output
            << "guiData = {\n"
            << "  frame = 2\n"
            << "  items.3.value = 65\n"
            << "}\n";
    }

    GuiDataProviderRegistry providers;
    if (!RegisterGuiSequenceDataProvider(providers))
    {
        std::cerr << "Failed to register sequence provider\n";
        fs::remove_all(root, fileError);
        return 1;
    }

    GuiDataProviderCreateContext createContext;
    createContext.options["data"] = frames.string();
    createContext.options["base_data"] =
        (root / "common.txt").string();
    createContext.options["frame_interval"] = "100";
    std::unique_ptr<IGuiDataProvider> provider = providers.Create(
        "sequence",
        createContext
    );
    std::string error;
    if (!provider
        || !provider->Initialize(
            GuiDataProviderInitContext{root},
            error
        ))
    {
        std::cerr << error << '\n';
        fs::remove_all(root, fileError);
        return 1;
    }

    std::shared_ptr<GuiDataRegistry> data = provider->Registry();
    if (data->ResolveNumber("frame") != 1.0
        || data->ResolveText("items.3.name") != "third item"
        || data->ResolveNumber("items.3.value") != 15.0)
    {
        std::cerr << "Sequence provider base merge failed\n";
        fs::remove_all(root, fileError);
        return 1;
    }

    GuiActionContext selection;
    selection.fallbackOperation = "select_item";
    selection.parameters["target"] = "selected.id";
    selection.parameters["persist"] = "yes";
    selection.listItemId = 3;
    selection.hasListItemId = true;
    if (provider->HandleAction(selection, error)
            != GuiDataProviderActionResult::Handled
        || provider->Registry()->ResolveText(
            "items.{selected.id}.name"
        ) != "third item")
    {
        std::cerr << "Sequence provider persistent selection failed\n";
        fs::remove_all(root, fileError);
        return 1;
    }

    if (provider->Tick(1000, error)
            != GuiDataProviderUpdateResult::Unchanged
        || provider->Tick(1100, error)
            != GuiDataProviderUpdateResult::Changed
        || provider->Registry()->ResolveNumber("frame") != 2.0
        || provider->Registry()->ResolveNumber("selected.id") != 3.0
        || provider->Registry()->ResolveNumber(
            "items.{selected.id}.value"
        ) != 65.0)
    {
        std::cerr << "Sequence provider frame advance failed: "
                  << error << '\n';
        fs::remove_all(root, fileError);
        return 1;
    }

    std::cout
        << "Sequence frame: "
        << provider->Registry()->ResolveNumber("frame") << '\n'
        << "Persistent selection: "
        << provider->Registry()->ResolveNumber("selected.id") << '\n'
        << "Dynamic selected value: "
        << provider->Registry()->ResolveNumber(
            "items.{selected.id}.value"
        ) << '\n';

    provider->Shutdown();
    fs::remove_all(root, fileError);

    return 0;
}
