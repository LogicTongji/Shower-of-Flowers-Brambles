#include "region_color_core.h"

#include <algorithm>
#include <fstream>

namespace
{

#pragma pack(push, 1)

struct RegionMapHeader
{
    char magic[4];
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t pixelFormat;
};

#pragma pack(pop)

static_assert(
    sizeof(RegionMapHeader) == 20,
    "Unexpected RegionMapHeader size"
);

}

bool LoadRegionMap(
    const std::filesystem::path& path,
    RegionMap& output
)
{
    std::ifstream file(
        path,
        std::ios::binary
    );

    if (!file)
    {
        return false;
    }

    RegionMapHeader header{};

    file.read(
        reinterpret_cast<char*>(&header),
        sizeof(header)
    );

    if (!file
        || header.magic[0] != 'R'
        || header.magic[1] != 'I'
        || header.magic[2] != 'D'
        || header.magic[3] != '1'
        || header.version != 1
        || header.pixelFormat != 1
        || header.width == 0
        || header.height == 0)
    {
        return false;
    }

    const size_t pixelCount =
        static_cast<size_t>(header.width)
        * static_cast<size_t>(header.height);

    output.width = header.width;
    output.height = header.height;
    output.regionIds.resize(pixelCount);

    file.read(
        reinterpret_cast<char*>(output.regionIds.data()),
        static_cast<std::streamsize>(
            pixelCount * sizeof(uint16_t)
        )
    );

    return file.good();
}

RegionColor GetRegionColor(
    float controlledPercentage
)
{
    if (controlledPercentage >= 90.0f)
    {
        return {80, 0, 0, 220};
    }

    if (controlledPercentage >= 80.0f)
    {
        return {150, 20, 20, 210};
    }

    if (controlledPercentage >= 60.0f)
    {
        return {230, 100, 100, 200};
    }

    if (controlledPercentage >= 20.0f)
    {
        return {240, 210, 60, 190};
    }

    if (controlledPercentage > 0.0f)
    {
        return {255, 245, 170, 180};
    }

    return {80, 130, 80, 160};
}

void BuildRegionOverlay(
    const RegionMap& regionMap,
    const std::vector<float>& percentages,
    std::vector<RgbaPixel>& output
)
{
    output.resize(regionMap.regionIds.size());

    for (size_t index = 0;
         index < regionMap.regionIds.size();
         ++index)
    {
        const uint16_t regionId =
            regionMap.regionIds[index];

        if (regionId == 0
            || regionId >= percentages.size())
        {
            output[index] = {0, 0, 0, 0};
            continue;
        }

        const RegionColor color =
            GetRegionColor(percentages[regionId]);

        output[index] = {
            color.r,
            color.g,
            color.b,
            color.a
        };
    }
}

namespace
{

bool SameColor(
    const RegionColor& first,
    const RegionColor& second
)
{
    return first.r == second.r
        && first.g == second.g
        && first.b == second.b
        && first.a == second.a;
}

float GetPercentage(
    const std::vector<float>& percentages,
    size_t regionId
)
{
    if (regionId >= percentages.size())
    {
        return 0.0f;
    }

    return percentages[regionId];
}

void FillRegionSpans(
    const std::vector<RegionSpan>& spans,
    const RegionColor& color,
    std::vector<RgbaPixel>& output
)
{
    const RgbaPixel pixel{
        color.r,
        color.g,
        color.b,
        color.a
    };

    for (const RegionSpan& span : spans)
    {
        std::fill(
            output.begin() + span.offset,
            output.begin()
                + span.offset
                + span.length,
            pixel
        );
    }
}

}

