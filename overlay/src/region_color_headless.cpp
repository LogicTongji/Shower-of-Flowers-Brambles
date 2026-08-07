#include "region_color_core.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

#pragma pack(push, 1)

struct BmpFileHeader
{
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixelOffset;
};

struct BmpInfoHeader
{
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitsPerPixel;
    uint32_t compression;
    uint32_t imageSize;
    int32_t pixelsPerMeterX;
    int32_t pixelsPerMeterY;
    uint32_t colorsUsed;
    uint32_t importantColors;
};

#pragma pack(pop)

static_assert(
    sizeof(BmpFileHeader) == 14,
    "Invalid BMP file header"
);

static_assert(
    sizeof(BmpInfoHeader) == 40,
    "Invalid BMP info header"
);

uint32_t PackPixel(
    const RgbaPixel& pixel
)
{
    return static_cast<uint32_t>(pixel.b)
        | (static_cast<uint32_t>(pixel.g) << 8)
        | (static_cast<uint32_t>(pixel.r) << 16)
        | (static_cast<uint32_t>(pixel.a) << 24);
}

RgbaPixel BlendOver(
    const RgbaPixel& background,
    const RgbaPixel& foreground
)
{
    const uint32_t alpha = foreground.a;
    const uint32_t inverseAlpha = 255 - alpha;

    return {
        static_cast<uint8_t>(
            (
                static_cast<uint32_t>(foreground.r) * alpha
                + static_cast<uint32_t>(background.r)
                    * inverseAlpha
            ) / 255
        ),

        static_cast<uint8_t>(
            (
                static_cast<uint32_t>(foreground.g) * alpha
                + static_cast<uint32_t>(background.g)
                    * inverseAlpha
            ) / 255
        ),

        static_cast<uint8_t>(
            (
                static_cast<uint32_t>(foreground.b) * alpha
                + static_cast<uint32_t>(background.b)
                    * inverseAlpha
            ) / 255
        ),

        255
    };
}

void WriteBmp32(
    const fs::path& path,
    uint32_t width,
    uint32_t height,
    const std::vector<RgbaPixel>& pixels
)
{
    if (pixels.size()
        != static_cast<size_t>(width)
            * static_cast<size_t>(height))
    {
        throw std::runtime_error(
            "BMP pixel count mismatch"
        );
    }

    constexpr uint32_t colorMaskSize = 16;
    constexpr uint32_t pixelOffset =
        sizeof(BmpFileHeader)
        + sizeof(BmpInfoHeader)
        + colorMaskSize;

    BmpFileHeader fileHeader{};
    fileHeader.type = 0x4D42;
    fileHeader.pixelOffset = pixelOffset;
    fileHeader.size =
        pixelOffset
        + static_cast<uint32_t>(
            pixels.size() * sizeof(uint32_t)
        );

    BmpInfoHeader infoHeader{};
    infoHeader.size = sizeof(BmpInfoHeader);
    infoHeader.width = static_cast<int32_t>(width);
    infoHeader.height = static_cast<int32_t>(height);
    infoHeader.planes = 1;
    infoHeader.bitsPerPixel = 32;
    infoHeader.compression = 3;
    infoHeader.imageSize =
        static_cast<uint32_t>(
            pixels.size() * sizeof(uint32_t)
        );

    std::ofstream file(
        path,
        std::ios::binary
    );

    if (!file)
    {
        throw std::runtime_error(
            "cannot create BMP: "
            + path.string()
        );
    }

    file.write(
        reinterpret_cast<const char*>(&fileHeader),
        sizeof(fileHeader)
    );

    file.write(
        reinterpret_cast<const char*>(&infoHeader),
        sizeof(infoHeader)
    );

    const uint32_t redMask = 0x00FF0000;
    const uint32_t greenMask = 0x0000FF00;
    const uint32_t blueMask = 0x000000FF;
    const uint32_t alphaMask = 0xFF000000;

    file.write(
        reinterpret_cast<const char*>(&redMask),
        sizeof(redMask)
    );

    file.write(
        reinterpret_cast<const char*>(&greenMask),
        sizeof(greenMask)
    );

    file.write(
        reinterpret_cast<const char*>(&blueMask),
        sizeof(blueMask)
    );

    file.write(
        reinterpret_cast<const char*>(&alphaMask),
        sizeof(alphaMask)
    );

    for (int y = static_cast<int>(height) - 1;
         y >= 0;
         --y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            const RgbaPixel& pixel =
                pixels[
                    static_cast<size_t>(y)
                    * width
                    + x
                ];

            const uint32_t packed =
                PackPixel(pixel);

            file.write(
                reinterpret_cast<const char*>(&packed),
                sizeof(packed)
            );
        }
    }

    if (!file)
    {
        throw std::runtime_error(
            "failed to write BMP"
        );
    }
}

uint16_t RegionAt(
    const RegionMap& map,
    int x,
    int y
)
{
    if (x < 0
        || y < 0
        || x >= static_cast<int>(map.width)
        || y >= static_cast<int>(map.height))
    {
        return 0;
    }

    return map.regionIds[
        static_cast<size_t>(y)
        * map.width
        + x
    ];
}

