#pragma once

#include <SDL.h>

#include <CoreGraphics/CoreGraphics.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gui_interpreter.h"
#include "gui_runtime.h"

struct GuiMacFontSet
{
    std::unordered_map<std::string, CGFontRef> byName;
};

bool LoadGuiMacFontDirectory(
    const std::filesystem::path& root,
    GuiMacFontSet& fonts,
    std::string& error
);

void DestroyGuiMacFontSet(
    GuiMacFontSet& fonts
);

void UpdateGuiTextTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const GuiWindowRuntime& windowRuntime,
    const GuiMacFontSet& fonts,
    int width,
    int height,
    const gui::GuiLayoutContext& layoutContext
);

void UpdateGuiTextCommandTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const gui::GuiTextCommand& command,
    const GuiMacFontSet& fonts
);

void UpdateGuiListTextTexture(
    SDL_Texture* texture,
    std::vector<uint8_t>& pixels,
    const GuiWindowRuntime& windowRuntime,
    std::string_view listName,
    const GuiListRuntimeLayout& layout,
    const GuiMacFontSet& fonts,
    const gui::GuiLayoutContext& layoutContext
);
