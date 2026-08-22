#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

enum class Hoi3LifecycleProbeStatus
{
    UnsupportedExecutable,
    Unavailable,
    Frontend,
    Gameplay
};

struct Hoi3LifecycleProbeResult
{
    Hoi3LifecycleProbeStatus status =
        Hoi3LifecycleProbeStatus::Unavailable;
    std::string playerTag;
};

bool DecodeHoi3PlayerTag(
    const uint8_t* bytes,
    std::size_t size,
    std::string& playerTag
);

Hoi3LifecycleProbeResult ProbeHoi3Lifecycle();
