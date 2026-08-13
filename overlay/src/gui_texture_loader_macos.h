#pragma once

#include <SDL.h>

#include <filesystem>

SDL_Texture* LoadGuiMacTexture(
    SDL_Renderer* renderer,
    const std::filesystem::path& path,
    int* width = nullptr,
    int* height = nullptr
);
