#pragma once

#include <string>
#include <string_view>

#include "core_module.h"

class LeaderCaptureCoreModule final : public core::IModule
{
public:
    std::string_view Id() const override;
    int Priority() const override;

    bool Initialize(
        core::Services& services,
        std::string& error
    ) override;
    void OnLifecycleEvent(
        const core::LifecycleEvent& event
    ) override;
    void Tick(uint64_t nowMilliseconds) override;
    void Shutdown() override;

private:
    core::DiagnosticSink diagnostic_;
    std::string lastHookError_;
    bool initialized_ = false;
};
