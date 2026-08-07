#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "war_map_regions.h"

namespace fs = std::filesystem;

namespace
{

constexpr uint32_t kColorLookupSize = 1u << 24;

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

struct ProvinceMap
{
    int width = 0;
    int height = 0;
    std::vector<uint16_t> regionIds;
    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxX = -1;
    int maxY = -1;
};

struct CroppedRegionMap
{
    int width = 0;
    int height = 0;
    std::vector<uint16_t> regionIds;
};

std::string Trim(std::string_view value)
{
    size_t begin = 0;
    size_t end = value.size();

    while (begin < end && std::isspace(
        static_cast<unsigned char>(value[begin])))
    {
        ++begin;
    }

    while (end > begin && std::isspace(
        static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

bool ParseFourIntegers(
    const std::string& line,
    std::array<int, 4>& values)
{
    size_t position = 0;
    int count = 0;

    while (position < line.size() && count < 4)
    {
        while (position < line.size()
            && !std::isdigit(static_cast<unsigned char>(line[position])))
        {
            ++position;
        }

        if (position == line.size())
        {
            break;
        }

        int value = 0;

        while (position < line.size()
            && std::isdigit(static_cast<unsigned char>(line[position])))
        {
            value = value * 10 + (line[position] - '0');
            ++position;
        }

        values[count++] = value;
    }

    return count == 4;
}

std::vector<uint16_t> LoadColorToProvince(
    const fs::path& definitionPath,
    uint32_t& maximumProvinceId)
{
    std::ifstream file(definitionPath, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error(
            "cannot open definition.csv: "
            + definitionPath.string());
    }

    std::vector<uint16_t> colorToProvince(
        kColorLookupSize,
        0
    );

    maximumProvinceId = 0;
    std::string line;

    while (std::getline(file, line))
    {
        std::array<int, 4> values{};

        if (!ParseFourIntegers(line, values))
        {
            continue;
        }

        const int provinceId = values[0];
        const int red = values[1];
        const int green = values[2];
        const int blue = values[3];

        if (provinceId < 0 || provinceId > UINT16_MAX
            || red < 0 || red > 255
            || green < 0 || green > 255
            || blue < 0 || blue > 255)
        {
            continue;
        }

        const uint32_t color =
            (static_cast<uint32_t>(red) << 16)
            | (static_cast<uint32_t>(green) << 8)
            | static_cast<uint32_t>(blue);

        colorToProvince[color] =
            static_cast<uint16_t>(provinceId);

        if (static_cast<uint32_t>(provinceId) > maximumProvinceId)
        {
            maximumProvinceId =
                static_cast<uint32_t>(provinceId);
        }
    }

    return colorToProvince;
}

std::vector<uint16_t> LoadProvinceToRegion(
    const fs::path& regionPath,
    uint32_t maximumProvinceId)
{
    std::unordered_map<std::string, uint16_t> regionIds;

    for (size_t index = 0;
         index < kWarMapRegionNames.size();
         ++index)
    {
        regionIds.emplace(
            std::string(kWarMapRegionNames[index]),
            static_cast<uint16_t>(index + 1)
        );
    }

    std::vector<uint16_t> provinceToRegion(
        maximumProvinceId + 1,
        0
    );

    std::ifstream file(regionPath, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error(
            "cannot open region.txt: "
            + regionPath.string());
    }

    uint16_t currentRegionId = 0;
    std::string line;

    while (std::getline(file, line))
    {
        const size_t commentPosition = line.find('#');

        if (commentPosition != std::string::npos)
        {
            line.resize(commentPosition);
        }

        const std::string trimmed = Trim(line);

        if (trimmed.empty())
        {
            continue;
        }

        if (trimmed == "}")
        {
            currentRegionId = 0;
            continue;
        }

        const size_t equalsPosition = trimmed.find('=');
        const size_t bracePosition = trimmed.find('{');

        if (equalsPosition != std::string::npos
            && bracePosition != std::string::npos
            && equalsPosition < bracePosition)
        {
            const std::string regionName = Trim(
                std::string_view(trimmed).substr(0, equalsPosition)
            );

            const auto iterator = regionIds.find(regionName);

            if (iterator != regionIds.end())
            {
                currentRegionId = iterator->second;
            }
            else
            {
                currentRegionId = 0;
            }

            continue;
        }

        if (currentRegionId == 0)
        {
            continue;
        }

        size_t position = 0;

        while (position < trimmed.size())
        {
            while (position < trimmed.size()
                && !std::isdigit(static_cast<unsigned char>(trimmed[position])))
            {
                ++position;
            }

            if (position == trimmed.size())
            {
                break;
            }

            uint32_t provinceId = 0;

            while (position < trimmed.size()
                && std::isdigit(static_cast<unsigned char>(trimmed[position])))
            {
                provinceId = provinceId * 10
                    + static_cast<uint32_t>(trimmed[position] - '0');
                ++position;
            }

            if (provinceId <= maximumProvinceId
                && provinceToRegion[provinceId] == 0)
            {
                provinceToRegion[provinceId] = currentRegionId;
            }
        }
    }

    return provinceToRegion;
}

ProvinceMap LoadProvinceMap(
    const fs::path& provincesPath,
    const std::vector<uint16_t>& colorToProvince,
    const std::vector<uint16_t>& provinceToRegion)
{
    std::ifstream file(provincesPath, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error(
            "cannot open provinces.bmp: "
            + provincesPath.string());
    }

    BmpFileHeader fileHeader{};
    BmpInfoHeader infoHeader{};

    file.read(
        reinterpret_cast<char*>(&fileHeader),
        sizeof(fileHeader)
    );

    file.read(
        reinterpret_cast<char*>(&infoHeader),
        sizeof(infoHeader)
    );

    if (!file
        || fileHeader.type != 0x4D42
        || infoHeader.bitsPerPixel != 24
        || infoHeader.compression != 0
        || infoHeader.width <= 0
        || infoHeader.height == 0)
    {
        throw std::runtime_error(
            "provinces.bmp must be an uncompressed 24-bit BMP");
    }

    ProvinceMap result;
    result.width = infoHeader.width;
    result.height = infoHeader.height > 0
        ? infoHeader.height
        : -infoHeader.height;

    result.regionIds.resize(
        static_cast<size_t>(result.width)
        * static_cast<size_t>(result.height),
        0
    );

    const size_t rowStride = (
        static_cast<size_t>(result.width) * 3 + 3
    ) & ~static_cast<size_t>(3);

    std::vector<uint8_t> row(rowStride);
    const bool bottomUp = infoHeader.height > 0;

    file.seekg(fileHeader.pixelOffset, std::ios::beg);

    for (int sourceRow = 0; sourceRow < result.height; ++sourceRow)
    {
        file.read(
            reinterpret_cast<char*>(row.data()),
            static_cast<std::streamsize>(row.size())
        );

        if (!file)
        {
            throw std::runtime_error(
                "unexpected end of provinces.bmp");
        }

        const int y = bottomUp
            ? result.height - 1 - sourceRow
            : sourceRow;

        for (int x = 0; x < result.width; ++x)
        {
            const size_t offset = static_cast<size_t>(x) * 3;
            const uint32_t blue = row[offset];
            const uint32_t green = row[offset + 1];
            const uint32_t red = row[offset + 2];

            const uint32_t color =
                (red << 16)
                | (green << 8)
                | blue;

            const uint16_t provinceId =
                colorToProvince[color];

            uint16_t regionId = 0;

            if (provinceId < provinceToRegion.size())
            {
                regionId = provinceToRegion[provinceId];
            }

            result.regionIds[
                static_cast<size_t>(y) * result.width + x
            ] = regionId;

            if (regionId != 0)
            {
                if (x < result.minX) result.minX = x;
                if (x > result.maxX) result.maxX = x;
                if (y < result.minY) result.minY = y;
                if (y > result.maxY) result.maxY = y;
            }
        }
    }

    if (result.maxX < 0 || result.maxY < 0)
    {
        throw std::runtime_error(
            "selected regions contain no valid provinces");
    }

    return result;
}

uint32_t MakePixel(
    uint8_t red,
    uint8_t green,
    uint8_t blue,
    uint8_t alpha)
{
    return static_cast<uint32_t>(blue)
        | (static_cast<uint32_t>(green) << 8)
        | (static_cast<uint32_t>(red) << 16)
        | (static_cast<uint32_t>(alpha) << 24);
}

void WriteBmp32(
    const fs::path& outputPath,
    int width,
    int height,
    const std::vector<uint32_t>& pixels)
{
    BmpFileHeader fileHeader{};
    BmpInfoHeader infoHeader{};

    constexpr uint32_t headerSize =
        sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) + 16;

    fileHeader.type = 0x4D42;
    fileHeader.pixelOffset = headerSize;
    fileHeader.size = headerSize
        + static_cast<uint32_t>(pixels.size() * sizeof(uint32_t));

    infoHeader.size = sizeof(BmpInfoHeader);
    infoHeader.width = width;
    infoHeader.height = height;
    infoHeader.planes = 1;
    infoHeader.bitsPerPixel = 32;
    infoHeader.compression = 3;
    infoHeader.imageSize = static_cast<uint32_t>(
        pixels.size() * sizeof(uint32_t)
    );

    std::ofstream file(outputPath, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error(
            "cannot create output: "
            + outputPath.string());
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

    for (int y = height - 1; y >= 0; --y)
    {
        const uint32_t* row = pixels.data()
            + static_cast<size_t>(y) * width;

        file.write(
            reinterpret_cast<const char*>(row),
            static_cast<std::streamsize>(
                static_cast<size_t>(width) * sizeof(uint32_t)
            )
        );
    }

    if (!file)
    {
        throw std::runtime_error(
            "failed while writing output BMP");
    }
}

CroppedRegionMap CropRegionMap(
    const ProvinceMap& provinceMap)
{
    constexpr int padding = 16;

    const int left = std::max(0, provinceMap.minX - padding);
    const int top = std::max(0, provinceMap.minY - padding);
    const int right = std::min(
        provinceMap.width - 1,
        provinceMap.maxX + padding
    );
    const int bottom = std::min(
        provinceMap.height - 1,
        provinceMap.maxY + padding
    );

    CroppedRegionMap result;
    result.width = right - left + 1;
    result.height = bottom - top + 1;
    result.regionIds.resize(
        static_cast<size_t>(result.width)
        * static_cast<size_t>(result.height),
        0
    );

    for (int y = 0; y < result.height; ++y)
    {
        for (int x = 0; x < result.width; ++x)
        {
            result.regionIds[
                static_cast<size_t>(y) * result.width + x
            ] = provinceMap.regionIds[
                static_cast<size_t>(top + y)
                * provinceMap.width
                + (left + x)
            ];
        }
    }

    return result;
}

CroppedRegionMap FlipVertical(
    const CroppedRegionMap& source)
{
    CroppedRegionMap result;
    result.width = source.width;
    result.height = source.height;
    result.regionIds.resize(source.regionIds.size(), 0);

    for (int y = 0; y < source.height; ++y)
    {
        const int sourceY = source.height - 1 - y;

        for (int x = 0; x < source.width; ++x)
        {
            result.regionIds[
                static_cast<size_t>(y) * source.width + x
            ] = source.regionIds[
                static_cast<size_t>(sourceY) * source.width + x
            ];
        }
    }

    return result;
}

void WriteRegionIdMap(
    const fs::path& outputPath,
    const CroppedRegionMap& regionMap)
{
    const RegionMapHeader header{
        {'R', 'I', 'D', '1'},
        1,
        static_cast<uint32_t>(regionMap.width),
        static_cast<uint32_t>(regionMap.height),
        1
    };

    std::ofstream file(
        outputPath,
        std::ios::binary
    );

    if (!file)
    {
        throw std::runtime_error(
            "cannot create region id map: "
            + outputPath.string());
    }

    file.write(
        reinterpret_cast<const char*>(&header),
        sizeof(header)
    );

    file.write(
        reinterpret_cast<const char*>(regionMap.regionIds.data()),
        static_cast<std::streamsize>(
            regionMap.regionIds.size() * sizeof(uint16_t)
        )
    );

    if (!file)
    {
        throw std::runtime_error(
            "failed to write region id map");
    }
}

void GenerateMap(
    const ProvinceMap& provinceMap,
    const fs::path& outputPath,
    const fs::path& regionIdOutputPath)
{
    const CroppedRegionMap croppedMap =
        FlipVertical(CropRegionMap(provinceMap));

    const int width = croppedMap.width;
    const int height = croppedMap.height;

    std::vector<uint32_t> pixels(
        static_cast<size_t>(width)
        * static_cast<size_t>(height),
        MakePixel(0, 0, 0, 0)
    );

    const auto regionAt = [&croppedMap](int x, int y) -> uint16_t
    {
        if (x < 0 || y < 0
            || x >= croppedMap.width
            || y >= croppedMap.height)
        {
            return 0;
        }

        return croppedMap.regionIds[
            static_cast<size_t>(y) * croppedMap.width + x
        ];
    };

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const uint16_t regionId = regionAt(x, y);

            if (regionId == 0)
            {
                continue;
            }

            const bool boundary =
                regionAt(x - 1, y) != regionId
                || regionAt(x + 1, y) != regionId
                || regionAt(x, y - 1) != regionId
                || regionAt(x, y + 1) != regionId;

            const uint32_t pixel = boundary
                ? MakePixel(60, 60, 60, 255)
                : MakePixel(218, 218, 210, 255);

            pixels[
                static_cast<size_t>(y) * width + x
            ] = pixel;
        }
    }

    WriteBmp32(
        outputPath,
        width,
        height,
        pixels
    );

    WriteRegionIdMap(
        regionIdOutputPath,
        croppedMap
    );
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        std::ios::sync_with_stdio(false);

        const fs::path root = argc > 1
            ? fs::path(argv[1])
            : fs::path(".");

        const fs::path outputPath = argc > 2
            ? fs::path(argv[2])
            : root / "china_map.bmp";

        const fs::path regionIdOutputPath = argc > 3
            ? fs::path(argv[3])
            : root / "china_region_ids.bin";

        const fs::path definitionPath =
            root / "map" / "definition.csv";
        const fs::path regionPath =
            root / "map" / "region.txt";
        const fs::path provincesPath =
            root / "map" / "provinces.bmp";

        uint32_t maximumProvinceId = 0;

        const auto colorToProvince = LoadColorToProvince(
            definitionPath,
            maximumProvinceId
        );

        const auto provinceToRegion = LoadProvinceToRegion(
            regionPath,
            maximumProvinceId
        );

        const ProvinceMap provinceMap = LoadProvinceMap(
            provincesPath,
            colorToProvince,
            provinceToRegion
        );

        GenerateMap(
            provinceMap,
            outputPath,
            regionIdOutputPath
        );

        std::cout
            << "saved: " << outputPath << '\n'
            << "saved: " << regionIdOutputPath << '\n'
            << "size: " << provinceMap.width
            << "x" << provinceMap.height << '\n';

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
