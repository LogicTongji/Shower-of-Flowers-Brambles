#include "gui_indexed_map_core.h"

#include <algorithm>
#include <fstream>
#include <utility>

namespace
{

#pragma pack(push, 1)

struct IndexedMapHeader
{
    char magic[4];
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t pixelFormat;
};

#pragma pack(pop)

static_assert(sizeof(IndexedMapHeader) == 20);

bool HasSupportedMagic(const IndexedMapHeader& header)
{
    return header.magic[0] == 'I'
        && header.magic[1] == 'D'
        && header.magic[2] == 'X'
        && header.magic[3] == '1';
}

bool SameColor(
    const IndexedMapColor& first,
    const IndexedMapColor& second
)
{
    return first.r == second.r
        && first.g == second.g
        && first.b == second.b
        && first.a == second.a;
}

float ReadValue(const std::vector<float>& values, std::size_t itemId)
{
    return itemId < values.size() ? values[itemId] : 0.0f;
}

void FillSpans(
    const std::vector<IndexedMapSpan>& spans,
    const IndexedMapColor& color,
    std::vector<RgbaPixel>& output
)
{
    const RgbaPixel pixel{color.r, color.g, color.b, color.a};
    for (const IndexedMapSpan& span : spans)
    {
        std::fill(
            output.begin() + span.offset,
            output.begin() + span.offset + span.length,
            pixel
        );
    }
}

}

bool LoadIndexedMapData(
    const std::filesystem::path& path,
    IndexedMapData& output
)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return false;
    }

    IndexedMapHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file
        || !HasSupportedMagic(header)
        || header.version != 1
        || header.pixelFormat != 1
        || header.width == 0
        || header.height == 0)
    {
        return false;
    }

    const std::size_t pixelCount =
        static_cast<std::size_t>(header.width) * header.height;
    IndexedMapData next;
    next.width = header.width;
    next.height = header.height;
    next.itemIds.resize(pixelCount);
    file.read(
        reinterpret_cast<char*>(next.itemIds.data()),
        static_cast<std::streamsize>(pixelCount * sizeof(uint16_t))
    );
    if (!file)
    {
        return false;
    }
    output = std::move(next);
    return true;
}

IndexedMapColor ResolveIndexedMapColor(
    float value,
    const IndexedMapColorRamp& colorRamp
)
{
    if (colorRamp.empty())
    {
        return {};
    }
    IndexedMapColor color = colorRamp.front().color;
    for (const IndexedMapColorStop& stop : colorRamp)
    {
        if (value < stop.minimum)
        {
            break;
        }
        color = stop.color;
    }
    return color;
}

void BuildIndexedMapOverlay(
    const IndexedMapData& map,
    const std::vector<float>& values,
    const IndexedMapColorRamp& colorRamp,
    std::vector<RgbaPixel>& output
)
{
    output.resize(map.itemIds.size());
    for (std::size_t index = 0; index < map.itemIds.size(); ++index)
    {
        const uint16_t itemId = map.itemIds[index];
        if (itemId == 0 || itemId >= values.size())
        {
            output[index] = {};
            continue;
        }
        const IndexedMapColor color = ResolveIndexedMapColor(
            values[itemId],
            colorRamp
        );
        output[index] = {color.r, color.g, color.b, color.a};
    }
}

bool BuildIndexedMapPixelIndex(
    const IndexedMapData& map,
    IndexedMapPixelIndex& output
)
{
    const std::size_t expectedPixelCount =
        static_cast<std::size_t>(map.width) * map.height;
    if (map.width == 0
        || map.height == 0
        || map.itemIds.size() != expectedPixelCount)
    {
        return false;
    }

    uint16_t maximumItemId = 0;
    for (const uint16_t itemId : map.itemIds)
    {
        maximumItemId = std::max(maximumItemId, itemId);
    }
    output.spansByItem.assign(
        static_cast<std::size_t>(maximumItemId) + 1,
        {}
    );
    output.boundsByItem.assign(
        static_cast<std::size_t>(maximumItemId) + 1,
        {}
    );
    output.anchorsByItem.assign(
        static_cast<std::size_t>(maximumItemId) + 1,
        {}
    );

    std::vector<uint64_t> sumX(
        static_cast<std::size_t>(maximumItemId) + 1,
        0
    );
    std::vector<uint64_t> sumY(
        static_cast<std::size_t>(maximumItemId) + 1,
        0
    );
    std::vector<uint64_t> counts(
        static_cast<std::size_t>(maximumItemId) + 1,
        0
    );

    for (uint32_t y = 0; y < map.height; ++y)
    {
        const std::size_t rowOffset = static_cast<std::size_t>(y) * map.width;
        uint16_t currentItemId = 0;
        std::size_t spanStart = 0;
        for (uint32_t x = 0; x <= map.width; ++x)
        {
            const uint16_t itemId = x < map.width
                ? map.itemIds[rowOffset + x]
                : 0;
            if (x < map.width && itemId != 0)
            {
                sumX[itemId] += x;
                sumY[itemId] += y;
                ++counts[itemId];
            }
            if (itemId == currentItemId)
            {
                continue;
            }

            if (currentItemId != 0)
            {
                output.spansByItem[currentItemId].push_back({
                    rowOffset + spanStart,
                    static_cast<std::size_t>(x) - spanStart
                });
                IndexedMapBounds& bounds =
                    output.boundsByItem[currentItemId];
                const uint32_t spanEnd = x - 1;
                if (!bounds.valid)
                {
                    bounds = {
                        static_cast<uint32_t>(spanStart),
                        y,
                        spanEnd,
                        y,
                        true
                    };
                }
                else
                {
                    bounds.minX = std::min(
                        bounds.minX,
                        static_cast<uint32_t>(spanStart)
                    );
                    bounds.maxX = std::max(bounds.maxX, spanEnd);
                    bounds.minY = std::min(bounds.minY, y);
                    bounds.maxY = std::max(bounds.maxY, y);
                }
            }
            currentItemId = itemId;
            spanStart = x;
        }
    }
    for (std::size_t itemId = 1;
        itemId < output.anchorsByItem.size();
        ++itemId)
    {
        if (counts[itemId] == 0)
        {
            continue;
        }
        output.anchorsByItem[itemId] = {
            static_cast<float>(
                static_cast<double>(sumX[itemId]) / counts[itemId]
            ),
            static_cast<float>(
                static_cast<double>(sumY[itemId]) / counts[itemId]
            ),
            true
        };
    }
    return true;
}

