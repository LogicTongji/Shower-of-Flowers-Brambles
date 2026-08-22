#pragma once

#include <d3d9.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "gui_interpreter.h"
#include "gui_runtime.h"

struct GuiIndexedMapD3D9DrawLayers
{
    gui::GuiRect rect;
    IDirect3DTexture9* base = nullptr;
    IDirect3DTexture9* overlay = nullptr;
    IDirect3DTexture9* boundary = nullptr;
    IDirect3DTexture9* hover = nullptr;
};

class GuiIndexedMapD3D9Runtime
{
public:
    GuiIndexedMapD3D9Runtime();
    ~GuiIndexedMapD3D9Runtime();

    bool Initialize(
        const std::filesystem::path& root,
        IDirect3DDevice9* device,
        const gui::GuiInterpreter& interpreter,
        const gui::WindowDefinition& window,
        std::string& error
    );

    void Shutdown();
    void Refresh(const gui::GuiLayoutContext& context);

    bool ResolveDrawLayers(
        const gui::GuiResolvedWidget& widget,
        GuiIndexedMapD3D9DrawLayers& output
    ) const;

    bool ResolveDrawRect(
        const gui::GuiResolvedWidget& widget,
        gui::GuiRect& rect
    ) const;

    bool ResolveItemAnchor(
        const gui::GuiResolvedWidget& widget,
        uint16_t itemId,
        int& x,
        int& y
    ) const;

    void HandleMove(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        int mouseX,
        int mouseY
    );

    void HandlePress(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        int mouseX,
        int mouseY
    );

    void HandleRelease(
        const std::vector<gui::GuiResolvedWidget>& widgets,
        int mouseX,
        int mouseY
    );

    void AttachItemIds(std::vector<GuiActionEvent>& events) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
