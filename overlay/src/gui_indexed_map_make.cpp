#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "gui_interpreter.h"

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

struct SourceMap
{
    int width = 0;
    int height = 0;
    std::vector<uint16_t> itemIds;
    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxX = -1;
    int maxY = -1;
};

struct CroppedMap
{
    int width = 0;
    int height = 0;
    std::vector<uint16_t> itemIds;
};

std::string Trim(std::string_view value)
{
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end
        && std::isspace(static_cast<unsigned char>(value[begin])))
    {
        ++begin;
    }
    while (end > begin
        && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

fs::path ResolvePath(const fs::path& root, fs::path path)
{
    std::string normalized = path.string();
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    path = normalized;
    return path.is_absolute() ? path : root / path;
}

uint8_t ToByte(float value)
{
    return static_cast<uint8_t>(std::lround(
        std::clamp(value, 0.0f, 1.0f) * 255.0f
    ));
}

uint32_t MakePixel(const gui::GuiRgbaColor& color)
{
    return static_cast<uint32_t>(ToByte(color.b))
        | (static_cast<uint32_t>(ToByte(color.g)) << 8)
        | (static_cast<uint32_t>(ToByte(color.r)) << 16)
        | (static_cast<uint32_t>(ToByte(color.a)) << 24);
}

bool ParseDefinitionRow(
    const std::string& line,
    std::array<int, 4>& values
)
{
    std::size_t position = 0;
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
            value = value * 10 + line[position] - '0';
            ++position;
        }
        values[count++] = value;
    }
    return count == 4;
}

std::vector<uint16_t> LoadColorToProvince(
    const fs::path& path,
    uint32_t& maximumProvinceId
)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error(
            "cannot open source definition: " + path.string()
        );
    }

    std::vector<uint16_t> colorToProvince(kColorLookupSize, 0);
    maximumProvinceId = 0;
    std::string line;
    while (std::getline(file, line))
    {
        std::array<int, 4> values{};
        if (!ParseDefinitionRow(line, values))
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
        colorToProvince[color] = static_cast<uint16_t>(provinceId);
        maximumProvinceId = std::max(
            maximumProvinceId,
            static_cast<uint32_t>(provinceId)
        );
    }
    return colorToProvince;
}

std::unordered_map<std::string, uint16_t> BuildSourceItemLookup(
    const gui::IndexedMapResource& resource
)
{
    std::unordered_map<std::string, uint16_t> lookup;
    std::unordered_set<uint16_t> ids;
    for (const gui::IndexedMapSourceItem& item : resource.sourceItems)
    {
        if (item.id == 0
            || item.name.empty()
            || !ids.insert(item.id).second
            || !lookup.emplace(item.name, item.id).second)
        {
            throw std::runtime_error(
                "indexed map source items contain duplicate or invalid IDs"
            );
        }
    }
    if (lookup.empty())
    {
        throw std::runtime_error("indexed map source items are empty");
    }
    return lookup;
}

void AssignProvinceIds(
    std::string_view text,
    uint16_t itemId,
    std::vector<uint16_t>& provinceToItem
)
{
    std::size_t position = 0;
    while (position < text.size())
    {
        while (position < text.size()
            && !std::isdigit(static_cast<unsigned char>(text[position])))
        {
            ++position;
        }
        if (position == text.size())
        {
            break;
        }

        uint32_t provinceId = 0;
        while (position < text.size()
            && std::isdigit(static_cast<unsigned char>(text[position])))
        {
            provinceId = provinceId * 10 + text[position] - '0';
            ++position;
        }
        if (provinceId < provinceToItem.size()
            && provinceToItem[provinceId] == 0)
        {
            provinceToItem[provinceId] = itemId;
        }
    }
}

