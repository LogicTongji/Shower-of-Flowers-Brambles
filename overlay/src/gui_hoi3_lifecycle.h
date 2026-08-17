#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

enum class GuiHoi3LifecycleProbeStatus
{
    UnsupportedExecutable,
    Unavailable,
    Frontend,
    Gameplay
};

struct GuiHoi3LifecycleProbeResult
{
    GuiHoi3LifecycleProbeStatus status =
        GuiHoi3LifecycleProbeStatus::Unavailable;
    std::string playerTag;
};

bool DecodeGuiHoi3PlayerTag(
    const uint8_t* bytes,
    std::size_t size,
    std::string& playerTag
);

GuiHoi3LifecycleProbeResult ProbeGuiHoi3Lifecycle();
