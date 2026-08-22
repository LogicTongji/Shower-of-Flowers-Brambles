#include "gui_texture_loader_d3d9.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cwctype>
#include <cstring>
#include <gdiplus.h>

namespace
{

using D3DXCreateTextureFromFileWFunction = HRESULT (WINAPI*)(
    IDirect3DDevice9*,
    LPCWSTR,
    IDirect3DTexture9**
);

D3DXCreateTextureFromFileWFunction ResolveDdsTextureLoader()
{
    static D3DXCreateTextureFromFileWFunction function = []()
    {
        for (int version = 43; version >= 24; --version)
        {
            const std::wstring name = L"d3dx9_"
                + std::to_wstring(version) + L".dll";
            HMODULE module = LoadLibraryW(name.c_str());
            if (!module)
            {
                continue;
            }
            const auto resolved = reinterpret_cast<
                D3DXCreateTextureFromFileWFunction
            >(GetProcAddress(module, "D3DXCreateTextureFromFileW"));
            if (resolved)
            {
                return resolved;
            }
            FreeLibrary(module);
        }
        return D3DXCreateTextureFromFileWFunction{};
    }();
    return function;
}

bool LoadDdsTexture(
    IDirect3DDevice9* device,
    const std::filesystem::path& path,
    GuiD3D9Texture& output,
    std::string& error
)
{
    const D3DXCreateTextureFromFileWFunction loadTexture =
        ResolveDdsTextureLoader();
    if (!loadTexture)
    {
        error = "DirectX 9 DDS texture loader is unavailable: "
            + path.string();
        return false;
    }

    IDirect3DTexture9* texture = nullptr;
    if (FAILED(loadTexture(device, path.wstring().c_str(), &texture))
        || !texture)
    {
        error = "Failed to decode DDS GUI texture: " + path.string();
        return false;
    }
    D3DSURFACE_DESC description{};
    if (FAILED(texture->GetLevelDesc(0, &description)))
    {
        texture->Release();
        error = "Failed to inspect DDS GUI texture: " + path.string();
        return false;
    }
    output.texture = texture;
    output.width = static_cast<int>(description.Width);
    output.height = static_cast<int>(description.Height);
    error.clear();
    return true;
}

}

GuiD3D9Texture::~GuiD3D9Texture()
{
    Reset();
}

GuiD3D9Texture::GuiD3D9Texture(
    GuiD3D9Texture&& other
) noexcept
    : texture(other.texture),
      width(other.width),
      height(other.height)
{
    other.texture = nullptr;
    other.width = 0;
    other.height = 0;
}

GuiD3D9Texture& GuiD3D9Texture::operator=(
    GuiD3D9Texture&& other
) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    Reset();
    texture = other.texture;
    width = other.width;
    height = other.height;
    other.texture = nullptr;
    other.width = 0;
    other.height = 0;
    return *this;
}

void GuiD3D9Texture::Reset()
{
    if (texture)
    {
        texture->Release();
    }
    texture = nullptr;
    width = 0;
    height = 0;
}

bool LoadGuiD3D9Texture(
    IDirect3DDevice9* device,
    const std::filesystem::path& path,
    GuiD3D9Texture& output,
    std::string& error
)
{
    output.Reset();
    if (!device || path.empty())
    {
        error = "D3D9 texture request is invalid";
        return false;
    }

    std::wstring extension = path.extension().wstring();
    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](wchar_t character)
        {
            return static_cast<wchar_t>(std::towlower(character));
        }
    );
    if (extension == L".dds")
    {
        return LoadDdsTexture(device, path, output, error);
    }

    Gdiplus::Bitmap bitmap(path.wstring().c_str(), FALSE);
    if (bitmap.GetLastStatus() != Gdiplus::Ok
        || bitmap.GetWidth() == 0
        || bitmap.GetHeight() == 0)
    {
        error = "Failed to decode GUI texture: " + path.string();
        return false;
    }

    IDirect3DTexture9* texture = nullptr;
    const UINT width = bitmap.GetWidth();
    const UINT height = bitmap.GetHeight();
    const HRESULT createResult = device->CreateTexture(
        width,
        height,
        1,
        0,
        D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED,
        &texture,
        nullptr
    );
    if (FAILED(createResult) || !texture)
    {
        error = "Failed to create D3D9 GUI texture: "
            + path.string();
        return false;
    }

    Gdiplus::Rect sourceRect(
        0,
        0,
        static_cast<INT>(width),
        static_cast<INT>(height)
    );
    Gdiplus::BitmapData bitmapData{};
    if (bitmap.LockBits(
            &sourceRect,
            Gdiplus::ImageLockModeRead,
            PixelFormat32bppARGB,
            &bitmapData
        ) != Gdiplus::Ok)
    {
        texture->Release();
        error = "Failed to lock decoded GUI texture: "
            + path.string();
        return false;
    }

    D3DLOCKED_RECT destination{};
    const HRESULT lockResult = texture->LockRect(
        0,
        &destination,
        nullptr,
        0
    );
    if (FAILED(lockResult))
    {
        bitmap.UnlockBits(&bitmapData);
        texture->Release();
        error = "Failed to upload D3D9 GUI texture: "
            + path.string();
        return false;
    }

    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
    const auto* sourceBase = static_cast<const uint8_t*>(
        bitmapData.Scan0
    );
    for (UINT y = 0; y < height; ++y)
    {
        const std::size_t sourceY = bitmapData.Stride >= 0
            ? y
            : height - y - 1;
        const uint8_t* source = sourceBase
            + static_cast<std::ptrdiff_t>(sourceY)
                * std::abs(bitmapData.Stride);
        auto* target = static_cast<uint8_t*>(destination.pBits)
            + static_cast<std::size_t>(y) * destination.Pitch;
        std::memcpy(target, source, rowBytes);
    }

    texture->UnlockRect(0);
    bitmap.UnlockBits(&bitmapData);
    output.texture = texture;
    output.width = static_cast<int>(width);
    output.height = static_cast<int>(height);
    return true;
}