std::vector<uint16_t> LoadProvinceToItem(
    const fs::path& path,
    const gui::IndexedMapResource& resource,
    uint32_t maximumProvinceId
)
{
    const auto itemLookup = BuildSourceItemLookup(resource);
    std::unordered_set<std::string> foundItems;
    std::vector<uint16_t> provinceToItem(maximumProvinceId + 1, 0);
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error(
            "cannot open source group file: " + path.string()
        );
    }

    uint16_t currentItemId = 0;
    std::string line;
    while (std::getline(file, line))
    {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos)
        {
            line.resize(comment);
        }
        const std::string trimmed = Trim(line);
        if (trimmed.empty())
        {
            continue;
        }

        const std::size_t equals = trimmed.find('=');
        const std::size_t opening = trimmed.find('{');
        if (equals != std::string::npos
            && opening != std::string::npos
            && equals < opening)
        {
            const std::string name = Trim(
                std::string_view(trimmed).substr(0, equals)
            );
            const auto item = itemLookup.find(name);
            currentItemId = item == itemLookup.end() ? 0 : item->second;
            if (currentItemId != 0)
            {
                foundItems.insert(name);
                AssignProvinceIds(
                    std::string_view(trimmed).substr(opening + 1),
                    currentItemId,
                    provinceToItem
                );
            }
            if (trimmed.find('}', opening + 1) != std::string::npos)
            {
                currentItemId = 0;
            }
            continue;
        }

        if (currentItemId != 0)
        {
            AssignProvinceIds(trimmed, currentItemId, provinceToItem);
        }
        if (trimmed.find('}') != std::string::npos)
        {
            currentItemId = 0;
        }
    }

    if (foundItems.size() != itemLookup.size())
    {
        for (const auto& item : itemLookup)
        {
            if (foundItems.find(item.first) == foundItems.end())
            {
                throw std::runtime_error(
                    "source item not found in group file: " + item.first
                );
            }
        }
    }
    return provinceToItem;
}

SourceMap LoadSourceMap(
    const fs::path& path,
    const std::vector<uint16_t>& colorToProvince,
    const std::vector<uint16_t>& provinceToItem
)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error(
            "cannot open source province map: " + path.string()
        );
    }

    BmpFileHeader fileHeader{};
    BmpInfoHeader infoHeader{};
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    file.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));
    if (!file
        || fileHeader.type != 0x4D42
        || infoHeader.bitsPerPixel != 24
        || infoHeader.compression != 0
        || infoHeader.width <= 0
        || infoHeader.height == 0)
    {
        throw std::runtime_error(
            "source province map must be an uncompressed 24-bit BMP"
        );
    }

    SourceMap result;
    result.width = infoHeader.width;
    result.height = std::abs(infoHeader.height);
    result.itemIds.assign(
        static_cast<std::size_t>(result.width) * result.height,
        0
    );
    const std::size_t rowStride =
        (static_cast<std::size_t>(result.width) * 3 + 3)
        & ~static_cast<std::size_t>(3);
    std::vector<uint8_t> row(rowStride);
    const bool bottomUp = infoHeader.height > 0;
    file.seekg(fileHeader.pixelOffset, std::ios::beg);

    for (int sourceY = 0; sourceY < result.height; ++sourceY)
    {
        file.read(
            reinterpret_cast<char*>(row.data()),
            static_cast<std::streamsize>(row.size())
        );
        if (!file)
        {
            throw std::runtime_error("source province map is truncated");
        }
        const int y = bottomUp ? result.height - 1 - sourceY : sourceY;
        for (int x = 0; x < result.width; ++x)
        {
            const std::size_t offset = static_cast<std::size_t>(x) * 3;
            const uint32_t color =
                (static_cast<uint32_t>(row[offset + 2]) << 16)
                | (static_cast<uint32_t>(row[offset + 1]) << 8)
                | static_cast<uint32_t>(row[offset]);
            const uint16_t provinceId = colorToProvince[color];
            const uint16_t itemId = provinceId < provinceToItem.size()
                ? provinceToItem[provinceId]
                : 0;
            result.itemIds[static_cast<std::size_t>(y) * result.width + x] =
                itemId;
            if (itemId != 0)
            {
                result.minX = std::min(result.minX, x);
                result.maxX = std::max(result.maxX, x);
                result.minY = std::min(result.minY, y);
                result.maxY = std::max(result.maxY, y);
            }
        }
    }
    if (result.maxX < 0 || result.maxY < 0)
    {
        throw std::runtime_error("source items contain no mapped pixels");
    }
    return result;
}

