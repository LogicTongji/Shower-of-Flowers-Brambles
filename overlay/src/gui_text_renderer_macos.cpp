#include "gui_text_renderer_macos.h"

#include <CoreText/CoreText.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <system_error>
#include <utility>

namespace
{

std::string NormalizeFontName(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        }
    );
    return value;
}

CGFontRef LoadFontFile(
    const std::filesystem::path& path
)
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

CTFontRef CreateGuiFont(
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

void DrawGuiTextLine(
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

    CTFontRef font = CreateGuiFont(fontSize, fontSource);
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

CGFloat MeasureGuiTextLine(
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

    CTFontRef font = CreateGuiFont(fontSize, fontSource);
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

void DrawGuiWrappedText(
    CGContextRef context,
    int canvasHeight,
    const gui::GuiTextCommand& command,
    int originX,
    int originY,
    CGFontRef fontSource
)
{
    CFStringRef text = CFStringCreateWithCString(
        kCFAllocatorDefault,
        command.text.c_str(),
        kCFStringEncodingUTF8
    );
    if (!text)
    {
        return;
    }
    CTFontRef font = CreateGuiFont(
        static_cast<CGFloat>(command.fontSize),
        fontSource
    );
    CGColorRef color = CGColorCreateGenericRGB(
        command.color[0],
        command.color[1],
        command.color[2],
        1.0
    );
    CTTextAlignment alignment = kCTTextAlignmentLeft;
    if (command.alignment == gui::GuiTextAlignment::Center)
    {
        alignment = kCTTextAlignmentCenter;
    }
    else if (command.alignment == gui::GuiTextAlignment::Right)
    {
        alignment = kCTTextAlignmentRight;
    }
    const CGFloat lineSpacing = static_cast<CGFloat>(
        std::max(0, command.lineSpacing)
    );
    const CTParagraphStyleSetting settings[] = {
        {
            kCTParagraphStyleSpecifierAlignment,
            sizeof(alignment),
            &alignment
        },
        {
            kCTParagraphStyleSpecifierLineSpacingAdjustment,
            sizeof(lineSpacing),
            &lineSpacing
        }
    };
    CTParagraphStyleRef paragraph = CTParagraphStyleCreate(
        settings,
        sizeof(settings) / sizeof(settings[0])
    );
    const void* keys[] = {
        kCTFontAttributeName,
        kCTForegroundColorAttributeName,
        kCTParagraphStyleAttributeName
    };
    const void* values[] = {font, color, paragraph};
    CFDictionaryRef attributes = CFDictionaryCreate(
        kCFAllocatorDefault,
        keys,
        values,
        3,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    CFAttributedStringRef attributed = CFAttributedStringCreate(
        kCFAllocatorDefault,
        text,
        attributes
    );
    CTFramesetterRef framesetter = attributed
        ? CTFramesetterCreateWithAttributedString(attributed)
        : nullptr;
    const CGFloat x = static_cast<CGFloat>(
        command.rect.x - originX
    );
    const CGFloat y = static_cast<CGFloat>(canvasHeight)
        - static_cast<CGFloat>(command.rect.y - originY)
        - static_cast<CGFloat>(command.rect.height);
    CGMutablePathRef path = CGPathCreateMutable();
    CGPathAddRect(
        path,
        nullptr,
        CGRectMake(
            x,
            y,
            static_cast<CGFloat>(command.rect.width),
            static_cast<CGFloat>(command.rect.height)
        )
    );
    CTFrameRef frame = framesetter
        ? CTFramesetterCreateFrame(
            framesetter,
            CFRangeMake(0, 0),
            path,
            nullptr
        )
        : nullptr;
    if (frame)
    {
        if (fontSource)
        {
            CGContextSetAllowsAntialiasing(context, false);
            CGContextSetShouldAntialias(context, false);
            CGContextSetShouldSmoothFonts(context, false);
        }
        CTFrameDraw(frame, context);
    }
    if (frame) CFRelease(frame);
    CGPathRelease(path);
    if (framesetter) CFRelease(framesetter);
    if (attributed) CFRelease(attributed);
    if (attributes) CFRelease(attributes);
    if (paragraph) CFRelease(paragraph);
    CGColorRelease(color);
    if (font) CFRelease(font);
    CFRelease(text);
}

CGFontRef SelectGuiFont(
    const gui::GuiTextCommand& command,
    const GuiMacFontSet& fonts
)
{
    const auto iterator = fonts.byName.find(
        NormalizeFontName(command.font)
    );
    return iterator == fonts.byName.end()
        ? nullptr
        : iterator->second;
}

void DrawGuiTextCommands(
    CGContextRef context,
    int canvasHeight,
    const std::vector<gui::GuiTextCommand>& commands,
    int originX,
    int originY,
    const GuiMacFontSet& fonts
)
{
    for (const gui::GuiTextCommand& command : commands)
    {
        const CGFontRef font = SelectGuiFont(command, fonts);
        if (command.wrap)
        {
            DrawGuiWrappedText(
                context,
                canvasHeight,
                command,
                originX,
                originY,
                font
            );
            continue;
        }
        const CGFloat fontSize = static_cast<CGFloat>(
            command.fontSize
        );
        const CGFloat textWidth = MeasureGuiTextLine(
            command.text,
            fontSize,
            font
        );
        CGFloat textX = static_cast<CGFloat>(
            command.rect.x - originX
        );
        if (command.alignment == gui::GuiTextAlignment::Center)
        {
            textX += (
                static_cast<CGFloat>(command.rect.width)
                - textWidth
            ) / 2.0;
        }
        else if (command.alignment == gui::GuiTextAlignment::Right)
        {
            textX += static_cast<CGFloat>(command.rect.width)
                - textWidth;
        }

        const CGFloat textY = static_cast<CGFloat>(
            command.rect.y - originY
        ) + std::max(
            0.0,
            (static_cast<CGFloat>(command.rect.height) - fontSize)
                / 2.0
        );
        DrawGuiTextLine(
            context,
            canvasHeight,
            command.text,
            textX,
            textY,
            fontSize,
            command.color[0],
            command.color[1],
            command.color[2],
            font
        );
    }
}

bool CreateGuiTextContext(
    std::vector<uint8_t>& pixels,
    int width,
    int height,
    CGContextRef& context
)
{
    pixels.assign(
        static_cast<size_t>(width)
            * static_cast<size_t>(height)
            * 4,
        0
    );

    CGColorSpaceRef colorSpace =
        CGColorSpaceCreateDeviceRGB();
    context = CGBitmapContextCreate(
        pixels.data(),
        width,
        height,
        8,
        width * 4,
        colorSpace,
        kCGImageAlphaPremultipliedLast
            | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colorSpace);
    return context != nullptr;
}

}

bool LoadGuiMacFontDirectory(
    const std::filesystem::path& root,
    GuiMacFontSet& fonts,
    std::string& error
)
{
    DestroyGuiMacFontSet(fonts);
    error.clear();

    std::error_code filesystemError;
    if (!std::filesystem::exists(root, filesystemError)
        || !std::filesystem::is_directory(root, filesystemError))
    {
        error = "font_directory_not_found: " + root.string();
        return false;
    }

    for (const std::filesystem::directory_entry& entry
        : std::filesystem::recursive_directory_iterator(
            root,
            filesystemError
        ))
    {
        if (filesystemError)
        {
            error = "font_directory_read_failed: "
                + root.string();
            DestroyGuiMacFontSet(fonts);
            return false;
        }

        if (!entry.is_regular_file())
        {
            continue;
        }

        const std::string extension = NormalizeFontName(
            entry.path().extension().string()
        );
        if (extension != ".ttf" && extension != ".otf")
        {
            continue;
        }

        CGFontRef font = LoadFontFile(entry.path());
        if (!font)
        {
            continue;
        }

        const std::string name = NormalizeFontName(
            entry.path().stem().string()
        );
        const auto existing = fonts.byName.find(name);
        if (existing != fonts.byName.end())
        {
            CGFontRelease(existing->second);
        }
        fonts.byName[name] = font;
    }

    if (fonts.byName.empty())
    {
        error = "no_fonts_loaded: " + root.string();
        return false;
    }

    return true;
}

void DestroyGuiMacFontSet(
    GuiMacFontSet& fonts
)
{
    for (const auto& entry : fonts.byName)
    {
        if (entry.second)
        {
            CGFontRelease(entry.second);
        }
    }
    fonts.byName.clear();
}

void UpdateGuiTextTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const GuiWindowRuntime& windowRuntime,
    const GuiMacFontSet& fonts,
    int width,
    int height,
    const gui::GuiLayoutContext& layoutContext
)
{
    if (!texture || width <= 0 || height <= 0)
    {
        return;
    }

    CGContextRef context = nullptr;
    if (!CreateGuiTextContext(pixels, width, height, context))
    {
        return;
    }

    DrawGuiTextCommands(
        context,
        height,
        windowRuntime.BuildTextCommands(layoutContext),
        0,
        0,
        fonts
    );
    CGContextRelease(context);
    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        width * 4
    );
}

void UpdateGuiTextCommandTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const gui::GuiTextCommand& command,
    const GuiMacFontSet& fonts
)
{
    const int width = command.rect.width;
    const int height = command.rect.height;
    if (!texture || width <= 0 || height <= 0)
    {
        return;
    }

    CGContextRef context = nullptr;
    if (!CreateGuiTextContext(pixels, width, height, context))
    {
        return;
    }

    gui::GuiTextCommand localCommand = command;
    localCommand.rect = {0, 0, width, height};
    DrawGuiTextCommands(
        context,
        height,
        std::vector<gui::GuiTextCommand>{std::move(localCommand)},
        0,
        0,
        fonts
    );
    CGContextRelease(context);
    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        width * 4
    );
}

void UpdateGuiListTextTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const GuiWindowRuntime& windowRuntime,
    std::string_view listName,
    const GuiListRuntimeLayout& layout,
    const GuiMacFontSet& fonts,
    const gui::GuiLayoutContext& layoutContext
)
{
    if (!texture
        || layout.viewport.width <= 0
        || layout.viewport.height <= 0)
    {
        return;
    }

    int width = 0;
    int height = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);
    if (width <= 0 || height <= 0)
    {
        return;
    }

    CGContextRef context = nullptr;
    if (!CreateGuiTextContext(pixels, width, height, context))
    {
        return;
    }

    DrawGuiTextCommands(
        context,
        height,
        windowRuntime.BuildListTextCommands(
            listName,
            layoutContext
        ),
        layout.viewport.x,
        layout.viewport.y,
        fonts
    );
    CGContextRelease(context);
    SDL_UpdateTexture(
        texture,
        nullptr,
        pixels.data(),
        width * 4
    );
}
