#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "core_hook_registry.h"
#include "core_lifecycle.h"

namespace core
{

using DiagnosticSink = std::function<void(std::string_view)>;

struct Services
{
    HookRegistry& hooks;
    LifecycleService& lifecycle;
    DiagnosticSink diagnostic;
};

class IModule
{
public:
    virtual ~IModule() = default;

    virtual std::string_view Id() const = 0;
    virtual int Priority() const
    {
        return 0;
    }

    virtual bool Initialize(
        Services& services,
        std::string& error
    ) = 0;

    virtual void OnLifecycleEvent(
        const LifecycleEvent& event
    ) = 0;

    virtual void Tick(uint64_t nowMilliseconds) = 0;
    virtual void Shutdown() = 0;
};

}
