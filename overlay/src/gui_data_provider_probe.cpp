#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "gui_data_provider.h"

namespace
{

class MockGuiDataProvider final : public IGuiDataProvider
{
public:
    explicit MockGuiDataProvider(int64_t initialValue)
        : initialValue_(initialValue),
          registry_(std::make_shared<GuiDataRegistry>())
    {
    }

    std::string_view Type() const override
    {
        return "mock";
    }

    bool Initialize(
        const GuiDataProviderInitContext&,
        std::string&
    ) override
    {
        registry_->Set("provider.ready", true);
        registry_->Set("counter", initialValue_);
        initialized_ = true;
        return true;
    }

    void Shutdown() override
    {
        initialized_ = false;
        registry_->Clear();
    }

    std::shared_ptr<GuiDataRegistry> Registry() const override
    {
        return registry_;
    }

    GuiDataProviderUpdateResult Tick(
        uint64_t,
        std::string&
    ) override
    {
        if (!initialized_ || ticked_)
        {
            return GuiDataProviderUpdateResult::Unchanged;
        }
        ticked_ = true;
        registry_->Set("counter", initialValue_ + 1);
        return GuiDataProviderUpdateResult::Changed;
    }

    GuiDataProviderActionResult HandleAction(
        const GuiActionContext& context,
        std::string& error
    ) override
    {
        if (context.fallbackOperation == "mock_increment")
        {
            registry_->Set(
                "counter",
                static_cast<int64_t>(
                    registry_->ResolveNumber("counter") + 5.0
                )
            );
            return GuiDataProviderActionResult::Handled;
        }
        if (context.fallbackOperation == "mock_failure")
        {
            error = "expected_mock_failure";
            return GuiDataProviderActionResult::Failed;
        }
        return GuiDataProviderActionResult::Unhandled;
    }

private:
    int64_t initialValue_ = 0;
    std::shared_ptr<GuiDataRegistry> registry_;
    bool initialized_ = false;
    bool ticked_ = false;
};

}

int main()
{
    GuiDataProviderRegistry providers;
    if (!providers.RegisterFactory(
            "mock",
            [](const GuiDataProviderCreateContext& context)
            {
                int64_t initialValue = 0;
                const std::string initial = context.Option("initial");
                if (!initial.empty())
                {
                    initialValue = std::stoll(initial);
                }
                return std::make_unique<MockGuiDataProvider>(
                    initialValue
                );
            }
        )
        || providers.RegisterFactory("MOCK", {})
        || !providers.HasFactory(" Mock "))
    {
        std::cerr << "GUI data provider registration failed\n";
        return 1;
    }

    GuiDataProviderCreateContext createContext;
    createContext.options["INITIAL"] = "6";
    std::unique_ptr<IGuiDataProvider> provider = providers.Create(
        " MOCK ",
        createContext
    );
    if (!provider
        || providers.Create("missing", createContext))
    {
        std::cerr << "GUI data provider creation failed\n";
        return 1;
    }

    std::string error;
    const std::filesystem::path root =
        std::filesystem::current_path();
    if (!provider->Initialize(
            GuiDataProviderInitContext{root},
            error
        )
        || !provider->Registry()->ResolveBool("provider.ready")
        || provider->Registry()->ResolveNumber("counter") != 6.0)
    {
        std::cerr << "Mock GUI data provider initialization failed\n";
        return 1;
    }

    if (provider->Tick(100, error)
            != GuiDataProviderUpdateResult::Changed
        || provider->Tick(200, error)
            != GuiDataProviderUpdateResult::Unchanged
        || provider->Registry()->ResolveNumber("counter") != 7.0)
    {
        std::cerr << "Mock GUI data provider tick failed\n";
        return 1;
    }

    GuiActionContext action;
    action.fallbackOperation = "mock_increment";
    if (provider->HandleAction(action, error)
            != GuiDataProviderActionResult::Handled
        || provider->Registry()->ResolveNumber("counter") != 12.0)
    {
        std::cerr << "Mock GUI data provider action failed\n";
        return 1;
    }

    action.fallbackOperation = "mock_failure";
    if (provider->HandleAction(action, error)
            != GuiDataProviderActionResult::Failed
        || error != "expected_mock_failure")
    {
        std::cerr << "Mock GUI data provider failure reporting failed\n";
        return 1;
    }

    std::cout
        << "Data provider type: " << provider->Type() << '\n'
        << "Data provider counter: "
        << provider->Registry()->ResolveNumber("counter") << '\n'
        << "Data provider error: " << error << '\n';

    provider->Shutdown();
    return 0;
}
