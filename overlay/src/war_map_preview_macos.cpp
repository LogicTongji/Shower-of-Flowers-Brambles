#include <SDL.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "gui_interpreter.h"
#include "region_color_core.h"
#include "war_map_input.h"
#include "war_map_region_info.h"
#include "war_map_state.h"

namespace fs = std::filesystem;

namespace
{

constexpr int kInitialWindowWidth = 1600;
constexpr int kInitialWindowHeight = 800;
constexpr int kMapPadding = 20;
constexpr int kSidebarWidth = 320;
constexpr int kBannerHeight = 72;
constexpr int kBannerTop = 20;
constexpr int kInfoCardHeight = 300;
constexpr int kInfoCardTop = 20;
constexpr int kMapTop = 120;
constexpr int kLegendWidth = 320;
constexpr int kLegendHeight = 150;
constexpr int kLegendOffsetY = -45;
constexpr int kLegendLeft = 40;
constexpr int kLegendPanelWidth = 320;
constexpr int kLegendPanelHeight = 500;
constexpr int kLegendPanelGap = 20;
constexpr int kMapLeft =
    kLegendLeft + kLegendPanelWidth + kLegendPanelGap;
constexpr int kWarProgressTitleHeight = 44;
constexpr int kWarProgressFrameWidth = 280;
constexpr int kWarProgressFrameHeight = 32;
constexpr int kWarProgressFrameX = 20;
constexpr int kWarProgressFrameY = 48;
constexpr int kWarProgressInnerX = 5;
constexpr int kWarProgressInnerY = 5;
constexpr int kWarProgressInnerWidth = 270;
constexpr int kWarProgressInnerHeight = 22;
constexpr int kCombatHeaderTop = 88;
constexpr int kCombatHeaderHeight = 32;
constexpr int kCombatListLeft = 14;
constexpr int kCombatListTop = 124;
constexpr int kCombatListWidth = 280;
constexpr int kCombatListHeight = 190;
constexpr int kCombatButtonWidth = 136;
constexpr int kCombatButtonHeight = 30;
constexpr int kCombatColumnGap = 8;
constexpr int kCombatRowGap = 6;
constexpr int kCombatRowStep =
    kCombatButtonHeight + kCombatRowGap;
constexpr int kCombatScrollBarGap = 6;
constexpr int kCombatScrollBarWidth = 8;
constexpr int kCombatScrollThumbMinimum = 24;
constexpr int kCombatMaximumRows =
    (static_cast<int>(kWarMapRegionCount) + 1) / 2;
constexpr int kCombatContentHeight =
    kCombatMaximumRows * kCombatRowStep;
constexpr int kLegendSwatchX = 11;
constexpr int kLegendSwatchSize = 14;
constexpr int kLegendSwatchFillInset = 2;
constexpr int kLegendSwatchFillSize =
    kLegendSwatchSize - 2 * kLegendSwatchFillInset;
constexpr int kLegendTitleColumnX = 300;
constexpr int kLegendTitleColumnWidth = 20;
constexpr Uint32 kStateUpdateIntervalMs = 120;

struct UiImageSet
{
    SDL_Texture* sidebar = nullptr;
    SDL_Texture* buttonNormal = nullptr;
    SDL_Texture* buttonPressed = nullptr;
    SDL_Texture* border = nullptr;
    SDL_Texture* legend = nullptr;
    SDL_Texture* legendText = nullptr;
    SDL_Texture* legendPanel = nullptr;
    SDL_Texture* warProgress = nullptr;
    SDL_Texture* warProgressTitle = nullptr;
    SDL_Texture* combatRegionButtonNormal = nullptr;
    SDL_Texture* combatRegionButtonPressed = nullptr;
    SDL_Texture* combatRegionHeader = nullptr;
    SDL_Texture* combatRegionLabels = nullptr;
};

struct UiFontSet
{
    CGFontRef smallChinese = nullptr;
    CGFontRef bodyChinese = nullptr;
    CGFontRef titleChinese = nullptr;
    CGFontRef titleJapanese = nullptr;
};

struct CombatRegionListState
{
    std::vector<uint16_t> regionIds;
    int scrollOffset = 0;
    int hoveredIndex = -1;
    uint16_t selectedRegionId = 0;
};

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

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
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
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }

    return texture;
}

bool LoadUiImageSet(
    SDL_Renderer* renderer,
    const fs::path& root,
    const gui::GuiInterpreter& interpreter,
    UiImageSet& output
)
{
    bool complete = true;

    const auto load = [
        renderer,
        &root,
        &interpreter,
        &complete
    ](
        const char* resourceName,
        const char* fallbackFileName,
        SDL_Texture*& target
    )
    {
        fs::path imagePath = interpreter.ResolveTexture(
            resourceName,
            root
        );

        if (imagePath.empty() && fallbackFileName)
        {
            imagePath = root
                / "gfx"
                / "war_map"
                / "china_anti_jap"
                / fallbackFileName;
        }

        if (!imagePath.empty())
        {
            target = LoadPngTexture(renderer, imagePath);
        }

        if (!target)
        {
            complete = false;
            std::cerr << "Failed to load UI image: "
                      << (imagePath.empty()
                          ? fs::path(resourceName)
                          : imagePath)
                      << '\n';
        }
    };

    load(
        "GFX_china_war_sidebar",
        "sidebar.png",
        output.sidebar
    );
    load(
        "GFX_china_war_border",
        "border.png",
        output.border
    );
    load(
        "GFX_china_war_legend",
        "legend.png",
        output.legend
    );
    load(
        "GFX_china_war_legend_panel",
        "legend_panel.png",
        output.legendPanel
    );
    load(
        "GFX_china_war_progress",
        "war_prograss.png",
        output.warProgress
    );

    const auto loadOptional = [
        renderer,
        &root,
        &interpreter
    ](
        const char* resourceName,
        SDL_Texture*& target
    )
    {
        const fs::path imagePath = interpreter.ResolveTexture(
            resourceName,
            root
        );

        if (!imagePath.empty())
        {
            target = LoadPngTexture(renderer, imagePath);
        }
    };

    loadOptional(
        "GFX_china_war_combat_button_normal",
        output.combatRegionButtonNormal
    );
    loadOptional(
        "GFX_china_war_combat_button_pressed",
        output.combatRegionButtonPressed
    );

    return complete;
}

