#include "war_map_state.h"

#include <cmath>

namespace
{

constexpr std::array<uint32_t, kWarMapRegionCount + 1>
kStaticRegionPopulation = {
    0,
    32453000,
    11601000,
    12042000,
    13385000,
    1651100,
    978000,
    6716000,
    1196000,
    2036000,
    2084000,
    53674000,
    9919000,
    9986000,
    31410000,
    3000000,
    38837000,
    11756000,
    28294000,
    41215000,
    15805000,
    21240000,
    23354000,
    34290000,
    25516000,
    2122000,
    2185000,
    7565000,
    3005000,
    2231000,
    680000,
    149000,
    2093000,
    688000,
    848000,
    5608000,
    4360000,
    1100000,
    5291000,
    616000,
    3000000,
    800000
};

float GetStaticControlledPercentage(
    std::string_view regionName
)
{
    if (regionName == "rehe_region"
        || regionName == "fengtian_region"
        || regionName == "jilin_region"
        || regionName == "songjiang_region"
        || regionName == "andong_region"
        || regionName == "jiandao_region"
        || regionName == "heihe_region"
        || regionName == "nenjiang_region"
        || regionName == "xingan_region"
        || regionName == "heilongjiang_region"
        || regionName == "liaonning_region"
        || regionName == "chahar_region"
        || regionName == "hebei_region"
        || regionName == "shandong_region"
        || regionName == "jiangsu_region"
        || regionName == "SF_Shanghai"
        || regionName == "anhui_region")
    {
        return 95.0f;
    }

    if (regionName == "hubei_region")
    {
        return 85.0f;
    }

    if (regionName == "shanxi_region")
    {
        return 65.0f;
    }

    if (regionName == "henan_region")
    {
        return 10.0f;
    }

    if (regionName == "hunan_region")
    {
        return 15.0f;
    }

    if (regionName == "jiangxi_region")
    {
        return 35.0f;
    }

    if (regionName == "suiyuan_region")
    {
        return 5.0f;
    }

    if (regionName == "fujian_region"
        || regionName == "chekiang_region"
        || regionName == "guangdong_region")
    {
        return 15.0f;
    }

    return 0.0f;
}

}

bool MockWarMapStateSource::Read(
    WarMapState& output
)
{
    output.visible = true;
    output.active = true;
    output.date = 1;
    output.viewerTag = "CHI";
    output.warProgress = {
        true,
        0.62f,
        0.38f
    };
    output.controlledPercentages.fill(0.0f);
    output.populations.fill(RegionPopulationState{});

    for (size_t regionId = 1;
         regionId <= kWarMapRegionCount;
         ++regionId)
    {
        const std::string_view regionName =
            kWarMapRegionNames[regionId - 1];
        const uint32_t totalPopulation =
            kStaticRegionPopulation[regionId];
        const float controlledPercentage =
            GetStaticControlledPercentage(regionName);

        const float ratio =
            controlledPercentage / 100.0f;

        const uint32_t affectedPopulation =
            static_cast<uint32_t>(
                static_cast<float>(totalPopulation)
                * std::pow(
                    ratio < 0.0f ? 0.0f : ratio,
                    1.2f
                )
                + 0.5f
            );

        output.controlledPercentages[regionId] =
            controlledPercentage;

        output.populations[regionId] = {
            totalPopulation > 0,
            totalPopulation,
            affectedPopulation,
            totalPopulation - affectedPopulation
        };
    }

    return true;
}

std::vector<float> ToPercentageVector(
    const WarMapState& state
)
{
    return std::vector<float>(
        state.controlledPercentages.begin(),
        state.controlledPercentages.end()
    );
}

bool HasStateChanged(
    const WarMapState& previous,
    const WarMapState& current
)
{
    if (previous.visible != current.visible
        || previous.active != current.active
        || previous.date != current.date
        || previous.viewerTag != current.viewerTag)
    {
        return true;
    }

    for (size_t index = 0;
         index < previous.controlledPercentages.size();
         ++index)
    {
        if (std::fabs(
            previous.controlledPercentages[index]
            - current.controlledPercentages[index]
        ) > 0.001f)
        {
            return true;
        }

        const RegionPopulationState& previousPopulation =
            previous.populations[index];
        const RegionPopulationState& currentPopulation =
            current.populations[index];

        if (previousPopulation.known
            != currentPopulation.known
            || previousPopulation.total
                != currentPopulation.total
            || previousPopulation.affected
                != currentPopulation.affected
            || previousPopulation.remaining
                != currentPopulation.remaining)
        {
            return true;
        }
    }

    return false;
}

void BuildOverlayFromState(
    const RegionMap& regionMap,
    const WarMapState& state,
    std::vector<RgbaPixel>& output
)
{
    if (!state.visible || !state.active)
    {
        output.assign(
            regionMap.regionIds.size(),
            RgbaPixel{0, 0, 0, 0}
        );
        return;
    }

    const std::vector<float> percentages =
        ToPercentageVector(state);

    BuildRegionOverlay(
        regionMap,
        percentages,
        output
    );
}