bool BuildRegionPixelIndex(
    const RegionMap& regionMap,
    RegionPixelIndex& output
)
{
    const size_t expectedPixelCount =
        static_cast<size_t>(regionMap.width)
        * static_cast<size_t>(regionMap.height);

    if (regionMap.width == 0
        || regionMap.height == 0
        || regionMap.regionIds.size() != expectedPixelCount)
    {
        return false;
    }

    uint16_t maximumRegionId = 0;

    for (const uint16_t regionId : regionMap.regionIds)
    {
        maximumRegionId = std::max(
            maximumRegionId,
            regionId
        );
    }

    output.spansByRegion.clear();
    output.spansByRegion.resize(
        static_cast<size_t>(maximumRegionId) + 1
    );
    output.boundsByRegion.clear();
    output.boundsByRegion.resize(
        static_cast<size_t>(maximumRegionId) + 1
    );

    for (uint32_t y = 0;
         y < regionMap.height;
         ++y)
    {
        const size_t rowOffset =
            static_cast<size_t>(y) * regionMap.width;

        uint16_t currentRegionId = 0;
        size_t spanStart = 0;

        for (uint32_t x = 0;
             x <= regionMap.width;
             ++x)
        {
            const uint16_t regionId =
                x < regionMap.width
                ? regionMap.regionIds[rowOffset + x]
                : 0;

            if (regionId == currentRegionId)
            {
                continue;
            }

            if (currentRegionId != 0)
            {
                output.spansByRegion[currentRegionId].push_back({
                    rowOffset + spanStart,
                    static_cast<size_t>(x) - spanStart
                });

                RegionBounds& bounds =
                    output.boundsByRegion[currentRegionId];

                const uint32_t spanEnd = x - 1;

                if (!bounds.valid)
                {
                    bounds.minX = static_cast<uint32_t>(spanStart);
                    bounds.maxX = spanEnd;
                    bounds.minY = y;
                    bounds.maxY = y;
                    bounds.valid = true;
                }
                else
                {
                    bounds.minX = std::min(
                        bounds.minX,
                        static_cast<uint32_t>(spanStart)
                    );
                    bounds.maxX = std::max(
                        bounds.maxX,
                        spanEnd
                    );
                    bounds.minY = std::min(bounds.minY, y);
                    bounds.maxY = std::max(bounds.maxY, y);
                }
            }

            currentRegionId = regionId;
            spanStart = x;
        }
    }

    return true;
}

bool UpdateChangedRegionOverlay(
    const RegionPixelIndex& pixelIndex,
    const std::vector<float>& previousPercentages,
    const std::vector<float>& currentPercentages,
    std::vector<RgbaPixel>& output,
    std::vector<uint16_t>* changedRegionIds
)
{
    if (pixelIndex.spansByRegion.empty()
        || pixelIndex.boundsByRegion.size()
            != pixelIndex.spansByRegion.size())
    {
        return false;
    }

    if (changedRegionIds)
    {
        changedRegionIds->clear();
    }

    bool changed = false;

    for (size_t regionId = 1;
         regionId < pixelIndex.spansByRegion.size();
         ++regionId)
    {
        const RegionColor previousColor =
            GetRegionColor(
                GetPercentage(
                    previousPercentages,
                    regionId
                )
            );

        const RegionColor currentColor =
            GetRegionColor(
                GetPercentage(
                    currentPercentages,
                    regionId
                )
            );

        if (SameColor(previousColor, currentColor))
        {
            continue;
        }

        FillRegionSpans(
            pixelIndex.spansByRegion[regionId],
            currentColor,
            output
        );

        if (changedRegionIds)
        {
            changedRegionIds->push_back(
                static_cast<uint16_t>(regionId)
            );
        }

        changed = true;
    }

    return changed;
}

bool UpdateRegionHighlight(
    const RegionPixelIndex& pixelIndex,
    uint16_t previousRegionId,
    uint16_t currentRegionId,
    std::vector<RgbaPixel>& output
)
{
    if (pixelIndex.spansByRegion.empty()
        || pixelIndex.boundsByRegion.size()
            != pixelIndex.spansByRegion.size()
        || output.empty()
        || previousRegionId == currentRegionId)
    {
        return false;
    }

    const RegionColor transparent{
        0,
        0,
        0,
        0
    };

    const RegionColor highlight{
        230,
        240,
        255,
        120
    };

    if (previousRegionId != 0
        && previousRegionId
            < pixelIndex.spansByRegion.size())
    {
        FillRegionSpans(
            pixelIndex.spansByRegion[previousRegionId],
            transparent,
            output
        );
    }

    if (currentRegionId != 0
        && currentRegionId
            < pixelIndex.spansByRegion.size())
    {
        FillRegionSpans(
            pixelIndex.spansByRegion[currentRegionId],
            highlight,
            output
        );
    }

    return true;
}