CroppedMap CropMap(const SourceMap& source, int padding)
{
    const int left = std::max(0, source.minX - padding);
    const int top = std::max(0, source.minY - padding);
    const int right = std::min(source.width - 1, source.maxX + padding);
    const int bottom = std::min(source.height - 1, source.maxY + padding);
    CroppedMap result;
    result.width = right - left + 1;
    result.height = bottom - top + 1;
    result.itemIds.resize(
        static_cast<std::size_t>(result.width) * result.height,
        0
    );
    for (int y = 0; y < result.height; ++y)
    {
        for (int x = 0; x < result.width; ++x)
        {
            result.itemIds[static_cast<std::size_t>(y) * result.width + x] =
                source.itemIds[
                    static_cast<std::size_t>(top + y) * source.width
                    + left + x
                ];
        }
    }
    return result;
}

void FlipVertical(CroppedMap& map)
{
    for (int y = 0; y < map.height / 2; ++y)
    {
        const int otherY = map.height - 1 - y;
        for (int x = 0; x < map.width; ++x)
        {
            std::swap(
                map.itemIds[static_cast<std::size_t>(y) * map.width + x],
                map.itemIds[
                    static_cast<std::size_t>(otherY) * map.width + x
                ]
            );
        }
    }
}

void WriteBmp32(
    const fs::path& path,
    int width,
    int height,
    const std::vector<uint32_t>& pixels
)
{
    fs::create_directories(path.parent_path());
    constexpr uint32_t headerSize =
        sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) + 16;
    BmpFileHeader fileHeader{};
    fileHeader.type = 0x4D42;
    fileHeader.pixelOffset = headerSize;
    fileHeader.size = headerSize
        + static_cast<uint32_t>(pixels.size() * sizeof(uint32_t));
    BmpInfoHeader infoHeader{};
    infoHeader.size = sizeof(BmpInfoHeader);
    infoHeader.width = width;
    infoHeader.height = height;
    infoHeader.planes = 1;
    infoHeader.bitsPerPixel = 32;
    infoHeader.compression = 3;
    infoHeader.imageSize = static_cast<uint32_t>(
        pixels.size() * sizeof(uint32_t)
    );

    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("cannot create texture: " + path.string());
    }
    file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    file.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));
    const uint32_t masks[] = {
        0x00FF0000,
        0x0000FF00,
        0x000000FF,
        0xFF000000
    };
    file.write(reinterpret_cast<const char*>(masks), sizeof(masks));
    for (int y = height - 1; y >= 0; --y)
    {
        const uint32_t* row = pixels.data()
            + static_cast<std::size_t>(y) * width;
        file.write(
            reinterpret_cast<const char*>(row),
            static_cast<std::streamsize>(
                static_cast<std::size_t>(width) * sizeof(uint32_t)
            )
        );
    }
    if (!file)
    {
        throw std::runtime_error("failed to write texture: " + path.string());
    }
}

void WriteIndex(const fs::path& path, const CroppedMap& map)
{
    fs::create_directories(path.parent_path());
    const IndexedMapHeader header{
        {'I', 'D', 'X', '1'},
        1,
        static_cast<uint32_t>(map.width),
        static_cast<uint32_t>(map.height),
        1
    };
    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("cannot create index: " + path.string());
    }
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(
        reinterpret_cast<const char*>(map.itemIds.data()),
        static_cast<std::streamsize>(
            map.itemIds.size() * sizeof(uint16_t)
        )
    );
    if (!file)
    {
        throw std::runtime_error("failed to write index: " + path.string());
    }
}

