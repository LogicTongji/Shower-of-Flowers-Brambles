#include "war_map_state.h"

#include <cmath>

bool MockWarMapStateSource::Read(
    WarMapState& output
)
{
    output.visible = true;
    output.active = true;
    output.date = 1;
    output.viewerTag = "CHI";
    output.controlledPercentages.fill(0.0f);
    output.populations.fill(RegionPopulationState{});

    output.controlledPercentages[1] = 15.0f;
    output.controlledPercentages[2] = 25.0f;
    output.controlledPercentages[3] = 65.0f;
    output.controlledPercentages[4] = 85.0f;
    output.controlledPercentages[5] = 95.0f;

    for (size_t regionId = 1;
         regionId <= kWarMapRegionCount;
         ++regionId)
    {
        const uint32_t totalPopulation =
            10000000U
            + static_cast<uint32_t>(regionId) * 250000U;

        const float ratio =
            output.controlledPercentages[regionId] / 100.0f;

        const uint32_t affectedPopulation =
            static_cast<uint32_t>(
                static_cast<float>(totalPopulation)
                * (ratio < 0.0f ? 0.0f : ratio)
            );

        output.populations[regionId] = {
            true,
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