void DestroyUiImageSet(UiImageSet& images)
{
    SDL_DestroyTexture(images.sidebar);
    SDL_DestroyTexture(images.buttonNormal);
    SDL_DestroyTexture(images.buttonPressed);
    SDL_DestroyTexture(images.border);
    SDL_DestroyTexture(images.legend);
    SDL_DestroyTexture(images.legendText);
    SDL_DestroyTexture(images.legendPanel);
    SDL_DestroyTexture(images.warProgress);
    SDL_DestroyTexture(images.warProgressTitle);
    SDL_DestroyTexture(images.combatRegionButtonNormal);
    SDL_DestroyTexture(images.combatRegionButtonPressed);
    SDL_DestroyTexture(images.combatRegionHeader);
    SDL_DestroyTexture(images.combatRegionLabels);
    images = {};
}

CGFontRef LoadFontFile(const fs::path& path)
{
    const std::string pathString = path.string();
    CGDataProviderRef provider =
        CGDataProviderCreateWithFilename(pathString.c_str());

    if (!provider)
    {
        return nullptr;
    }

    CGFontRef font = CGFontCreateWithDataProvider(provider);
    CGDataProviderRelease(provider);
    return font;
}

bool LoadUiFontSet(
    const fs::path& root,
    UiFontSet& output
)
{
    CGFontRef pixelChina = LoadFontFile(
        root / "font" / "pixel_china.ttf"
    );
    CGFontRef japFont1 = LoadFontFile(
        root / "font" / "jap_font1.ttf"
    );
    CGFontRef maozedongArt = LoadFontFile(
        root / "font" / "maozedong_art.ttf"
    );

    if (!pixelChina || !japFont1 || !maozedongArt)
    {
        if (pixelChina)
        {
            CGFontRelease(pixelChina);
        }
        if (japFont1)
        {
            CGFontRelease(japFont1);
        }
        if (maozedongArt)
        {
            CGFontRelease(maozedongArt);
        }
        return false;
    }

    output.smallChinese = CGFontRetain(japFont1);
    output.bodyChinese = CGFontRetain(pixelChina);
    output.titleChinese = maozedongArt;
    output.titleJapanese = japFont1;
    CGFontRelease(pixelChina);
    CGFontRelease(japFont1);
    return true;
}

