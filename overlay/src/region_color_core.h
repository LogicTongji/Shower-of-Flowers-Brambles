#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

struct RegionMap
{
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint16_t> regionIds;
};

struct RegionColor
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
};

struct RgbaPixel
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
};

struct RegionSpan
{
    size_t offset = 0;
    size_t length = 0;
};

struct RegionBounds
{
    uint32_t minX = 0;
    uint32_t minY = 0;
    uint32_t maxX = 0;
    uint32_t maxY = 0;
    bool valid = false;
};

struct RegionPixelIndex
{
    std::vector<std::vector<RegionSpan>>
        spansByRegion;
    std::vector<RegionBounds>
        boundsByRegion;
};

bool LoadRegionMap(
    const std::filesystem::path& path,
    RegionMap& output
);

RegionColor GetRegionColor(
    float controlledPercentage
);

void BuildRegionOverlay(
    const RegionMap& regionMap,
    const std::vector<float>& percentages,
    std::vector<RgbaPixel>& output
);

bool BuildRegionPixelIndex(
    const RegionMap& regionMap,
    RegionPixelIndex& output
);

bool UpdateChangedRegionOverlay(
    const RegionPixelIndex& pixelIndex,
    const std::vector<float>& previousPercentages,
    const std::vector<float>& currentPercentages,
    std::vector<RgbaPixel>& output,
    std::vector<uint16_t>* changedRegionIds = nullptr
);

bool UpdateRegionHighlight(
    const RegionPixelIndex& pixelIndex,
    uint16_t previousRegionId,
    uint16_t currentRegionId,
    std::vector<RgbaPixel>& output
);