bool IsRegionBoundary(
    const RegionMap& map,
    int x,
    int y
)
{
    const uint16_t regionId =
        RegionAt(map, x, y);

    if (regionId == 0)
    {
        return false;
    }

    return RegionAt(map, x - 1, y) != regionId
        || RegionAt(map, x + 1, y) != regionId
        || RegionAt(map, x, y - 1) != regionId
        || RegionAt(map, x, y + 1) != regionId;
}

RgbaPixel GetDebugRegionColor(
    uint16_t regionId
)
{
    uint32_t value =
        static_cast<uint32_t>(regionId)
        * 2654435761u;

    const uint8_t red =
        static_cast<uint8_t>(50 + (value & 0x7F));

    const uint8_t green =
        static_cast<uint8_t>(
            50 + ((value >> 8) & 0x7F)
        );

    const uint8_t blue =
        static_cast<uint8_t>(
            50 + ((value >> 16) & 0x7F)
        );

    return {
        red,
        green,
        blue,
        255
    };
}

std::vector<RgbaPixel> BuildWarMapPreview(
    const RegionMap& regionMap,
    const std::vector<float>& percentages
)
{
    std::vector<RgbaPixel> overlay;

    BuildRegionOverlay(
        regionMap,
        percentages,
        overlay
    );

    std::vector<RgbaPixel> output(
        overlay.size()
    );

    const RgbaPixel background = {
        25,
        35,
        48,
        255
    };

    const RgbaPixel boundaryColor = {
        35,
        35,
        35,
        255
    };

    for (uint32_t y = 0;
         y < regionMap.height;
         ++y)
    {
        for (uint32_t x = 0;
             x < regionMap.width;
             ++x)
        {
            const size_t index =
                static_cast<size_t>(y)
                * regionMap.width
                + x;

            const uint16_t regionId =
                regionMap.regionIds[index];

            if (regionId == 0)
            {
                output[index] = background;
                continue;
            }

            if (IsRegionBoundary(
                regionMap,
                static_cast<int>(x),
                static_cast<int>(y)
            ))
            {
                output[index] =
                    boundaryColor;

                continue;
            }

            output[index] =
                BlendOver(
                    background,
                    overlay[index]
                );
        }
    }

    return output;
}

std::vector<RgbaPixel> BuildDebugPreview(
    const RegionMap& regionMap
)
{
    std::vector<RgbaPixel> output(
        regionMap.regionIds.size()
    );

    const RgbaPixel background = {
        25,
        35,
        48,
        255
    };

    const RgbaPixel boundaryColor = {
        20,
        20,
        20,
        255
    };

    for (uint32_t y = 0;
         y < regionMap.height;
         ++y)
    {
        for (uint32_t x = 0;
             x < regionMap.width;
             ++x)
        {
            const size_t index =
                static_cast<size_t>(y)
                * regionMap.width
                + x;

            const uint16_t regionId =
                regionMap.regionIds[index];

            if (regionId == 0)
            {
                output[index] = background;
                continue;
            }

            if (IsRegionBoundary(
                regionMap,
                static_cast<int>(x),
                static_cast<int>(y)
            ))
            {
                output[index] =
                    boundaryColor;

                continue;
            }

            output[index] =
                GetDebugRegionColor(regionId);
        }
    }

    return output;
}

int main(int argc, char** argv)
{
    try
    {
        const fs::path inputPath =
            argc > 1
                ? fs::path(argv[1])
                : fs::path("china_region_ids.bin");

        const fs::path warMapPath =
            argc > 2
                ? fs::path(argv[2])
                : fs::path("war_map_preview.bmp");

        const fs::path debugMapPath =
            argc > 3
                ? fs::path(argv[3])
                : fs::path("region_debug.bmp");

        RegionMap regionMap;

        if (!LoadRegionMap(
            inputPath,
            regionMap
        ))
        {
            std::cerr
                << "failed to load region map\n";

            return 1;
        }

        std::vector<float> percentages(
            64,
            0.0f
        );

        percentages[1] = 15.0f;
        percentages[2] = 25.0f;
        percentages[3] = 65.0f;
        percentages[4] = 85.0f;
        percentages[5] = 95.0f;

        const auto warMap =
            BuildWarMapPreview(
                regionMap,
                percentages
            );

        const auto debugMap =
            BuildDebugPreview(
                regionMap
            );

        WriteBmp32(
            warMapPath,
            regionMap.width,
            regionMap.height,
            warMap
        );

        WriteBmp32(
            debugMapPath,
            regionMap.width,
            regionMap.height,
            debugMap
        );

        std::cout
            << "width: "
            << regionMap.width
            << "\nheight: "
            << regionMap.height
            << "\nwar map: "
            << warMapPath
            << "\ndebug map: "
            << debugMapPath
            << '\n';

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "error: "
            << error.what()
            << '\n';

        return 1;
    }
}