void DestroyUiFontSet(UiFontSet& fonts)
{
    if (fonts.smallChinese)
    {
        CGFontRelease(fonts.smallChinese);
    }

    if (fonts.bodyChinese)
    {
        CGFontRelease(fonts.bodyChinese);
    }

    if (fonts.titleChinese)
    {
        CGFontRelease(fonts.titleChinese);
    }

    if (fonts.titleJapanese)
    {
        CGFontRelease(fonts.titleJapanese);
    }

    fonts = {};
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

MapRect CalculateMapRect(
    int windowWidth,
    int windowHeight,
    int mapWidth,
    int mapHeight
)
{
    const int viewportWidth = std::max(
        1,
        windowWidth
            - kSidebarWidth
            - kMapLeft
            - kMapPadding
    );
    const int viewportHeight = std::max(
        1,
        windowHeight - kMapTop - kMapPadding
    );

    const double widthScale =
        static_cast<double>(viewportWidth)
        / static_cast<double>(mapWidth);
    const double heightScale =
        static_cast<double>(viewportHeight)
        / static_cast<double>(mapHeight);
    const double scale = std::max(
        0.01,
        std::min(widthScale, heightScale)
    );

    const int displayWidth = std::max(
        1,
        static_cast<int>(mapWidth * scale)
    );
    const int displayHeight = std::max(
        1,
        static_cast<int>(mapHeight * scale)
    );

    return {
        kMapLeft
            + (viewportWidth - displayWidth) / 2,
        kMapTop + (viewportHeight - displayHeight) / 2,
        displayWidth,
        displayHeight
    };
}

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

CTFontRef CreateUiFont(
    CGFloat fontSize,
    CGFontRef fontSource
)
{
    if (fontSource)
    {
        return CTFontCreateWithGraphicsFont(
            fontSource,
            fontSize,
            nullptr,
            nullptr
        );
    }

    return CTFontCreateWithName(
        CFSTR("PingFang SC"),
        fontSize,
        nullptr
    );
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
    CGFloat blue,
    CGFontRef fontSource
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

    CTFontRef font = CreateUiFont(fontSize, fontSource);
    CGColorRef color = CGColorCreateGenericRGB(
        red,
        green,
        blue,
        1.0
    );
    const void* keys[] = {
        kCTFontAttributeName,
        kCTForegroundColorAttributeName
    };
    const void* values[] = {font, color};
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
        if (fontSource)
        {
            CGContextSetAllowsAntialiasing(context, false);
            CGContextSetShouldAntialias(context, false);
            CGContextSetShouldSmoothFonts(context, false);
        }

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
    CGColorRelease(color);
    if (font)
    {
        CFRelease(font);
    }
    CFRelease(cfText);
}

CGFloat MeasureCoreTextLine(
    const std::string& text,
    CGFloat fontSize,
    CGFontRef fontSource
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

    CTFontRef font = CreateUiFont(fontSize, fontSource);
    const void* keys[] = {kCTFontAttributeName};
    const void* values[] = {font};
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
        width = static_cast<CGFloat>(
            CTLineGetTypographicBounds(
                line,
                nullptr,
                nullptr,
                nullptr
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
    if (font)
    {
        CFRelease(font);
    }
    CFRelease(cfText);
    return width;
}

void DrawBannerText(
    CGContextRef context,
    const std::string& text,
    CGFloat x,
    CGFloat y,
    CGFloat size,
    CGFloat red,
    CGFloat green,
    CGFloat blue,
    CGFontRef font
)
{
    DrawCoreTextLine(
        context,
        kBannerHeight,
        text,
        x + 2.0,
        y + 2.0,
        size,
        0.02,
        0.03,
        0.04,
        font
    );
    DrawCoreTextLine(
        context,
        kBannerHeight,
        text,
        x,
        y,
        size,
        red,
        green,
        blue,
        font
    );
}

void UpdateWarBannerTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const WarMapState& state,
    int width,
    const UiFontSet& fonts
)
{
    pixels.assign(
        static_cast<size_t>(width) * kBannerHeight * 4,
        0
    );
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
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

    const CGFloat centerX = static_cast<CGFloat>(width) / 2.0;
    const CGFloat smallSize = 26.0;
    const CGFloat largeSize = 44.0;
    const CGFloat gap = 18.0;

    if (state.viewerTag == "JAP")
    {
        const std::string title = "暴支膺惩";
        const CGFloat titleWidth = MeasureCoreTextLine(
            title,
            largeSize,
            fonts.titleJapanese
        );
        DrawBannerText(
            context,
            title,
            centerX - titleWidth / 2.0,
            8.0,
            largeSize,
            0.34,
            0.90,
            0.90,
            fonts.titleJapanese
        );
    }
    else
    {
        const std::string left = "一寸山河一寸血";
        const std::string center = "中国人民抗日战争";
        const std::string right = "十万青年十万军";
        const CGFloat leftWidth = MeasureCoreTextLine(
            left,
            smallSize,
            fonts.smallChinese
        );
        const CGFloat centerWidth = MeasureCoreTextLine(
            center,
            largeSize,
            fonts.titleChinese
        );
        const CGFloat centerLeft =
            centerX - centerWidth / 2.0;
        const CGFloat centerRight =
            centerX + centerWidth / 2.0;
        DrawBannerText(
            context,
            left,
            centerLeft - gap - leftWidth,
            24.0,
            smallSize,
            0.30,
            0.84,
            0.85,
            fonts.smallChinese
        );
        DrawBannerText(
            context,
            center,
            centerX - centerWidth / 2.0,
            4.0,
            largeSize,
            0.34,
            0.90,
            0.90,
            fonts.titleChinese
        );
        DrawBannerText(
            context,
            right,
            centerRight + gap,
            24.0,
            smallSize,
            0.30,
            0.84,
            0.85,
            fonts.smallChinese
        );
    }

    CGContextRelease(context);
    SDL_UpdateTexture(texture, nullptr, pixels.data(), width * 4);
}

void UpdateInfoCardTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const WarMapState& state,
    uint16_t selectedRegionId,
    const UiFontSet& fonts
)
{
    pixels.assign(
        static_cast<size_t>(kSidebarWidth) * kInfoCardHeight * 4,
        0
    );
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(),
        kSidebarWidth,
        kInfoCardHeight,
        8,
        kSidebarWidth * 4,
        colorSpace,
        kCGImageAlphaPremultipliedLast
            | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colorSpace);

    if (!context)
    {
        return;
    }

    constexpr int detailX = 25;
    const auto draw = [&context, &fonts, detailX](
        const std::string& text,
        int y,
        CGFloat size
    )
    {
        DrawCoreTextLine(
            context,
            kInfoCardHeight,
            text,
            detailX,
            y,
            size,
            0.0,
            0.0,
            0.0,
            fonts.bodyChinese
        );
    };

    if (selectedRegionId == 0
        || selectedRegionId >= kWarMapRegionNames.size() + 1
        || selectedRegionId >= state.populations.size())
    {
        CGContextRelease(context);
        SDL_UpdateTexture(
            texture,
            nullptr,
            pixels.data(),
            kSidebarWidth * 4
        );
        return;
    }

    const std::string regionId = std::string(
        kWarMapRegionNames[selectedRegionId - 1]
    );
    const RegionStaticInfo staticInfo =
        GetRegionStaticInfo(regionId);
    const RegionPopulationState& population =
        state.populations[selectedRegionId];
    const bool japanViewer = state.viewerTag == "JAP";
    const std::string regionName =
        std::string(staticInfo.chineseName);
    const CGFloat regionNameWidth = MeasureCoreTextLine(
        regionName,
        20.0,
        fonts.bodyChinese
    );

    DrawCoreTextLine(
        context,
        kInfoCardHeight,
        regionName,
        (static_cast<CGFloat>(kSidebarWidth)
            - regionNameWidth) / 2.0,
        25,
        20,
        0.0,
        0.0,
        0.0,
        fonts.bodyChinese
    );
    draw(
        "省会：" + std::string(staticInfo.capitalName),
        120,
        16
    );
    draw(
        FormatPopulationValue(
            population.known,
            population.total,
            "总人口："
        ),
        156,
        16
    );
    draw(
        FormatPopulationValue(
            population.known,
            population.affected,
            japanViewer ? "控制人口：" : "沦陷人口："
        ),
        192,
        16
    );
    draw(
        std::string(japanViewer ? "控制程度：" : "沦陷程度：")
            + FormatPercentage(
                state.controlledPercentages[selectedRegionId]
            ),
        228,
        16
    );
    draw(
        FormatPopulationValue(
            population.known,
            population.remaining,
            "剩余人口："
        ),
        264,
        16
    );

    CGContextRelease(context);
    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        kSidebarWidth * 4
    );
}

void UpdateLegendTextTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const WarMapState& state,
    const UiFontSet& fonts
)
{
    pixels.assign(
        static_cast<size_t>(kLegendWidth) * kLegendHeight * 4,
        0
    );
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(),
        kLegendWidth,
        kLegendHeight,
        8,
        kLegendWidth * 4,
        colorSpace,
        kCGImageAlphaPremultipliedLast
            | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colorSpace);

    if (!context)
    {
        return;
    }

    constexpr CGFloat textRed = 0.30;
    constexpr CGFloat textGreen = 0.84;
    constexpr CGFloat textBlue = 0.85;
    constexpr CGFloat textSize = 16.0;

    const CGFloat titleFontSize = 18.0;
    const CGFloat titleGap = 5.0;
    const CGFloat titleHeight =
        titleFontSize * 2.0 + titleGap;
    const CGFloat titleTop =
        (static_cast<CGFloat>(kLegendHeight)
            - titleHeight) / 2.0;
    const std::string titleCharacters[] = {
        "图",
        "例"
    };

    for (size_t index = 0; index < 2; ++index)
    {
        const CGFloat characterWidth =
            MeasureCoreTextLine(
                titleCharacters[index],
                titleFontSize,
                fonts.smallChinese
            );
        const CGFloat characterX =
            kLegendTitleColumnX
            + (kLegendTitleColumnWidth
                - characterWidth) / 2.0;

        DrawCoreTextLine(
            context,
            kLegendHeight,
            titleCharacters[index],
            characterX,
            titleTop + index * (titleFontSize + titleGap),
            titleFontSize,
            textRed,
            textGreen,
            textBlue,
            fonts.smallChinese
        );
    }
    DrawCoreTextLine(
        context,
        kLegendHeight,
        "交战区",
        58.0,
        29.0,
        textSize,
        textRed,
        textGreen,
        textBlue,
        fonts.smallChinese
    );

    const bool japanViewer = state.viewerTag == "JAP";
    DrawCoreTextLine(
        context,
        kLegendHeight,
        japanViewer ? "初步占领" : "部分沦陷",
        58.0,
        58.0,
        textSize,
        textRed,
        textGreen,
        textBlue,
        fonts.smallChinese
    );
    DrawCoreTextLine(
        context,
        kLegendHeight,
        japanViewer ? "基本占领" : "半部沦陷",
        58.0,
        87.0,
        textSize,
        textRed,
        textGreen,
        textBlue,
        fonts.smallChinese
    );
    DrawCoreTextLine(
        context,
        kLegendHeight,
        japanViewer ? "完全占领" : "全部沦陷",
        58.0,
        116.0,
        textSize,
        textRed,
        textGreen,
        textBlue,
        fonts.smallChinese
    );

    CGContextRelease(context);
    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        kLegendWidth * 4
    );
}

