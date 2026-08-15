#pragma once

#include <d3d9.h>

#include <filesystem>
#include <string>

struct GuiD3D9Texture
{
    GuiD3D9Texture() = default;
    ~GuiD3D9Texture();
    GuiD3D9Texture(const GuiD3D9Texture&) = delete;
    GuiD3D9Texture& operator=(const GuiD3D9Texture&) = delete;
    GuiD3D9Texture(GuiD3D9Texture&& other) noexcept;
    GuiD3D9Texture& operator=(GuiD3D9Texture&& other) noexcept;

    IDirect3DTexture9* texture = nullptr;
    int width = 0;
    int height = 0;

    void Reset();
};

bool LoadGuiD3D9Texture(
    IDirect3DDevice9* device,
    const std::filesystem::path& path,
    GuiD3D9Texture& output,
    std::string& error
);
