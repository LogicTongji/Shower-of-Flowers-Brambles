#pragma once

#include <SDL.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "gui_interpreter.h"
#include "gui_runtime.h"

struct GuiIndexedMapMacResourceStats
{
    std::size_t textureCount = 0;
    uint64_t textureBytes = 0;
    uint64_t cpuBytes = 0;
};

class GuiIndexedMapMacRuntime
{
public:
    GuiIndexedMapMacRuntime();
    ~GuiIndexedMapMacRuntime();

    GuiIndexedMapMacRuntime(const GuiIndexedMapMacRuntime&) = delete;
    GuiIndexedMapMacRuntime& operator=(
        const GuiIndexedMapMacRuntime&
    ) = delete;

    bool Initialize(
        const std::filesystem::path& root,
        SDL_Renderer* renderer,
        const gui::GuiInterpreter& interpreter,
        const gui::WindowDefinition& window,
        std::string& error
    );

    void Shutdown();

    void Refresh(const gui::GuiLayoutContext& context);

    void Draw(
        const std::vector<gui::GuiResolvedWidget>& widgets
    ) const;

    bool DrawWidget(const gui::GuiResolvedWidget& widget) const;

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

    void AttachItemIds(
        std::vector<GuiActionEvent>& events
    ) const;

    GuiIndexedMapMacResourceStats ResourceStats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
