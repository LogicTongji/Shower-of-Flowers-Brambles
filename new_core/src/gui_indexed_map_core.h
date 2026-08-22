#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

struct IndexedMapData
{
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint16_t> itemIds;
};

struct IndexedMapColor
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
};

struct IndexedMapColorStop
{
    float minimum = 0.0f;
    IndexedMapColor color;
};

using IndexedMapColorRamp = std::vector<IndexedMapColorStop>;

struct RgbaPixel
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
};

struct IndexedMapSpan
{
    size_t offset = 0;
    size_t length = 0;
};

struct IndexedMapBounds
{
    uint32_t minX = 0;
    uint32_t minY = 0;
    uint32_t maxX = 0;
    uint32_t maxY = 0;
    bool valid = false;
};

struct IndexedMapAnchor
{
    float x = 0.0f;
    float y = 0.0f;
    bool valid = false;
};

struct IndexedMapPixelIndex
{
    std::vector<std::vector<IndexedMapSpan>> spansByItem;
    std::vector<IndexedMapBounds> boundsByItem;
    std::vector<IndexedMapAnchor> anchorsByItem;
};

bool LoadIndexedMapData(
    const std::filesystem::path& path,
    IndexedMapData& output
);

IndexedMapColor ResolveIndexedMapColor(
    float value,
    const IndexedMapColorRamp& colorRamp
);

void BuildIndexedMapOverlay(
    const IndexedMapData& map,
    const std::vector<float>& values,
    const IndexedMapColorRamp& colorRamp,
    std::vector<RgbaPixel>& output
);

bool BuildIndexedMapPixelIndex(
    const IndexedMapData& map,
    IndexedMapPixelIndex& output
);

bool UpdateChangedIndexedMapOverlay(
    const IndexedMapPixelIndex& pixelIndex,
    const std::vector<float>& previousValues,
    const std::vector<float>& currentValues,
    const IndexedMapColorRamp& colorRamp,
    std::vector<RgbaPixel>& output,
    std::vector<uint16_t>* changedItemIds = nullptr
);

bool UpdateIndexedMapHighlight(
    const IndexedMapPixelIndex& pixelIndex,
    uint16_t previousItemId,
    uint16_t currentItemId,
    const IndexedMapColor& highlightColor,
    std::vector<RgbaPixel>& output
);

void BuildIndexedMapBoundaryOverlay(
    const IndexedMapData& map,
    const IndexedMapColor& boundaryColor,
    int boundaryWidth,
    std::vector<RgbaPixel>& output
);
