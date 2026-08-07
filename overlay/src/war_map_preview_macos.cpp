#include <SDL.h>

#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "region_color_core.h"
#include "war_map_input.h"
#include "war_map_region_info.h"
#include "war_map_regions.h"
#include "war_map_state.h"

namespace fs = std::filesystem;

namespace
{

constexpr int kInitialWindowWidth = 1200;
constexpr int kInitialWindowHeight = 800;
constexpr int kPanelWidth = 320;
constexpr int kPanelPadding = 20;
constexpr int kInfoCardHeight = 300;
constexpr int kBannerHeight = 72;
constexpr int kBannerTop = 20;
constexpr int kSkinButtonTop = 410;
constexpr int kSkinButtonHeight = 48;
constexpr Uint32 kStateUpdateIntervalMs = 120;

struct RgbColor
{
    Uint8 r;
    Uint8 g;
    Uint8 b;
};

struct UiImageSet
{
    SDL_Texture* sidebar = nullptr;
    SDL_Texture* buttonNormal = nullptr;
    SDL_Texture* buttonPressed = nullptr;
    SDL_Texture* border = nullptr;
};

struct Glyph
{
    char character;
    std::array<const char*, 7> rows;
};

const std::array<Glyph, 41> kGlyphs = {{
    {'A', {" ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"}},
    {'B', {"#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### "}},
    {'C', {" ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### "}},
    {'D', {"#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### "}},
    {'E', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####"}},
    {'F', {"#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    "}},
    {'G', {" ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ### "}},
    {'H', {"#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #"}},
    {'I', {"#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "#####"}},
    {'J', {"#####", "    #", "    #", "    #", "    #", "#   #", " ### "}},
    {'K', {"#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #"}},
    {'L', {"#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####"}},
    {'M', {"#   #", "## ##", "# # #", "#   #", "#   #", "#   #", "#   #"}},
    {'N', {"#   #", "##  #", "##  #", "# # #", "#  ##", "#  ##", "#   #"}},
    {'O', {" ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "}},
    {'P', {"#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    "}},
    {'Q', {" ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #"}},
    {'R', {"#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #"}},
    {'S', {" ####", "#    ", "#    ", " ### ", "    #", "    #", "#### "}},
    {'T', {"#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  "}},
    {'U', {"#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### "}},
    {'V', {"#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  "}},
    {'W', {"#   #", "#   #", "#   #", "# # #", "# # #", "## ##", "#   #"}},
    {'X', {"#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #"}},
    {'Y', {"#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  "}},
    {'Z', {"#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####"}},
    {'0', {" ### ", "#  ##", "# # #", "##  #", "#   #", "#   #", " ### "}},
    {'1', {"  #  ", " ##  ", "# #  ", "  #  ", "  #  ", "  #  ", "#####"}},
    {'2', {" ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####"}},
    {'3', {"#### ", "    #", "    #", " ### ", "    #", "    #", "#### "}},
    {'4', {"   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # "}},
    {'5', {"#####", "#    ", "#    ", "#### ", "    #", "    #", "#### "}},
    {'6', {" ### ", "#    ", "#    ", "#### ", "#   #", "#   #", " ### "}},
    {'7', {"#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#    "}},
    {'8', {" ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### "}},
    {'9', {" ### ", "#   #", "#   #", " ####", "    #", "    #", " ### "}},
    {'_', {"     ", "     ", "     ", "     ", "     ", "     ", "#####"}},
    {':', {"     ", "  #  ", "     ", "     ", "  #  ", "     ", "     "}},
    {'%', {"#   #", "   # ", "  #  ", " #   ", "#   #", "     ", "     "}},
    {'.', {"     ", "     ", "     ", "     ", "     ", "  #  ", "     "}},
    {' ', {"     ", "     ", "     ", "     ", "     ", "     ", "     "}}
}};

const Glyph* FindGlyph(char character)
{
    for (const Glyph& glyph : kGlyphs)
    {
        if (glyph.character == character)
        {
            return &glyph;
        }
    }

    return nullptr;
}

std::string ToUpperAscii(std::string value)
{
    for (char& character : value)
    {
        if (character >= 'a' && character <= 'z')
        {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }

    return value;
}

void DrawText(
    SDL_Renderer* renderer,
    int x,
    int y,
    const std::string& text,
    int scale,
    RgbColor color
)
{
    SDL_SetRenderDrawColor(
        renderer,
        color.r,
        color.g,
        color.b,
        SDL_ALPHA_OPAQUE
    );

    int cursorX = x;

    for (char character : ToUpperAscii(text))
    {
        const Glyph* glyph = FindGlyph(character);

        if (!glyph)
        {
            cursorX += 6 * scale;
            continue;
        }

        for (int row = 0; row < 7; ++row)
        {
            for (int column = 0; column < 5; ++column)
            {
                if (glyph->rows[row][column] != ' ')
                {
                    SDL_Rect pixel{
                        cursorX + column * scale,
                        y + row * scale,
                        scale,
                        scale
                    };

                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }

        cursorX += 6 * scale;
    }
}

SDL_Texture* LoadBitmapTexture(
    SDL_Renderer* renderer,
    const fs::path& path,
    int& width,
    int& height
)
{
    SDL_Surface* surface = SDL_LoadBMP(path.string().c_str());

    if (!surface)
    {
        return nullptr;
    }

    width = surface->w;
    height = surface->h;

    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);
    return texture;
}

SDL_Texture* LoadPngTexture(
    SDL_Renderer* renderer,
    const fs::path& path
)
{
    const std::string pathString = path.string();

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(pathString.c_str()),
        pathString.size(),
        false
    );

    if (!url)
    {
        return nullptr;
    }

    CGImageSourceRef imageSource =
        CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);

    if (!imageSource)
    {
        return nullptr;
    }

    CGImageRef image = CGImageSourceCreateImageAtIndex(
        imageSource,
        0,
        nullptr
    );
    CFRelease(imageSource);

    if (!image)
    {
        return nullptr;
    }

    const size_t width = CGImageGetWidth(image);
    const size_t height = CGImageGetHeight(image);

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0,
        static_cast<int>(width),
        static_cast<int>(height),
        32,
        SDL_PIXELFORMAT_RGBA32
    );

    if (!surface)
    {
        CGImageRelease(image);
        return nullptr;
    }

    CGColorSpaceRef colorSpace =
        CGColorSpaceCreateDeviceRGB();

    CGContextRef context = CGBitmapContextCreate(
        surface->pixels,
        width,
        height,
        8,
        surface->pitch,
        colorSpace,
        kCGImageAlphaPremultipliedLast
            | kCGBitmapByteOrder32Big
    );

    CGColorSpaceRelease(colorSpace);

    if (!context)
    {
        SDL_FreeSurface(surface);
        CGImageRelease(image);
        return nullptr;
    }

    CGContextTranslateCTM(
        context,
        0,
        static_cast<CGFloat>(height)
    );
    CGContextScaleCTM(context, 1.0, -1.0);
    CGContextDrawImage(
        context,
        CGRectMake(
            0,
            0,
            static_cast<CGFloat>(width),
            static_cast<CGFloat>(height)
        ),
        image
    );

    CGContextRelease(context);
    CGImageRelease(image);

    SDL_Texture* texture = SDL_CreateTextureFromSurface(
        renderer,
        surface
    );

    SDL_FreeSurface(surface);

    if (texture)
    {
        SDL_SetTextureBlendMode(
            texture,
            SDL_BLENDMODE_BLEND
        );
    }

    return texture;
}

bool LoadUiImageSet(
    SDL_Renderer* renderer,
    const fs::path& root,
    UiImageSet& output
)
{
    bool complete = true;
    const fs::path imageRoot =
        root / "gui" / "chinawar";

    const auto load = [
        renderer,
        &imageRoot,
        &complete
    ](
        const char* fileName,
        SDL_Texture*& target
    )
    {
        target = LoadPngTexture(
            renderer,
            imageRoot / fileName
        );

        if (!target)
        {
            complete = false;
            std::cerr << "Failed to load UI image: "
                      << (imageRoot / fileName) << '\n';
        }
    };

    load("sidebar.png", output.sidebar);
    load("button_normal.png", output.buttonNormal);
    load("button_pressed.png", output.buttonPressed);
    load("border.png", output.border);

    return complete;
}

void DestroyUiImageSet(UiImageSet& images)
{
    SDL_DestroyTexture(images.sidebar);
    SDL_DestroyTexture(images.buttonNormal);
    SDL_DestroyTexture(images.buttonPressed);
    SDL_DestroyTexture(images.border);
    images = {};
}

SDL_Rect GetSkinButtonRect(
    int windowWidth,
    int windowHeight
)
{
    return {
        windowWidth - kPanelWidth + kPanelPadding,
        std::min(
            kSkinButtonTop,
            std::max(0, windowHeight - kSkinButtonHeight - kPanelPadding)
        ),
        kPanelWidth - 2 * kPanelPadding,
        kSkinButtonHeight
    };
}

bool IsPointInside(
    const SDL_Rect& rect,
    int x,
    int y
)
{
    return x >= rect.x
        && x < rect.x + rect.w
        && y >= rect.y
        && y < rect.y + rect.h;
}

void DrawUiImage(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const SDL_Rect& destination
)
{
    if (texture)
    {
        SDL_RenderCopy(
            renderer,
            texture,
            nullptr,
            &destination
        );
    }
}

void DrawWindowBorder(
    SDL_Renderer* renderer,
    int windowWidth,
    int windowHeight,
    const UiImageSet& uiImages
)
{
    const SDL_Rect windowRect{
        0,
        0,
        windowWidth,
        windowHeight
    };

    DrawUiImage(
        renderer,
        uiImages.border,
        windowRect
    );
}

MapRect CalculateMapRect(
    int windowWidth,
    int windowHeight,
    int mapWidth,
    int mapHeight
)
{
    const int viewportWidth =
        std::max(1, windowWidth - kPanelWidth);

    const double widthScale =
        static_cast<double>(viewportWidth - 2 * kPanelPadding)
        / static_cast<double>(mapWidth);

    const double heightScale =
        static_cast<double>(windowHeight - 2 * kPanelPadding)
        / static_cast<double>(mapHeight);

    const double scale =
        std::max(0.01, std::min(widthScale, heightScale));

    const int displayWidth =
        std::max(1, static_cast<int>(mapWidth * scale));
    const int displayHeight =
        std::max(1, static_cast<int>(mapHeight * scale));

    return {
        (viewportWidth - displayWidth) / 2,
        (windowHeight - displayHeight) / 2,
        displayWidth,
        displayHeight
    };
}

class SimulatedStateSource final
    : public IWarMapStateSource
{
public:
    bool Read(WarMapState& output) override
    {
        const double seconds =
            static_cast<double>(SDL_GetTicks()) / 1000.0;

        output.visible = true;
        output.active = true;
        output.date = static_cast<int64_t>(seconds);
        output.viewerTag = "CHI";
        output.controlledPercentages.fill(0.0f);

        for (size_t regionId = 1;
             regionId <= kWarMapRegionCount;
             ++regionId)
        {
            const double phase =
                std::fmod(seconds * 18.0
                    + static_cast<double>(regionId) * 7.0,
                    101.0);

            output.controlledPercentages[regionId] =
                static_cast<float>(phase);
        }

        return true;
    }
};

std::string FormatPercentage(float percentage)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << percentage << "%";
    return stream.str();
}

std::string FormatPopulationValue(
    bool known,
    uint32_t value,
    const std::string& prefix
)
{
    if (!known)
    {
        return prefix + "数据待补";
    }

    std::ostringstream stream;

    if (value >= 10000)
    {
        stream << std::fixed << std::setprecision(1)
               << static_cast<double>(value) / 10000.0
               << " 万";
    }
    else
    {
        stream << value;
    }

    return prefix + stream.str();
}

void DrawCoreTextLine(
    CGContextRef context,
    int canvasHeight,
    const std::string& text,
    CGFloat x,
    CGFloat topY,
    CGFloat fontSize,
    CGFloat red,
    CGFloat green,
    CGFloat blue
)
{
    CFStringRef cfText = CFStringCreateWithCString(
        kCFAllocatorDefault,
        text.c_str(),
        kCFStringEncodingUTF8
    );

    if (!cfText)
    {
        return;
    }

    CTFontRef font = CTFontCreateWithName(
        CFSTR("PingFang SC"),
        fontSize,
        nullptr
    );

    if (!font)
    {
        CFRelease(cfText);
        return;
    }

    CGColorRef textColor = CGColorCreateGenericRGB(
        red,
        green,
        blue,
        1.0
    );

    const void* keys[] = {
        kCTFontAttributeName,
        kCTForegroundColorAttributeName
    };
    const void* values[] = {
        font,
        textColor
    };

    CFDictionaryRef attributes = CFDictionaryCreate(
        kCFAllocatorDefault,
        keys,
        values,
        2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );

    CFAttributedStringRef attributedString =
        CFAttributedStringCreate(
            kCFAllocatorDefault,
            cfText,
            attributes
        );

    CTLineRef line = attributedString
        ? CTLineCreateWithAttributedString(attributedString)
        : nullptr;

    if (line)
    {
        CGContextSetRGBFillColor(
            context,
            red,
            green,
            blue,
            1.0
        );

        CGContextSetTextPosition(
            context,
            x,
            static_cast<CGFloat>(canvasHeight)
                - topY
                - fontSize
        );

        CTLineDraw(line, context);
        CFRelease(line);
    }

    if (attributedString)
    {
        CFRelease(attributedString);
    }

    if (attributes)
    {
        CFRelease(attributes);
    }

    CGColorRelease(textColor);
    CFRelease(font);
    CFRelease(cfText);
}

CGFloat MeasureCoreTextLine(
    const std::string& text,
    CGFloat fontSize
)
{
    CFStringRef cfText = CFStringCreateWithCString(
        kCFAllocatorDefault,
        text.c_str(),
        kCFStringEncodingUTF8
    );

    if (!cfText)
    {
        return 0.0;
    }

    CTFontRef font = CTFontCreateWithName(
        CFSTR("PingFang SC"),
        fontSize,
        nullptr
    );

    if (!font)
    {
        CFRelease(cfText);
        return 0.0;
    }

    const void* keys[] = {
        kCTFontAttributeName
    };
    const void* values[] = {
        font
    };

    CFDictionaryRef attributes = CFDictionaryCreate(
        kCFAllocatorDefault,
        keys,
        values,
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );

    CFAttributedStringRef attributedString =
        CFAttributedStringCreate(
            kCFAllocatorDefault,
            cfText,
            attributes
        );

    CTLineRef line = attributedString
        ? CTLineCreateWithAttributedString(attributedString)
        : nullptr;

    CGFloat width = 0.0;

    if (line)
    {
        double ascent = 0.0;
        double descent = 0.0;
        double leading = 0.0;
        width = static_cast<CGFloat>(
            CTLineGetTypographicBounds(
                line,
                &ascent,
                &descent,
                &leading
            )
        );
        CFRelease(line);
    }

    if (attributedString)
    {
        CFRelease(attributedString);
    }

    if (attributes)
    {
        CFRelease(attributes);
    }

    CFRelease(font);
    CFRelease(cfText);

    return width;
}

void DrawBannerText(
    CGContextRef context,
    int canvasHeight,
    const std::string& text,
    CGFloat x,
    CGFloat topY,
    CGFloat fontSize,
    CGFloat red,
    CGFloat green,
    CGFloat blue
)
{
    DrawCoreTextLine(
        context,
        canvasHeight,
        text,
        x + 2.0,
        topY + 2.0,
        fontSize,
        0.02,
        0.03,
        0.04
    );

    DrawCoreTextLine(
        context,
        canvasHeight,
        text,
        x,
        topY,
        fontSize,
        red,
        green,
        blue
    );
}

void UpdateWarBannerTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const WarMapState& state,
    int width
)
{
    pixels.assign(
        static_cast<size_t>(width)
            * kBannerHeight
            * 4,
        0
    );

    CGColorSpaceRef colorSpace =
        CGColorSpaceCreateDeviceRGB();

    CGContextRef context = CGBitmapContextCreate(
        pixels.data(),
        width,
        kBannerHeight,
        8,
        width * 4,
        colorSpace,
        kCGImageAlphaPremultipliedLast
            | kCGBitmapByteOrder32Big
    );

    CGColorSpaceRelease(colorSpace);

    if (!context)
    {
        return;
    }

    const CGFloat centerX =
        static_cast<CGFloat>(width) / 2.0;
    const CGFloat smallFontSize = 15.0;
    const CGFloat largeFontSize = 30.0;
    const CGFloat textGap = 18.0;

    if (state.viewerTag == "JAP")
    {
        const std::string title = "暴支膺惩";
        const CGFloat titleWidth = MeasureCoreTextLine(
            title,
            largeFontSize
        );

        DrawBannerText(
            context,
            kBannerHeight,
            title,
            centerX - titleWidth / 2.0,
            14.0,
            largeFontSize,
            0.95,
            0.78,
            0.28
        );
    }
    else
    {
        const std::string leftText = "打倒日本帝国主义";
        const std::string centerText = "中国人民抗日战争";
        const std::string rightText = "万众一心誓灭倭寇";

        const CGFloat leftWidth = MeasureCoreTextLine(
            leftText,
            smallFontSize
        );
        const CGFloat centerWidth = MeasureCoreTextLine(
            centerText,
            largeFontSize
        );
        const CGFloat rightWidth = MeasureCoreTextLine(
            rightText,
            smallFontSize
        );

        const CGFloat totalWidth =
            leftWidth + textGap
            + centerWidth + textGap
            + rightWidth;
        const CGFloat startX =
            centerX - totalWidth / 2.0;

        DrawBannerText(
            context,
            kBannerHeight,
            leftText,
            startX,
            25.0,
            smallFontSize,
            0.84,
            0.87,
            0.92
        );

        DrawBannerText(
            context,
            kBannerHeight,
            centerText,
            startX + leftWidth + textGap,
            12.0,
            largeFontSize,
            0.95,
            0.78,
            0.28
        );

        DrawBannerText(
            context,
            kBannerHeight,
            rightText,
            startX + leftWidth + textGap
                + centerWidth + textGap,
            25.0,
            smallFontSize,
            0.84,
            0.87,
            0.92
        );
    }

    CGContextRelease(context);

    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        width * 4
    );
}

void UpdateInfoCardTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const WarMapState& state,
    uint16_t selectedRegionId
)
{
    pixels.assign(
        static_cast<size_t>(kPanelWidth)
            * kInfoCardHeight
            * 4,
        0
    );

    CGColorSpaceRef colorSpace =
        CGColorSpaceCreateDeviceRGB();

    CGContextRef context = CGBitmapContextCreate(
        pixels.data(),
        kPanelWidth,
        kInfoCardHeight,
        8,
        kPanelWidth * 4,
        colorSpace,
        kCGImageAlphaPremultipliedLast
            | kCGBitmapByteOrder32Big
    );

    CGColorSpaceRelease(colorSpace);

    if (!context)
    {
        return;
    }

    CGContextSetRGBFillColor(
        context,
        0.098,
        0.114,
        0.141,
        0.20
    );
    CGContextFillRect(
        context,
        CGRectMake(0, 0, kPanelWidth, kInfoCardHeight)
    );

    const int textX = kPanelPadding;

    if (selectedRegionId == 0
        || selectedRegionId > kWarMapRegionNames.size())
    {
        DrawCoreTextLine(
            context,
            kInfoCardHeight,
            "悬停或点击一个区域",
            textX,
            25,
            20,
            0.67,
            0.71,
            0.77
        );

        CGContextRelease(context);

        SDL_UpdateTexture(
            texture,
            nullptr,
            pixels.data(),
            kPanelWidth * 4
        );

        return;
    }

    const std::string regionId =
        std::string(kWarMapRegionNames[selectedRegionId - 1]);
    const RegionStaticInfo staticInfo =
        GetRegionStaticInfo(regionId);

    const bool isJapanViewer =
        state.viewerTag == "JAP";

    const std::string percentageLabel =
        isJapanViewer ? "控制程度：" : "沦陷程度：";

    const RegionPopulationState& population =
        state.populations[selectedRegionId];

    DrawCoreTextLine(
        context,
        kInfoCardHeight,
        std::string(staticInfo.chineseName),
        textX,
        20,
        26,
        0.92,
        0.92,
        0.92
    );

    DrawCoreTextLine(
        context,
        kInfoCardHeight,
        "省会：" + std::string(staticInfo.capitalName),
        textX,
        70,
        18,
        0.70,
        0.74,
        0.81
    );

    DrawCoreTextLine(
        context,
        kInfoCardHeight,
        FormatPopulationValue(
            population.known,
            population.total,
            "总人口："
        ),
        textX,
        115,
        18,
        0.82,
        0.84,
        0.88
    );

    DrawCoreTextLine(
        context,
        kInfoCardHeight,
        FormatPopulationValue(
            population.known,
            population.affected,
            isJapanViewer ? "控制人口：" : "沦陷人口："
        ),
        textX,
        155,
        18,
        0.82,
        0.84,
        0.88
    );

    DrawCoreTextLine(
        context,
        kInfoCardHeight,
        percentageLabel
            + FormatPercentage(
                state.controlledPercentages[selectedRegionId]
            ),
        textX,
        195,
        20,
        0.98,
        0.82,
        0.38
    );

    DrawCoreTextLine(
        context,
        kInfoCardHeight,
        FormatPopulationValue(
            population.known,
            population.remaining,
            "剩余人口："
        ),
        textX,
        240,
        18,
        0.70,
        0.74,
        0.81
    );

    CGContextRelease(context);

    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        kPanelWidth * 4
    );
}