void GenerateAssets(
    const gui::IndexedMapResource& resource,
    const SourceMap& source,
    const fs::path& texturePath,
    const fs::path& indexPath
)
{
    CroppedMap map = CropMap(source, resource.cropPadding);
    if (resource.flipVertical)
    {
        FlipVertical(map);
    }

    const uint32_t fillPixel = MakePixel(resource.sourceFillColor);
    const uint32_t boundaryPixel = MakePixel(resource.sourceBoundaryColor);
    std::vector<uint32_t> pixels(
        static_cast<std::size_t>(map.width) * map.height,
        0
    );
    const auto itemAt = [&map](int x, int y)
    {
        if (x < 0 || y < 0 || x >= map.width || y >= map.height)
        {
            return static_cast<uint16_t>(0);
        }
        return map.itemIds[static_cast<std::size_t>(y) * map.width + x];
    };
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const uint16_t itemId = itemAt(x, y);
            if (itemId == 0)
            {
                continue;
            }
            const bool boundary = itemAt(x - 1, y) != itemId
                || itemAt(x + 1, y) != itemId
                || itemAt(x, y - 1) != itemId
                || itemAt(x, y + 1) != itemId;
            pixels[static_cast<std::size_t>(y) * map.width + x] =
                boundary ? boundaryPixel : fillPixel;
        }
    }
    WriteBmp32(texturePath, map.width, map.height, pixels);
    WriteIndex(indexPath, map);
    std::cout << "resource: " << resource.name << '\n'
              << "items: " << resource.sourceItems.size() << '\n'
              << "size: " << map.width << 'x' << map.height << '\n'
              << "texture: " << texturePath << '\n'
              << "index: " << indexPath << '\n';
}

const gui::IndexedMapResource& SelectResource(
    const gui::GuiInterpreter& interpreter,
    std::string_view requestedName
)
{
    if (!requestedName.empty())
    {
        const gui::IndexedMapResource* resource =
            interpreter.FindIndexedMap(std::string(requestedName));
        if (!resource)
        {
            throw std::runtime_error(
                "indexed map resource not found: "
                + std::string(requestedName)
            );
        }
        return *resource;
    }

    const gui::IndexedMapResource* selected = nullptr;
    for (const auto& entry : interpreter.IndexedMaps())
    {
        if (entry.second.sourceItems.empty())
        {
            continue;
        }
        if (selected)
        {
            throw std::runtime_error(
                "multiple buildable indexed maps; specify a resource name"
            );
        }
        selected = &entry.second;
    }
    if (!selected)
    {
        throw std::runtime_error("no buildable indexed map resource found");
    }
    return *selected;
}

}

int main(int argc, char** argv)
{
    try
    {
        const fs::path root = argc >= 2 ? fs::path(argv[1]) : fs::path(".");
        const std::string resourceName = argc >= 3 ? argv[2] : std::string{};
        gui::GuiInterpreter interpreter;
        std::string error;
        if (!interpreter.LoadDirectory(root / "interface", error))
        {
            throw std::runtime_error(error);
        }
        const gui::IndexedMapResource& resource = SelectResource(
            interpreter,
            resourceName
        );
        if (resource.sourceDefinitionFile.empty()
            || resource.sourceProvinceFile.empty()
            || resource.sourceGroupFile.empty()
            || resource.textureFile.empty()
            || resource.indexFile.empty())
        {
            throw std::runtime_error(
                "indexed map build paths are incomplete: " + resource.name
            );
        }

        uint32_t maximumProvinceId = 0;
        const std::vector<uint16_t> colorToProvince = LoadColorToProvince(
            ResolvePath(root, resource.sourceDefinitionFile),
            maximumProvinceId
        );
        const std::vector<uint16_t> provinceToItem = LoadProvinceToItem(
            ResolvePath(root, resource.sourceGroupFile),
            resource,
            maximumProvinceId
        );
        const SourceMap source = LoadSourceMap(
            ResolvePath(root, resource.sourceProvinceFile),
            colorToProvince,
            provinceToItem
        );
        GenerateAssets(
            resource,
            source,
            interpreter.ResolveIndexedMapTexture(resource.name, root),
            interpreter.ResolveIndexedMapIndex(resource.name, root)
        );
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "error: " << exception.what() << '\n';
        return 1;
    }
}
