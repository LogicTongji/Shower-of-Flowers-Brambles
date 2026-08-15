#include "gui_text_renderer_d3d9.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <gdiplus.h>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gui_texture_loader_d3d9.h"

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

std::wstring Utf8ToWide(std::string_view value)
{
    if (value.empty())
    {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0
    );
    if (length <= 0)
    {
        return {};
    }
    std::wstring output(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        output.data(),
        length
    );
    return output;
}

uint8_t ToByte(float value)
{
    return static_cast<uint8_t>(
        std::clamp(value, 0.0f, 1.0f) * 255.0f
    );
}

std::string BuildSignature(const gui::GuiTextCommand& command)
{
    std::ostringstream output;
    output
        << command.text << '\n'
        << NormalizeFontName(command.font) << '\n'
        << command.fontSize << ':'
        << command.rect.width << ':'
        << command.rect.height << ':'
        << static_cast<int>(command.alignment) << ':'
        << command.lineSpacing << ':'
        << command.wrap << ':'
        << ToByte(command.color[0]) << ':'
        << ToByte(command.color[1]) << ':'
        << ToByte(command.color[2]);
    return output.str();
}

}

struct GuiTextRendererD3D9::Impl
{
    struct FontSource
    {
        std::unique_ptr<Gdiplus::PrivateFontCollection> collection;
        std::wstring familyName;
    };

    struct TextTexture
    {
        GuiD3D9Texture texture;
        std::string signature;
    };

    IDirect3DDevice9* device = nullptr;
    std::unordered_map<std::string, FontSource> fonts;
    std::unordered_map<std::string, TextTexture> textures;
    std::unordered_set<std::string> activeSlots;

    const FontSource* FindFont(std::string_view name) const
    {
        const auto found = fonts.find(
            NormalizeFontName(std::string(name))
        );
        return found == fonts.end() ? nullptr : &found->second;
    }

    bool LoadFonts(
        const std::filesystem::path& root,
        std::string& error
    )
    {
        std::error_code filesystemError;
        if (!std::filesystem::is_directory(root, filesystemError))
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
                error = "font_directory_read_failed: " + root.string();
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

            FontSource source;
            source.collection =
                std::make_unique<Gdiplus::PrivateFontCollection>();
            if (source.collection->AddFontFile(
                    entry.path().wstring().c_str()
                ) != Gdiplus::Ok)
            {
                continue;
            }
            const INT familyCount = source.collection->GetFamilyCount();
            if (familyCount <= 0)
            {
                continue;
            }
            std::vector<Gdiplus::FontFamily> families(
                static_cast<std::size_t>(familyCount)
            );
            INT foundCount = 0;
            if (source.collection->GetFamilies(
                    familyCount,
                    families.data(),
                    &foundCount
                ) != Gdiplus::Ok
                || foundCount <= 0)
            {
                continue;
            }
            wchar_t familyName[LF_FACESIZE]{};
            if (families.front().GetFamilyName(familyName)
                != Gdiplus::Ok)
            {
                continue;
            }
            source.familyName = familyName;
            fonts[NormalizeFontName(
                entry.path().stem().string()
            )] = std::move(source);
        }
        if (fonts.empty())
        {
            error = "no_fonts_loaded: " + root.string();
            return false;
        }
        return true;
    }

    bool Render(
        const gui::GuiTextCommand& command,
        GuiD3D9Texture& output
    ) const
    {
        output.Reset();
        const int width = command.rect.width;
        const int height = command.rect.height;
        if (!device || width <= 0 || height <= 0)
        {
            return false;
        }

        Gdiplus::Bitmap bitmap(
            width,
            height,
            PixelFormat32bppARGB
        );
        Gdiplus::Graphics graphics(&bitmap);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
        graphics.SetTextRenderingHint(
            Gdiplus::TextRenderingHintAntiAliasGridFit
        );
        graphics.SetCompositingMode(
            Gdiplus::CompositingModeSourceOver
        );

        const FontSource* source = FindFont(command.font);
        std::unique_ptr<Gdiplus::FontFamily> privateFamily;
        const Gdiplus::FontFamily* family =
            Gdiplus::FontFamily::GenericSansSerif();
        if (source)
        {
            privateFamily = std::make_unique<Gdiplus::FontFamily>(
                source->familyName.c_str(),
                source->collection.get()
            );
            if (privateFamily->IsAvailable())
            {
                family = privateFamily.get();
            }
        }
        Gdiplus::Font font(
            family,
            static_cast<Gdiplus::REAL>(std::max(1, command.fontSize)),
            Gdiplus::FontStyleRegular,
            Gdiplus::UnitPixel
        );
        if (!font.IsAvailable())
        {
            return false;
        }

        Gdiplus::StringFormat format;
        if (command.alignment == gui::GuiTextAlignment::Center)
        {
            format.SetAlignment(Gdiplus::StringAlignmentCenter);
        }
        else if (command.alignment == gui::GuiTextAlignment::Right)
        {
            format.SetAlignment(Gdiplus::StringAlignmentFar);
        }
        else
        {
            format.SetAlignment(Gdiplus::StringAlignmentNear);
        }
        format.SetLineAlignment(
            command.wrap
                ? Gdiplus::StringAlignmentNear
                : Gdiplus::StringAlignmentCenter
        );
        format.SetFormatFlags(
            command.wrap
                ? 0
                : Gdiplus::StringFormatFlagsNoWrap
        );
        format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

        const std::wstring text = Utf8ToWide(command.text);
        Gdiplus::SolidBrush brush(Gdiplus::Color(
            255,
            ToByte(command.color[0]),
            ToByte(command.color[1]),
            ToByte(command.color[2])
        ));
        graphics.DrawString(
            text.c_str(),
            static_cast<INT>(text.size()),
            &font,
            Gdiplus::RectF(
                0.0f,
                0.0f,
                static_cast<Gdiplus::REAL>(width),
                static_cast<Gdiplus::REAL>(height)
            ),
            &format,
            &brush
        );

        IDirect3DTexture9* texture = nullptr;
        if (FAILED(device->CreateTexture(
                static_cast<UINT>(width),
                static_cast<UINT>(height),
                1,
                0,
                D3DFMT_A8R8G8B8,
                D3DPOOL_MANAGED,
                &texture,
                nullptr
            )))
        {
            return false;
        }

        Gdiplus::Rect lockBounds(0, 0, width, height);
        Gdiplus::BitmapData sourcePixels{};
        if (bitmap.LockBits(
                &lockBounds,
                Gdiplus::ImageLockModeRead,
                PixelFormat32bppARGB,
                &sourcePixels
            ) != Gdiplus::Ok)
        {
            texture->Release();
            return false;
        }
        D3DLOCKED_RECT destination{};
        if (FAILED(texture->LockRect(0, &destination, nullptr, 0)))
        {
            bitmap.UnlockBits(&sourcePixels);
            texture->Release();
            return false;
        }
        const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
        const auto* sourceBase = static_cast<const uint8_t*>(
            sourcePixels.Scan0
        );
        for (int y = 0; y < height; ++y)
        {
            const int sourceY = sourcePixels.Stride >= 0
                ? y
                : height - y - 1;
            const uint8_t* sourceRow = sourceBase
                + static_cast<std::ptrdiff_t>(sourceY)
                    * std::abs(sourcePixels.Stride);
            auto* destinationRow = static_cast<uint8_t*>(
                destination.pBits
            ) + static_cast<std::size_t>(y) * destination.Pitch;
            std::memcpy(destinationRow, sourceRow, rowBytes);
        }
        texture->UnlockRect(0);
        bitmap.UnlockBits(&sourcePixels);
        output.texture = texture;
        output.width = width;
        output.height = height;
        return true;
    }
};

