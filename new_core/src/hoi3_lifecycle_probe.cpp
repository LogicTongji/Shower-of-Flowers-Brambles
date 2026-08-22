#include <array>
#include <cstdint>
#include <iostream>
#include <string>

#include "hoi3_lifecycle.h"

int main()
{
    std::string tag;
    const std::array<uint8_t, 4> frontend{'-', '-', '-', 0};
    const std::array<uint8_t, 4> gameplay{'C', 'H', 'I', 0};
    const std::array<uint8_t, 4> numeric{'U', '0', '1', 0};
    const std::array<uint8_t, 4> invalid{'C', 'h', 'I', 0};
    if (!DecodeHoi3PlayerTag(
            frontend.data(),
            frontend.size(),
            tag
        )
        || tag != "---"
        || !DecodeHoi3PlayerTag(
            gameplay.data(),
            gameplay.size(),
            tag
        )
        || tag != "CHI"
        || !DecodeHoi3PlayerTag(
            numeric.data(),
            numeric.size(),
            tag
        )
        || tag != "U01"
        || DecodeHoi3PlayerTag(
            invalid.data(),
            invalid.size(),
            tag
        ))
    {
        std::cerr << "HOI3 player-tag decoding failed\n";
        return 1;
    }

    const Hoi3LifecycleProbeResult process = ProbeHoi3Lifecycle();
    if (process.status
        != Hoi3LifecycleProbeStatus::UnsupportedExecutable)
    {
        std::cerr << "Non-HOI3 executable was accepted\n";
        return 2;
    }

    std::cout << "HOI3 lifecycle probe passed\n";
    return 0;
}