void DrawLegendSwatches(
    SDL_Renderer* renderer,
    const SDL_Rect& legendRect
)
{
    const SDL_Color colors[] = {
        {255, 245, 170, SDL_ALPHA_OPAQUE},
        {240, 210, 60, SDL_ALPHA_OPAQUE},
        {230, 100, 100, SDL_ALPHA_OPAQUE},
        {150, 20, 20, SDL_ALPHA_OPAQUE},
        {80, 0, 0, SDL_ALPHA_OPAQUE}
    };
    const int positions[][2] = {
        {kLegendSwatchX, 33},
        {kLegendSwatchX + 23, 33},
        {kLegendSwatchX, 62},
        {kLegendSwatchX, 91},
        {kLegendSwatchX, 120}
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (size_t index = 0; index < 5; ++index)
    {
        SDL_SetRenderDrawColor(
            renderer,
            colors[index].r,
            colors[index].g,
            colors[index].b,
            colors[index].a
        );
        const SDL_Rect swatch{
            legendRect.x + positions[index][0]
                + kLegendSwatchFillInset,
            legendRect.y + positions[index][1]
                + kLegendSwatchFillInset,
            kLegendSwatchFillSize,
            kLegendSwatchFillSize
        };
        SDL_RenderFillRect(renderer, &swatch);
    }
}

void UpdateWarProgressTitleTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const UiFontSet& fonts
)
{
    pixels.assign(
        static_cast<size_t>(kLegendPanelWidth)
            * kWarProgressTitleHeight
            * 4,
        0
    );

    CGColorSpaceRef colorSpace =
        CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(),
        kLegendPanelWidth,
        kWarProgressTitleHeight,
        8,
        kLegendPanelWidth * 4,
        colorSpace,
        kCGImageAlphaPremultipliedLast
            | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colorSpace);

    if (!context)
    {
        return;
    }

    const std::string title = "总体战局";
    const CGFloat fontSize = 22.0;
    const CGFloat titleWidth = MeasureCoreTextLine(
        title,
        fontSize,
        fonts.smallChinese
    );

    DrawCoreTextLine(
        context,
        kWarProgressTitleHeight,
        title,
        (static_cast<CGFloat>(kLegendPanelWidth)
            - titleWidth) / 2.0,
        8.0,
        fontSize,
        0.30,
        0.84,
        0.85,
        fonts.smallChinese
    );

    CGContextRelease(context);
    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        kLegendPanelWidth * 4
    );
}

void UpdateCombatRegionHeaderTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const UiFontSet& fonts
)
{
    pixels.assign(
        static_cast<size_t>(kLegendPanelWidth)
            * kCombatHeaderHeight
            * 4,
        0
    );

    CGColorSpaceRef colorSpace =
        CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(),
        kLegendPanelWidth,
        kCombatHeaderHeight,
        8,
        kLegendPanelWidth * 4,
        colorSpace,
        kCGImageAlphaPremultipliedLast
            | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colorSpace);

    if (!context)
    {
        return;
    }

    const std::string title = "交战区";
    const CGFloat fontSize = 18.0;
    const CGFloat titleWidth = MeasureCoreTextLine(
        title,
        fontSize,
        fonts.smallChinese
    );

    DrawCoreTextLine(
        context,
        kCombatHeaderHeight,
        title,
        (static_cast<CGFloat>(kLegendPanelWidth)
            - titleWidth) / 2.0,
        5.0,
        fontSize,
        0.30,
        0.84,
        0.85,
        fonts.smallChinese
    );

    CGContextRelease(context);
    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        kLegendPanelWidth * 4
    );
}

void DrawWarProgressBar(
    SDL_Renderer* renderer,
    SDL_Texture* frameTexture,
    const SDL_Rect& panelRect,
    const WarProgressState& progress
)
{
    if (!frameTexture || !progress.known)
    {
        return;
    }

    const SDL_Rect progressRect{
        panelRect.x + kWarProgressFrameX,
        panelRect.y + kWarProgressFrameY,
        kWarProgressFrameWidth,
        kWarProgressFrameHeight
    };

    DrawUiImage(
        renderer,
        frameTexture,
        progressRect
    );

    const float own = std::max(
        0.0f,
        std::min(1.0f, progress.own)
    );
    const float enemy = std::max(
        0.0f,
        std::min(1.0f, progress.enemy)
    );
    const float total = own + enemy;

    if (total <= 0.0f)
    {
        return;
    }

    const int ownWidth = static_cast<int>(
        kWarProgressInnerWidth * (own / total)
    );

    const SDL_Rect ownRect{
        progressRect.x + kWarProgressInnerX,
        progressRect.y + kWarProgressInnerY,
        ownWidth,
        kWarProgressInnerHeight
    };
    const SDL_Rect enemyRect{
        progressRect.x + kWarProgressInnerX + ownWidth,
        progressRect.y + kWarProgressInnerY,
        kWarProgressInnerWidth - ownWidth,
        kWarProgressInnerHeight
    };

    SDL_SetRenderDrawColor(
        renderer,
        70,
        180,
        95,
        SDL_ALPHA_OPAQUE
    );
    SDL_RenderFillRect(renderer, &ownRect);

    SDL_SetRenderDrawColor(
        renderer,
        185,
        70,
        70,
        SDL_ALPHA_OPAQUE
    );
    SDL_RenderFillRect(renderer, &enemyRect);
}

std::vector<uint16_t> BuildCombatRegionIds(
    const WarMapState& state
)
{
    std::vector<uint16_t> regionIds;

    if (!state.active)
    {
        return regionIds;
    }

    for (size_t regionId = 1;
         regionId <= kWarMapRegionCount;
         ++regionId)
    {
        if (state.controlledPercentages[regionId] > 0.0f
            && state.controlledPercentages[regionId] < 90.0f)
        {
            regionIds.push_back(
                static_cast<uint16_t>(regionId)
            );
        }
    }

    return regionIds;
}