GuiTextRendererD3D9::GuiTextRendererD3D9()
    : impl_(std::make_unique<Impl>())
{
}

GuiTextRendererD3D9::~GuiTextRendererD3D9()
{
    Shutdown();
}

bool GuiTextRendererD3D9::Initialize(
    const std::filesystem::path& fontRoot,
    IDirect3DDevice9* device,
    std::string& error
)
{
    Shutdown();
    impl_->device = device;
    if (!device || !impl_->LoadFonts(fontRoot, error))
    {
        Shutdown();
        return false;
    }
    return true;
}

void GuiTextRendererD3D9::Shutdown()
{
    impl_->activeSlots.clear();
    impl_->textures.clear();
    impl_->fonts.clear();
    impl_->device = nullptr;
}

void GuiTextRendererD3D9::BeginFrame()
{
    impl_->activeSlots.clear();
}

IDirect3DTexture9* GuiTextRendererD3D9::Resolve(
    std::string slot,
    const gui::GuiTextCommand& command
)
{
    if (slot.empty() || command.text.empty())
    {
        return nullptr;
    }
    impl_->activeSlots.insert(slot);
    const std::string signature = BuildSignature(command);
    Impl::TextTexture& cached = impl_->textures[slot];
    if (cached.signature == signature && cached.texture.texture)
    {
        return cached.texture.texture;
    }
    if (!impl_->Render(command, cached.texture))
    {
        cached.signature.clear();
        return nullptr;
    }
    cached.signature = signature;
    return cached.texture.texture;
}

void GuiTextRendererD3D9::EndFrame()
{
    for (auto iterator = impl_->textures.begin();
        iterator != impl_->textures.end();)
    {
        if (impl_->activeSlots.find(iterator->first)
            != impl_->activeSlots.end())
        {
            ++iterator;
        }
        else
        {
            iterator = impl_->textures.erase(iterator);
        }
    }
}
