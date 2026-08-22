#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core_module.h"

namespace core
{

class ModuleRegistry
{
public:
    bool Register(
        std::unique_ptr<IModule> module,
        std::string& error
    );

    bool InitializeAll(Services& services, std::string& error);
    void DispatchLifecycle(const LifecycleEvent& event);
    void Tick(uint64_t nowMilliseconds);
    void ShutdownAll();

    IModule* Find(std::string_view id) const;
    bool IsInitialized() const;
    std::vector<std::string> ModuleIds() const;

private:
    struct Entry
    {
        std::unique_ptr<IModule> module;
        bool initialized = false;
    };

    void Diagnose(std::string_view message) const;

    std::vector<Entry> entries_;
    DiagnosticSink diagnostic_;
    bool initialized_ = false;
};

}
