#pragma once

#include <d3d9.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "gui_interpreter.h"

class GuiTextRendererD3D9
{
public:
    GuiTextRendererD3D9();
    ~GuiTextRendererD3D9();

    bool Initialize(
        const std::filesystem::path& fontRoot,
        IDirect3DDevice9* device,
        std::string& error
    );

    void Shutdown();
    void BeginFrame();

    IDirect3DTexture9* Resolve(
        std::string slot,
        const gui::GuiTextCommand& command
    );

    void EndFrame();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