bool UpdateChangedIndexedMapOverlay(
    const IndexedMapPixelIndex& pixelIndex,
    const std::vector<float>& previousValues,
    const std::vector<float>& currentValues,
    const IndexedMapColorRamp& colorRamp,
    std::vector<RgbaPixel>& output,
    std::vector<uint16_t>* changedItemIds
)
{
    if (pixelIndex.spansByItem.empty()
        || pixelIndex.boundsByItem.size() != pixelIndex.spansByItem.size())
    {
        return false;
    }
    if (changedItemIds)
    {
        changedItemIds->clear();
    }

    bool changed = false;
    for (std::size_t itemId = 1;
         itemId < pixelIndex.spansByItem.size();
         ++itemId)
    {
        const IndexedMapColor previousColor = ResolveIndexedMapColor(
            ReadValue(previousValues, itemId),
            colorRamp
        );
        const IndexedMapColor currentColor = ResolveIndexedMapColor(
            ReadValue(currentValues, itemId),
            colorRamp
        );
        if (SameColor(previousColor, currentColor))
        {
            continue;
        }
        FillSpans(pixelIndex.spansByItem[itemId], currentColor, output);
        if (changedItemIds)
        {
            changedItemIds->push_back(static_cast<uint16_t>(itemId));
        }
        changed = true;
    }
    return changed;
}

bool UpdateIndexedMapHighlight(
    const IndexedMapPixelIndex& pixelIndex,
    uint16_t previousItemId,
    uint16_t currentItemId,
    const IndexedMapColor& highlightColor,
    std::vector<RgbaPixel>& output
)
{
    if (pixelIndex.spansByItem.empty()
        || pixelIndex.boundsByItem.size() != pixelIndex.spansByItem.size()
        || output.empty()
        || previousItemId == currentItemId)
    {
        return false;
    }
    if (previousItemId != 0
        && previousItemId < pixelIndex.spansByItem.size())
    {
        FillSpans(pixelIndex.spansByItem[previousItemId], {}, output);
    }
    if (currentItemId != 0
        && currentItemId < pixelIndex.spansByItem.size())
    {
        FillSpans(
            pixelIndex.spansByItem[currentItemId],
            highlightColor,
            output
        );
    }
    return true;
}

void BuildIndexedMapBoundaryOverlay(
    const IndexedMapData& map,
    const IndexedMapColor& boundaryColor,
    int boundaryWidth,
    std::vector<RgbaPixel>& output
)
{
    const std::size_t pixelCount = map.itemIds.size();
    std::vector<uint8_t> boundaryMask(pixelCount, 0);
    for (uint32_t y = 0; y < map.height; ++y)
    {
        for (uint32_t x = 0; x < map.width; ++x)
        {
            const std::size_t index = static_cast<std::size_t>(y) * map.width + x;
            const uint16_t itemId = map.itemIds[index];
            if (itemId == 0)
            {
                continue;
            }
            const auto differs = [&map, x, y, itemId](int offsetX, int offsetY)
            {
                const int sampleX = static_cast<int>(x) + offsetX;
                const int sampleY = static_cast<int>(y) + offsetY;
                if (sampleX < 0
                    || sampleY < 0
                    || sampleX >= static_cast<int>(map.width)
                    || sampleY >= static_cast<int>(map.height))
                {
                    return true;
                }
                return map.itemIds[
                    static_cast<std::size_t>(sampleY) * map.width
                    + static_cast<std::size_t>(sampleX)
                ] != itemId;
            };
            if (differs(-1, 0)
                || differs(1, 0)
                || differs(0, -1)
                || differs(0, 1))
            {
                boundaryMask[index] = 1;
            }
        }
    }

    output.assign(pixelCount, {});
    const int radius = std::max(0, boundaryWidth);
    for (uint32_t y = 0; y < map.height; ++y)
    {
        for (uint32_t x = 0; x < map.width; ++x)
        {
            bool nearBoundary = false;
            for (int offsetY = -radius;
                 offsetY <= radius && !nearBoundary;
                 ++offsetY)
            {
                for (int offsetX = -radius; offsetX <= radius; ++offsetX)
                {
                    const int sampleX = static_cast<int>(x) + offsetX;
                    const int sampleY = static_cast<int>(y) + offsetY;
                    if (sampleX < 0
                        || sampleY < 0
                        || sampleX >= static_cast<int>(map.width)
                        || sampleY >= static_cast<int>(map.height))
                    {
                        continue;
                    }
                    if (boundaryMask[
                        static_cast<std::size_t>(sampleY) * map.width
                        + static_cast<std::size_t>(sampleX)
                    ] != 0)
                    {
                        nearBoundary = true;
                        break;
                    }
                }
            }
            if (nearBoundary)
            {
                output[static_cast<std::size_t>(y) * map.width + x] = {
                    boundaryColor.r,
                    boundaryColor.g,
                    boundaryColor.b,
                    boundaryColor.a
                };
            }
        }
    }
}