int GetCombatRegionContentHeight(
    size_t regionCount
)
{
    const int rowCount = static_cast<int>(
        (regionCount + 1) / 2
    );
    return rowCount * kCombatRowStep;
}

int GetCombatRegionMaximumScroll(
    size_t regionCount
)
{
    return std::max(
        0,
        GetCombatRegionContentHeight(regionCount)
            - kCombatListHeight
    );
}

void ClampCombatRegionScroll(
    CombatRegionListState& listState
)
{
    listState.scrollOffset = std::max(
        0,
        std::min(
            listState.scrollOffset,
            GetCombatRegionMaximumScroll(
                listState.regionIds.size()
            )
        )
    );
}

void UpdateCombatRegionLabelsTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const std::vector<uint16_t>& regionIds,
    const UiFontSet& fonts
)
{
    pixels.assign(
        static_cast<size_t>(kCombatListWidth)
            * kCombatContentHeight
            * 4,
        0
    );

    CGColorSpaceRef colorSpace =
        CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        pixels.data(),
        kCombatListWidth,
        kCombatContentHeight,
        8,
        kCombatListWidth * 4,
        colorSpace,
        kCGImageAlphaPremultipliedLast
            | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colorSpace);

    if (!context)
    {
        return;
    }

    for (size_t index = 0;
         index < regionIds.size();
         ++index)
    {
        const uint16_t regionId = regionIds[index];
        const size_t row = index / 2;
        const size_t column = index % 2;
        const std::string regionName = std::string(
            GetRegionStaticInfo(
                kWarMapRegionNames[regionId - 1]
            ).chineseName
        );
        const CGFloat fontSize = 14.0;
        const CGFloat textWidth = MeasureCoreTextLine(
            regionName,
            fontSize,
            fonts.bodyChinese
        );
        const CGFloat buttonLeft =
            static_cast<CGFloat>(column)
            * (kCombatButtonWidth + kCombatColumnGap);
        const CGFloat textX =
            buttonLeft
            + (kCombatButtonWidth - textWidth) / 2.0;
        const CGFloat textY =
            static_cast<CGFloat>(row * kCombatRowStep)
            + 6.0;

        DrawCoreTextLine(
            context,
            kCombatContentHeight,
            regionName,
            textX,
            textY,
            fontSize,
            0.30,
            0.84,
            0.85,
            fonts.bodyChinese
        );
    }

    CGContextRelease(context);
    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        kCombatListWidth * 4
    );
}

SDL_Rect GetCombatRegionListRect(
    const SDL_Rect& panelRect
)
{
    return {
        panelRect.x + kCombatListLeft,
        panelRect.y + kCombatListTop,
        kCombatListWidth,
        kCombatListHeight
    };
}

SDL_Rect GetWarInfoPanelRect(
    const MapRect& mapRect
)
{
    return {
        kLegendLeft,
        mapRect.y + kLegendOffsetY
            + kLegendHeight
            + kLegendPanelGap,
        kLegendPanelWidth,
        kLegendPanelHeight
    };
}

int GetCombatRegionIndexAtPoint(
    const SDL_Rect& panelRect,
    const CombatRegionListState& listState,
    int mouseX,
    int mouseY
)
{
    const SDL_Rect listRect =
        GetCombatRegionListRect(panelRect);
    const SDL_Point mousePoint{
        mouseX,
        mouseY
    };

    if (!SDL_PointInRect(
        &mousePoint,
        &listRect
    ))
    {
        return -1;
    }

    const int contentX = mouseX - listRect.x;
    const int contentY =
        mouseY - listRect.y + listState.scrollOffset;
    const int columnWidth =
        kCombatButtonWidth + kCombatColumnGap;
    const int row = contentY / kCombatRowStep;
    const int column = contentX / columnWidth;

    if (column < 0 || column > 1)
    {
        return -1;
    }

    const int localX =
        contentX - column * columnWidth;
    const int localY =
        contentY - row * kCombatRowStep;

    if (localX < 0
        || localX >= kCombatButtonWidth
        || localY < 0
        || localY >= kCombatButtonHeight)
    {
        return -1;
    }

    const int index = row * 2 + column;
    if (index < 0
        || index >= static_cast<int>(
            listState.regionIds.size()
        ))
    {
        return -1;
    }

    return index;
}

void DrawCombatRegionList(
    SDL_Renderer* renderer,
    const SDL_Rect& panelRect,
    const UiImageSet& images,
    const CombatRegionListState& listState
)
{
    const SDL_Rect listRect =
        GetCombatRegionListRect(panelRect);
    const int contentHeight =
        GetCombatRegionContentHeight(
            listState.regionIds.size()
        );

    SDL_RenderSetClipRect(renderer, &listRect);

    for (size_t index = 0;
         index < listState.regionIds.size();
         ++index)
    {
        const size_t row = index / 2;
        const size_t column = index % 2;
        const SDL_Rect buttonRect{
            listRect.x
                + static_cast<int>(column)
                    * (kCombatButtonWidth
                        + kCombatColumnGap),
            listRect.y
                + static_cast<int>(row)
                    * kCombatRowStep
                - listState.scrollOffset,
            kCombatButtonWidth,
            kCombatButtonHeight
        };
        const uint16_t regionId =
            listState.regionIds[index];
        SDL_Texture* buttonTexture =
            regionId == listState.selectedRegionId
            ? images.combatRegionButtonPressed
            : images.combatRegionButtonNormal;

        if (buttonTexture)
        {
            DrawUiImage(
                renderer,
                buttonTexture,
                buttonRect
            );
        }
        else
        {
            SDL_SetRenderDrawColor(
                renderer,
                15,
                45,
                50,
                SDL_ALPHA_OPAQUE
            );
            SDL_RenderFillRect(renderer, &buttonRect);
            SDL_SetRenderDrawColor(
                renderer,
                80,
                190,
                190,
                SDL_ALPHA_OPAQUE
            );
            SDL_RenderDrawRect(renderer, &buttonRect);
        }
    }

    if (images.combatRegionLabels)
    {
        const SDL_Rect sourceRect{
            0,
            listState.scrollOffset,
            kCombatListWidth,
            kCombatListHeight
        };
        const SDL_Rect destinationRect{
            listRect.x,
            listRect.y,
            kCombatListWidth,
            kCombatListHeight
        };
        SDL_RenderCopy(
            renderer,
            images.combatRegionLabels,
            &sourceRect,
            &destinationRect
        );
    }

    SDL_RenderSetClipRect(renderer, nullptr);

    const int maximumScroll =
        GetCombatRegionMaximumScroll(
            listState.regionIds.size()
        );
    if (maximumScroll <= 0)
    {
        return;
    }

    const SDL_Rect trackRect{
        listRect.x + kCombatListWidth
            + kCombatScrollBarGap,
        listRect.y,
        kCombatScrollBarWidth,
        kCombatListHeight
    };
    const int thumbHeight = std::max(
        kCombatScrollThumbMinimum,
        kCombatListHeight * kCombatListHeight
            / std::max(kCombatListHeight, contentHeight)
    );
    const int thumbTravel =
        kCombatListHeight - thumbHeight;
    const int thumbTop =
        thumbTravel * listState.scrollOffset
            / maximumScroll;
    const SDL_Rect thumbRect{
        trackRect.x,
        trackRect.y + thumbTop,
        trackRect.w,
        thumbHeight
    };

    SDL_SetRenderDrawColor(
        renderer,
        35,
        75,
        78,
        SDL_ALPHA_OPAQUE
    );
    SDL_RenderFillRect(renderer, &trackRect);
    SDL_SetRenderDrawColor(
        renderer,
        100,
        205,
        200,
        SDL_ALPHA_OPAQUE
    );
    SDL_RenderFillRect(renderer, &thumbRect);
}