void UpdateOverlayTexture(
    SDL_Texture* texture,
    const std::vector<RgbaPixel>& pixels,
    int width
)
{
    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        width * static_cast<int>(sizeof(RgbaPixel))
    );
}

void UpdateOverlayTextureRegion(
    SDL_Texture* texture,
    const std::vector<RgbaPixel>& pixels,
    int mapWidth,
    const RegionBounds& bounds
)
{
    if (!bounds.valid)
    {
        return;
    }

    const int regionWidth =
        static_cast<int>(bounds.maxX - bounds.minX + 1);
    const int regionHeight =
        static_cast<int>(bounds.maxY - bounds.minY + 1);

    const SDL_Rect dirtyRect{
        static_cast<int>(bounds.minX),
        static_cast<int>(bounds.minY),
        regionWidth,
        regionHeight
    };

    const size_t firstPixel =
        static_cast<size_t>(bounds.minY) * mapWidth
        + bounds.minX;

    SDL_UpdateTexture(
        texture,
        &dirtyRect,
        pixels.data() + firstPixel,
        mapWidth * static_cast<int>(sizeof(RgbaPixel))
    );
}

void DrawInfoPanel(
    SDL_Renderer* renderer,
    int windowWidth,
    int windowHeight,
    SDL_Texture* infoTexture,
    const UiImageSet& uiImages,
    bool buttonPressed
)
{
    const int panelX = windowWidth - kPanelWidth;

    SDL_SetRenderDrawColor(renderer, 25, 29, 36, SDL_ALPHA_OPAQUE);
    SDL_Rect panel{panelX, 0, kPanelWidth, windowHeight};
    SDL_RenderFillRect(renderer, &panel);

    DrawUiImage(
        renderer,
        uiImages.sidebar,
        panel
    );

    SDL_SetRenderDrawColor(renderer, 70, 78, 92, SDL_ALPHA_OPAQUE);
    SDL_Rect separator{panelX, 0, 2, windowHeight};
    SDL_RenderFillRect(renderer, &separator);

    SDL_Rect cardRect{
        panelX,
        90,
        kPanelWidth,
        kInfoCardHeight
    };

    SDL_RenderCopy(
        renderer,
        infoTexture,
        nullptr,
        &cardRect
    );

    const SDL_Rect buttonRect = GetSkinButtonRect(
        windowWidth,
        windowHeight
    );

    DrawUiImage(
        renderer,
        buttonPressed
            ? uiImages.buttonPressed
            : uiImages.buttonNormal,
        buttonRect
    );

}

