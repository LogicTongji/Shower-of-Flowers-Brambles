#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "region_color_core.h"
#include "war_map_regions.h"

struct RegionPopulationState
{
    bool known = false;
    uint32_t total = 0;
    uint32_t affected = 0;
    uint32_t remaining = 0;
};

struct WarProgressState
{
    bool known = false;
    float own = 0.0f;
    float enemy = 0.0f;
};

struct WarMapState
{
    bool visible = false;
    bool active = false;
    int64_t date = 0;
    std::string viewerTag;
    WarProgressState warProgress;

    std::array<float, kWarMapRegionCount + 1>
        controlledPercentages{};

    std::array<RegionPopulationState,
        kWarMapRegionCount + 1>
        populations{};
};

class IWarMapStateSource
{
public:
    virtual ~IWarMapStateSource() = default;

    virtual bool Read(
        WarMapState& output
    ) = 0;
};

class MockWarMapStateSource final
    : public IWarMapStateSource
{
public:
    bool Read(
        WarMapState& output
    ) override;
};

std::vector<float> ToPercentageVector(
    const WarMapState& state
);

bool HasStateChanged(
    const WarMapState& previous,
    const WarMapState& current
);

void BuildOverlayFromState(
    const RegionMap& regionMap,
    const WarMapState& state,
    std::vector<RgbaPixel>& output
);