void DrawInfoPanel(
    SDL_Renderer* renderer,
    int windowWidth,
    int windowHeight,
    SDL_Texture* infoTexture,
    const UiImageSet& images
)
{
    const SDL_Rect panel{
        windowWidth - kSidebarWidth,
        0,
        kSidebarWidth,
        windowHeight
    };
    if (images.sidebar)
    {
        DrawUiImage(renderer, images.sidebar, panel);
    }
    else
    {
        SDL_SetRenderDrawColor(renderer, 8, 20, 26, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(renderer, &panel);
    }

    const SDL_Rect card{
        panel.x,
        kInfoCardTop,
        kSidebarWidth,
        kInfoCardHeight
    };
    SDL_RenderCopy(renderer, infoTexture, nullptr, &card);
}

void DrawWindowBorder(
    SDL_Renderer* renderer,
    int windowWidth,
    int windowHeight,
    const UiImageSet& images
)
{
    DrawUiImage(
        renderer,
        images.border,
        SDL_Rect{0, 0, windowWidth, windowHeight}
    );
}

void BuildRegionBoundaryOverlay(
    const RegionMap& regionMap,
    std::vector<RgbaPixel>& output
)
{
    const size_t pixelCount = regionMap.regionIds.size();
    std::vector<uint8_t> boundaryMask(pixelCount, 0);

    for (uint32_t y = 0; y < regionMap.height; ++y)
    {
        for (uint32_t x = 0; x < regionMap.width; ++x)
        {
            const size_t index =
                static_cast<size_t>(y) * regionMap.width + x;
            const uint16_t regionId = regionMap.regionIds[index];

            if (regionId == 0)
            {
                continue;
            }

            const auto differs = [
                &regionMap,
                x,
                y,
                regionId
            ](int offsetX, int offsetY)
            {
                const int neighborX =
                    static_cast<int>(x) + offsetX;
                const int neighborY =
                    static_cast<int>(y) + offsetY;

                if (neighborX < 0
                    || neighborY < 0
                    || neighborX
                        >= static_cast<int>(regionMap.width)
                    || neighborY
                        >= static_cast<int>(regionMap.height))
                {
                    return true;
                }

                const size_t neighborIndex =
                    static_cast<size_t>(neighborY)
                        * regionMap.width
                    + static_cast<size_t>(neighborX);
                return regionMap.regionIds[neighborIndex]
                    != regionId;
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

    output.assign(pixelCount, RgbaPixel{0, 0, 0, 0});

    for (uint32_t y = 0; y < regionMap.height; ++y)
    {
        for (uint32_t x = 0; x < regionMap.width; ++x)
        {
            bool nearBoundary = false;

            for (int offsetY = -1; offsetY <= 1 && !nearBoundary; ++offsetY)
            {
                for (int offsetX = -1; offsetX <= 1; ++offsetX)
                {
                    const int sampleX =
                        static_cast<int>(x) + offsetX;
                    const int sampleY =
                        static_cast<int>(y) + offsetY;

                    if (sampleX < 0
                        || sampleY < 0
                        || sampleX
                            >= static_cast<int>(regionMap.width)
                        || sampleY
                            >= static_cast<int>(regionMap.height))
                    {
                        continue;
                    }

                    const size_t sampleIndex =
                        static_cast<size_t>(sampleY)
                            * regionMap.width
                        + static_cast<size_t>(sampleX);

                    if (boundaryMask[sampleIndex] != 0)
                    {
                        nearBoundary = true;
                        break;
                    }
                }
            }

            if (nearBoundary)
            {
                output[
                    static_cast<size_t>(y) * regionMap.width + x
                ] = {4, 15, 18, 235};
            }
        }
    }
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

    const SDL_Rect dirtyRect{
        static_cast<int>(bounds.minX),
        static_cast<int>(bounds.minY),
        static_cast<int>(bounds.maxX - bounds.minX + 1),
        static_cast<int>(bounds.maxY - bounds.minY + 1)
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

int Run(const fs::path& root)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    SDL_Window* window = SDL_CreateWindow(
        "China War Map",
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

    gui::GuiInterpreter guiInterpreter;
    std::string guiError;
    if (!guiInterpreter.LoadDirectory(root / "interface", guiError))
    {
        std::cerr << "Failed to load GUI definitions: "
                  << guiError << '\n';
    }
    else if (!guiError.empty())
    {
        std::cerr << "GUI definition warning: "
                  << guiError << '\n';
    }

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
    LoadUiImageSet(
        renderer,
        root,
        guiInterpreter,
        uiImages
    );

    UiFontSet uiFonts;
    LoadUiFontSet(root, uiFonts);

    RegionMap regionMap;
    if (!LoadRegionMap(regionMapPath, regionMap)
        || static_cast<int>(regionMap.width) != mapWidth
        || static_cast<int>(regionMap.height) != mapHeight)
    {
        std::cerr << "Failed to load or validate "
                  << regionMapPath << '\n';
        DestroyUiFontSet(uiFonts);
        DestroyUiImageSet(uiImages);
        SDL_DestroyTexture(baseTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    RegionPixelIndex regionPixelIndex;
    if (!BuildRegionPixelIndex(regionMap, regionPixelIndex))
    {
        std::cerr << "Failed to build Region pixel index\n";
        DestroyUiFontSet(uiFonts);
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
    SDL_Texture* hoverTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        mapWidth,
        mapHeight
    );

    if (!overlayTexture || !hoverTexture)
    {
        std::cerr << "Failed to create map overlay textures: "
                  << SDL_GetError() << '\n';
        SDL_DestroyTexture(hoverTexture);
        SDL_DestroyTexture(overlayTexture);
        DestroyUiFontSet(uiFonts);
        DestroyUiImageSet(uiImages);
        SDL_DestroyTexture(baseTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureBlendMode(overlayTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(hoverTexture, SDL_BLENDMODE_BLEND);

    std::vector<RgbaPixel> overlayPixels;
    std::vector<RgbaPixel> hoverPixels(
        regionMap.regionIds.size(),
        RgbaPixel{0, 0, 0, 0}
    );
    UpdateOverlayTexture(
        hoverTexture,
        hoverPixels,
        mapWidth
    );

    SDL_Texture* boundaryTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        mapWidth,
        mapHeight
    );

    if (!boundaryTexture)
    {
        std::cerr << "Failed to create Region boundary texture: "
                  << SDL_GetError() << '\n';
        SDL_DestroyTexture(hoverTexture);
        SDL_DestroyTexture(overlayTexture);
        DestroyUiFontSet(uiFonts);
        DestroyUiImageSet(uiImages);
        SDL_DestroyTexture(baseTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureBlendMode(boundaryTexture, SDL_BLENDMODE_BLEND);
    std::vector<RgbaPixel> boundaryPixels;
    BuildRegionBoundaryOverlay(regionMap, boundaryPixels);
    UpdateOverlayTexture(
        boundaryTexture,
        boundaryPixels,
        mapWidth
    );

    SDL_Texture* infoTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        kSidebarWidth,
        kInfoCardHeight
    );
    SDL_Texture* bannerTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        kInitialWindowWidth - kSidebarWidth,
        kBannerHeight
    );
    uiImages.legendText = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        kLegendWidth,
        kLegendHeight
    );
    uiImages.warProgressTitle = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        kLegendPanelWidth,
        kWarProgressTitleHeight
    );
    uiImages.combatRegionHeader = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        kLegendPanelWidth,
        kCombatHeaderHeight
    );
    uiImages.combatRegionLabels = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        kCombatListWidth,
        kCombatContentHeight
    );

    if (!infoTexture
        || !bannerTexture
        || !uiImages.legendText
        || !uiImages.warProgressTitle
        || !uiImages.combatRegionHeader
        || !uiImages.combatRegionLabels)
    {
        std::cerr << "Failed to create UI textures: "
                  << SDL_GetError() << '\n';
        SDL_DestroyTexture(bannerTexture);
        SDL_DestroyTexture(infoTexture);
        SDL_DestroyTexture(boundaryTexture);
        SDL_DestroyTexture(hoverTexture);
        SDL_DestroyTexture(overlayTexture);
        DestroyUiFontSet(uiFonts);
        DestroyUiImageSet(uiImages);
        SDL_DestroyTexture(baseTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureBlendMode(infoTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(bannerTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(uiImages.legendText, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(
        uiImages.warProgressTitle,
        SDL_BLENDMODE_BLEND
    );
    SDL_SetTextureBlendMode(
        uiImages.combatRegionHeader,
        SDL_BLENDMODE_BLEND
    );
    SDL_SetTextureBlendMode(
        uiImages.combatRegionLabels,
        SDL_BLENDMODE_BLEND
    );

    MockWarMapStateSource stateSource;
    WarMapState state;
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
    std::vector<uint8_t> infoCardPixels;
    std::vector<uint8_t> bannerPixels;
    std::vector<uint8_t> legendTextPixels;
    std::vector<uint8_t> warProgressTitlePixels;
    std::vector<uint8_t> combatRegionHeaderPixels;
    std::vector<uint8_t> combatRegionLabelPixels;
    CombatRegionListState combatRegionList;
    stateSource.Read(state);
    UpdateLegendTextTexture(
        uiImages.legendText,
        legendTextPixels,
        state,
        uiFonts
    );
    UpdateWarProgressTitleTexture(
        uiImages.warProgressTitle,
        warProgressTitlePixels,
        uiFonts
    );
    UpdateCombatRegionHeaderTexture(
        uiImages.combatRegionHeader,
        combatRegionHeaderPixels,
        uiFonts
    );
    combatRegionList.regionIds =
        BuildCombatRegionIds(state);
    UpdateCombatRegionLabelsTexture(
        uiImages.combatRegionLabels,
        combatRegionLabelPixels,
        combatRegionList.regionIds,
        uiFonts
    );
    bool running = true;

    while (running)
    {
        SDL_Event event{};

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT
                || (event.type == SDL_KEYDOWN
                    && event.key.keysym.sym == SDLK_ESCAPE))
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
                uint16_t newHoveredRegionId = PickRegion(
                    regionMap,
                    mapRect,
                    event.motion.x,
                    event.motion.y
                );

                if (newHoveredRegionId
                    >= regionPixelIndex.spansByRegion.size())
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

                const SDL_Rect warInfoPanelRect =
                    GetWarInfoPanelRect(mapRect);
                combatRegionList.hoveredIndex =
                    GetCombatRegionIndexAtPoint(
                        warInfoPanelRect,
                        combatRegionList,
                        event.motion.x,
                        event.motion.y
                    );
            }
            else if (event.type == SDL_MOUSEWHEEL)
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
                const SDL_Rect listRect =
                    GetCombatRegionListRect(
                        GetWarInfoPanelRect(mapRect)
                    );
                int mouseX = 0;
                int mouseY = 0;
                SDL_GetMouseState(&mouseX, &mouseY);

                if (mouseX >= listRect.x
                    && mouseX < listRect.x + listRect.w
                    && mouseY >= listRect.y
                    && mouseY < listRect.y + listRect.h)
                {
                    combatRegionList.scrollOffset -=
                        event.wheel.y * kCombatRowStep;
                    ClampCombatRegionScroll(combatRegionList);
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

                const MapRect mapRect = CalculateMapRect(
                    windowWidth,
                    windowHeight,
                    mapWidth,
                    mapHeight
                );
                const SDL_Rect warInfoPanelRect =
                    GetWarInfoPanelRect(mapRect);
                const int combatIndex =
                    GetCombatRegionIndexAtPoint(
                        warInfoPanelRect,
                        combatRegionList,
                        event.button.x,
                        event.button.y
                    );

                if (combatIndex >= 0)
                {
                    selectedRegionId =
                        combatRegionList.regionIds[
                            combatIndex
                        ];
                    combatRegionList.selectedRegionId =
                        selectedRegionId;
                    infoCardDirty = true;
                }
                else
                {
                    selectedRegionId = PickRegion(
                        regionMap,
                        mapRect,
                        event.button.x,
                        event.button.y
                    );

                    if (selectedRegionId != 0
                        && selectedRegionId
                            < regionPixelIndex
                                .spansByRegion.size())
                    {
                        infoCardDirty = true;
                        const float percentage =
                            selectedRegionId
                                < state.controlledPercentages.size()
                            ? state.controlledPercentages[
                                selectedRegionId
                            ]
                            : 0.0f;
                        const std::string title =
                            "China War Map - Region "
                            + std::to_string(selectedRegionId)
                            + " "
                            + FormatPercentage(percentage);
                        SDL_SetWindowTitle(
                            window,
                            title.c_str()
                        );
                    }
                }
            }
        }

        const Uint32 now = SDL_GetTicks();
        if (now >= nextStateUpdate)
        {
            stateSource.Read(state);
            infoCardDirty = true;

            const std::vector<uint16_t> currentCombatRegionIds =
                BuildCombatRegionIds(state);
            if (currentCombatRegionIds
                != combatRegionList.regionIds)
            {
                combatRegionList.regionIds =
                    currentCombatRegionIds;
                if (std::find(
                    combatRegionList.regionIds.begin(),
                    combatRegionList.regionIds.end(),
                    combatRegionList.selectedRegionId
                ) == combatRegionList.regionIds.end())
                {
                    combatRegionList.selectedRegionId = 0;
                }
                ClampCombatRegionScroll(combatRegionList);
                UpdateCombatRegionLabelsTexture(
                    uiImages.combatRegionLabels,
                    combatRegionLabelPixels,
                    combatRegionList.regionIds,
                    uiFonts
                );
            }

            if (state.viewerTag != previousViewerTag)
            {
                previousViewerTag = state.viewerTag;
                bannerDirty = true;
                UpdateLegendTextTexture(
                    uiImages.legendText,
                    legendTextPixels,
                    state,
                    uiFonts
                );
            }

            const std::vector<float> currentPercentages =
                ToPercentageVector(state);
            bool uploadFullTexture = false;
            bool overlayChanged = false;

            if (!hasPreviousOverlay
                || state.visible != previousVisible
                || state.active != previousActive)
            {
                BuildOverlayFromState(
                    regionMap,
                    state,
                    overlayPixels
                );
                uploadFullTexture = true;
                overlayChanged = true;
            }
            else if (state.visible && state.active)
            {
                overlayChanged = UpdateChangedRegionOverlay(
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
                            regionPixelIndex
                                .boundsByRegion[regionId]
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
                selectedRegionId,
                uiFonts
            );
            infoCardDirty = false;
        }

        if (bannerDirty)
        {
            UpdateWarBannerTexture(
                bannerTexture,
                bannerPixels,
                state,
                kInitialWindowWidth - kSidebarWidth,
                uiFonts
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
        const SDL_Rect destination{
            mapRect.x,
            mapRect.y,
            mapRect.width,
            mapRect.height
        };

        SDL_SetRenderDrawColor(
            renderer,
            0,
            0,
            0,
            SDL_ALPHA_OPAQUE
        );
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, baseTexture, nullptr, &destination);
        SDL_RenderCopy(renderer, overlayTexture, nullptr, &destination);
        SDL_RenderCopy(renderer, boundaryTexture, nullptr, &destination);
        SDL_RenderCopy(renderer, hoverTexture, nullptr, &destination);

        if (uiImages.legend)
        {
            const SDL_Rect legendRect{
                kLegendLeft,
                mapRect.y + kLegendOffsetY,
                kLegendWidth,
                kLegendHeight
            };
            DrawUiImage(renderer, uiImages.legend, legendRect);
            DrawLegendSwatches(renderer, legendRect);
            DrawUiImage(renderer, uiImages.legendText, legendRect);

            if (uiImages.legendPanel)
            {
                const SDL_Rect legendPanelRect =
                    GetWarInfoPanelRect(mapRect);
                DrawUiImage(
                    renderer,
                    uiImages.legendPanel,
                    legendPanelRect
                );
                DrawWarProgressBar(
                    renderer,
                    uiImages.warProgress,
                    legendPanelRect,
                    state.warProgress
                );
                const SDL_Rect warProgressTitleRect{
                    legendPanelRect.x,
                    legendPanelRect.y,
                    kLegendPanelWidth,
                    kWarProgressTitleHeight
                };
                DrawUiImage(
                    renderer,
                    uiImages.warProgressTitle,
                    warProgressTitleRect
                );
                const SDL_Rect combatHeaderRect{
                    legendPanelRect.x,
                    legendPanelRect.y + kCombatHeaderTop,
                    kLegendPanelWidth,
                    kCombatHeaderHeight
                };
                DrawUiImage(
                    renderer,
                    uiImages.combatRegionHeader,
                    combatHeaderRect
                );
                DrawCombatRegionList(
                    renderer,
                    legendPanelRect,
                    uiImages,
                    combatRegionList
                );
            }
        }

        DrawInfoPanel(
            renderer,
            windowWidth,
            windowHeight,
            infoTexture,
            uiImages
        );

        const SDL_Rect bannerRect{
            0,
            kBannerTop,
            std::max(1, windowWidth - kSidebarWidth),
            kBannerHeight
        };
        SDL_RenderCopy(
            renderer,
            bannerTexture,
            nullptr,
            &bannerRect
        );

        DrawWindowBorder(
            renderer,
            windowWidth,
            windowHeight,
            uiImages
        );
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(bannerTexture);
    SDL_DestroyTexture(infoTexture);
    SDL_DestroyTexture(boundaryTexture);
    SDL_DestroyTexture(hoverTexture);
    SDL_DestroyTexture(overlayTexture);
    DestroyUiFontSet(uiFonts);
    DestroyUiImageSet(uiImages);
    SDL_DestroyTexture(baseTexture);
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