int Run(const fs::path& root)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    SDL_Window* window = SDL_CreateWindow(
        "War Map Preview",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        kInitialWindowWidth,
        kInitialWindowHeight,
        SDL_WINDOW_SHOWN
    );

    if (!window)
    {
        std::cerr << "SDL_CreateWindow failed: "
                  << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer)
    {
        std::cerr << "SDL_CreateRenderer failed: "
                  << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const fs::path mapPath = root / "china_map.bmp";
    const fs::path regionMapPath = root / "china_region_ids.bin";

    int mapWidth = 0;
    int mapHeight = 0;
    SDL_Texture* baseTexture = LoadBitmapTexture(
        renderer,
        mapPath,
        mapWidth,
        mapHeight
    );

    if (!baseTexture)
    {
        std::cerr << "Failed to load " << mapPath << ": "
                  << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    UiImageSet uiImages;
    if (!LoadUiImageSet(renderer, root, uiImages))
    {
        std::cerr
            << "One or more UI images are unavailable; "
               "using fallback rendering for missing images\n";
    }

    RegionMap regionMap;

    if (!LoadRegionMap(regionMapPath, regionMap)
        || static_cast<int>(regionMap.width) != mapWidth
        || static_cast<int>(regionMap.height) != mapHeight)
    {
        std::cerr << "Failed to load or validate "
                  << regionMapPath << '\n';
        DestroyUiImageSet(uiImages);
        SDL_DestroyTexture(baseTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    RegionPixelIndex regionPixelIndex;

    if (!BuildRegionPixelIndex(
        regionMap,
        regionPixelIndex
    ))
    {
        std::cerr << "Failed to build Region pixel index\n";
        DestroyUiImageSet(uiImages);
        SDL_DestroyTexture(baseTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* overlayTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        mapWidth,
        mapHeight
    );

    if (!overlayTexture)
    {
        std::cerr << "SDL_CreateTexture failed: "
                  << SDL_GetError() << '\n';
        DestroyUiImageSet(uiImages);
        SDL_DestroyTexture(baseTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureBlendMode(overlayTexture, SDL_BLENDMODE_BLEND);

    SDL_Texture* hoverTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        mapWidth,
        mapHeight
    );

    if (!hoverTexture)
    {
        std::cerr << "SDL_CreateTexture for hover layer failed: "
                  << SDL_GetError() << '\n';
        DestroyUiImageSet(uiImages);
        SDL_DestroyTexture(overlayTexture);
        SDL_DestroyTexture(baseTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureBlendMode(
        hoverTexture,
        SDL_BLENDMODE_BLEND
    );

    std::vector<RgbaPixel> hoverPixels(
        regionMap.regionIds.size(),
        RgbaPixel{0, 0, 0, 0}
    );

    SDL_UpdateTexture(
        hoverTexture,
        nullptr,
        hoverPixels.data(),
        mapWidth * static_cast<int>(sizeof(RgbaPixel))
    );

    SDL_Texture* infoTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        kPanelWidth,
        kInfoCardHeight
    );

    if (!infoTexture)
    {
        std::cerr << "SDL_CreateTexture for info card failed: "
                  << SDL_GetError() << '\n';
        DestroyUiImageSet(uiImages);
        SDL_DestroyTexture(hoverTexture);
        SDL_DestroyTexture(overlayTexture);
        SDL_DestroyTexture(baseTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureBlendMode(
        infoTexture,
        SDL_BLENDMODE_BLEND
    );

    const int bannerWidth =
        kInitialWindowWidth - kPanelWidth;

    SDL_Texture* bannerTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        bannerWidth,
        kBannerHeight
    );

    if (!bannerTexture)
    {
        std::cerr << "SDL_CreateTexture for war banner failed: "
                  << SDL_GetError() << '\n';
        DestroyUiImageSet(uiImages);
        SDL_DestroyTexture(infoTexture);
        SDL_DestroyTexture(hoverTexture);
        SDL_DestroyTexture(overlayTexture);
        SDL_DestroyTexture(baseTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureBlendMode(
        bannerTexture,
        SDL_BLENDMODE_BLEND
    );

    std::vector<uint8_t> infoCardPixels;
    std::vector<uint8_t> bannerPixels;

    SimulatedStateSource stateSource;
    WarMapState state;
    std::vector<RgbaPixel> overlayPixels;
    std::vector<float> previousPercentages;
    std::vector<uint16_t> changedRegionIds;
    bool hasPreviousOverlay = false;
    bool previousVisible = false;
    bool previousActive = false;
    Uint32 nextStateUpdate = 0;
    uint16_t hoveredRegionId = 0;
    uint16_t selectedRegionId = 0;
    bool infoCardDirty = true;
    bool bannerDirty = true;
    std::string previousViewerTag;
    bool skinButtonPressed = false;
    bool running = true;

    while (running)
    {
        SDL_Event event{};

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_KEYDOWN
                && event.key.keysym.sym == SDLK_ESCAPE)
            {
                running = false;
            }
            else if (event.type == SDL_MOUSEMOTION)
            {
                int windowWidth = 0;
                int windowHeight = 0;

                SDL_GetWindowSize(
                    window,
                    &windowWidth,
                    &windowHeight
                );

                const MapRect mapRect = CalculateMapRect(
                    windowWidth,
                    windowHeight,
                    mapWidth,
                    mapHeight
                );

                uint16_t newHoveredRegionId =
                    PickRegion(
                        regionMap,
                        mapRect,
                        event.motion.x,
                        event.motion.y
                    );

                if (newHoveredRegionId
                    > kWarMapRegionNames.size())
                {
                    newHoveredRegionId = 0;
                }

                if (newHoveredRegionId != hoveredRegionId)
                {
                    const uint16_t previousRegionId =
                        hoveredRegionId;

                    UpdateRegionHighlight(
                        regionPixelIndex,
                        previousRegionId,
                        newHoveredRegionId,
                        hoverPixels
                    );

                    if (previousRegionId != 0)
                    {
                        UpdateOverlayTextureRegion(
                            hoverTexture,
                            hoverPixels,
                            mapWidth,
                            regionPixelIndex
                                .boundsByRegion[previousRegionId]
                        );
                    }

                    if (newHoveredRegionId != 0)
                    {
                        UpdateOverlayTextureRegion(
                            hoverTexture,
                            hoverPixels,
                            mapWidth,
                            regionPixelIndex
                                .boundsByRegion[newHoveredRegionId]
                        );
                    }

                    hoveredRegionId = newHoveredRegionId;
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN
                && event.button.button == SDL_BUTTON_LEFT)
            {
                int windowWidth = 0;
                int windowHeight = 0;
                SDL_GetWindowSize(
                    window,
                    &windowWidth,
                    &windowHeight
                );

                skinButtonPressed = IsPointInside(
                    GetSkinButtonRect(
                        windowWidth,
                        windowHeight
                    ),
                    event.button.x,
                    event.button.y
                );

                const MapRect mapRect = CalculateMapRect(
                    windowWidth,
                    windowHeight,
                    mapWidth,
                    mapHeight
                );

                const uint16_t regionId = PickRegion(
                    regionMap,
                    mapRect,
                    event.button.x,
                    event.button.y
                );

                if (regionId != 0
                    && regionId <= kWarMapRegionNames.size())
                {
                    selectedRegionId = regionId;
                    infoCardDirty = true;

                    const std::string title =
                        "War Map Preview - "
                        + std::string(
                            kWarMapRegionNames[regionId - 1]
                        )
                        + " "
                        + FormatPercentage(
                            state.controlledPercentages[regionId]
                        );

                    SDL_SetWindowTitle(window, title.c_str());
                }
            }
            else if (event.type == SDL_MOUSEBUTTONUP
                && event.button.button == SDL_BUTTON_LEFT)
            {
                skinButtonPressed = false;
            }
        }

        const Uint32 now = SDL_GetTicks();

        if (now >= nextStateUpdate)
        {
            stateSource.Read(state);
            infoCardDirty = true;

            if (state.viewerTag != previousViewerTag)
            {
                previousViewerTag = state.viewerTag;
                bannerDirty = true;
            }

            const std::vector<float> currentPercentages =
                ToPercentageVector(state);

            bool overlayChanged = false;
            bool uploadFullTexture = false;

            if (!hasPreviousOverlay
                || state.visible != previousVisible
                || state.active != previousActive)
            {
                BuildOverlayFromState(
                    regionMap,
                    state,
                    overlayPixels
                );

                overlayChanged = true;
                uploadFullTexture = true;
            }
            else if (state.visible && state.active)
            {
                overlayChanged =
                    UpdateChangedRegionOverlay(
                        regionPixelIndex,
                        previousPercentages,
                        currentPercentages,
                        overlayPixels,
                        &changedRegionIds
                    );
            }

            if (overlayChanged)
            {
                if (uploadFullTexture)
                {
                    UpdateOverlayTexture(
                        overlayTexture,
                        overlayPixels,
                        mapWidth
                    );
                }
                else
                {
                    for (const uint16_t regionId : changedRegionIds)
                    {
                        UpdateOverlayTextureRegion(
                            overlayTexture,
                            overlayPixels,
                            mapWidth,
                            regionPixelIndex.boundsByRegion[regionId]
                        );
                    }
                }
            }

            previousPercentages = currentPercentages;
            previousVisible = state.visible;
            previousActive = state.active;
            hasPreviousOverlay = true;
            nextStateUpdate = now + kStateUpdateIntervalMs;
        }

        if (infoCardDirty)
        {
            UpdateInfoCardTexture(
                infoTexture,
                infoCardPixels,
                state,
                selectedRegionId
            );
            infoCardDirty = false;
        }

        if (bannerDirty)
        {
            UpdateWarBannerTexture(
                bannerTexture,
                bannerPixels,
                state,
                bannerWidth
            );
            bannerDirty = false;
        }

        int windowWidth = 0;
        int windowHeight = 0;
        SDL_GetWindowSize(
            window,
            &windowWidth,
            &windowHeight
        );

        const MapRect mapRect = CalculateMapRect(
            windowWidth,
            windowHeight,
            mapWidth,
            mapHeight
        );

        SDL_SetRenderDrawColor(renderer,0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        SDL_Rect destination{
            mapRect.x,
            mapRect.y,
            mapRect.width,
            mapRect.height
        };

        SDL_RenderCopy(renderer, baseTexture, nullptr, &destination);
        SDL_RenderCopy(renderer, overlayTexture, nullptr, &destination);
        SDL_RenderCopy(renderer, hoverTexture, nullptr, &destination);

        DrawInfoPanel(
            renderer,
            windowWidth,
            windowHeight,
            infoTexture,
            uiImages,
            skinButtonPressed
        );
        DrawWindowBorder(
            renderer,
            windowWidth,
            windowHeight,
            uiImages
        );

        SDL_Rect bannerRect{
            0,
            kBannerTop,
            windowWidth - kPanelWidth,
            kBannerHeight
        };

        SDL_RenderCopy(
            renderer,
            bannerTexture,
            nullptr,
            &bannerRect
        );

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(infoTexture);
    SDL_DestroyTexture(bannerTexture);
    SDL_DestroyTexture(hoverTexture);
    SDL_DestroyTexture(overlayTexture);
    SDL_DestroyTexture(baseTexture);
    DestroyUiImageSet(uiImages);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

}

int main(int argc, char** argv)
{
    const fs::path root = argc >= 2
        ? fs::path(argv[1])
        : fs::current_path();

    return Run(root);
}
