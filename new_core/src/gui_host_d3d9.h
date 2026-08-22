#pragma once

#include <d3d9.h>
#include <windows.h>

#include <filesystem>
#include <memory>
#include <string>

class GuiD3D9Host
{
public:
    GuiD3D9Host();
    ~GuiD3D9Host();

    bool Initialize(
        const std::filesystem::path& root,
        IDirect3DDevice9* device,
        std::string& error
    );

    void Shutdown();
    void TickAndRender(IDirect3DDevice9* device);
    void BeforeDeviceReset();
    bool AfterDeviceReset(
        IDirect3DDevice9* device,
        std::string& error
    );

    bool HandleWindowMessage(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    );

    bool IsInitialized() const;
    bool UsesDevice(IDirect3DDevice9* device) const;
    HWND TargetWindow() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
