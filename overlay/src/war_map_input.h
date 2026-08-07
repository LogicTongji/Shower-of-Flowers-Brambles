#pragma once

#include <cstdint>

#include "region_color_core.h"

struct MapRect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

inline uint16_t PickRegion(
    const RegionMap& regionMap,
    const MapRect& mapRect,
    int mouseX,
    int mouseY
)
{
    if (mouseX < mapRect.x
        || mouseY < mapRect.y
        || mouseX >= mapRect.x + mapRect.width
        || mouseY >= mapRect.y + mapRect.height)
    {
        return 0;
    }

    const int localX = mouseX - mapRect.x;
    const int localY = mouseY - mapRect.y;

    const int textureX =
        localX * static_cast<int>(regionMap.width)
        / mapRect.width;

    const int textureY =
        localY * static_cast<int>(regionMap.height)
        / mapRect.height;

    if (textureX < 0
        || textureY < 0
        || textureX >= static_cast<int>(regionMap.width)
        || textureY >= static_cast<int>(regionMap.height))
    {
        return 0;
    }

    return regionMap.regionIds[
        static_cast<size_t>(textureY)
        * regionMap.width
        + textureX
    ];
}
