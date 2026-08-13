#include "gui_texture_loader_macos.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <algorithm>
#include <cctype>

namespace
{

std::string Lower(std::string value)
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

SDL_Texture* LoadBitmap(
    SDL_Renderer* renderer,
    const std::filesystem::path& path,
    int* width,
    int* height
)
{
    SDL_Surface* surface = SDL_LoadBMP(path.string().c_str());
    if (!surface)
    {
        return nullptr;
    }
    if (width)
    {
        *width = surface->w;
    }
    if (height)
    {
        *height = surface->h;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(
        renderer,
        surface
    );
    SDL_FreeSurface(surface);
    return texture;
}

SDL_Texture* LoadImageIoTexture(
    SDL_Renderer* renderer,
    const std::filesystem::path& path,
    int* outputWidth,
    int* outputHeight
)
{
    const std::string pathString = path.string();
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(pathString.c_str()),
        pathString.size(),
        false
    );
    if (!url)
    {
        return nullptr;
    }

    CGImageSourceRef imageSource =
        CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (!imageSource)
    {
        return nullptr;
    }

    CGImageRef image = CGImageSourceCreateImageAtIndex(
        imageSource,
        0,
        nullptr
    );
    CFRelease(imageSource);
    if (!image)
    {
        return nullptr;
    }

    const size_t width = CGImageGetWidth(image);
    const size_t height = CGImageGetHeight(image);
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0,
        static_cast<int>(width),
        static_cast<int>(height),
        32,
        SDL_PIXELFORMAT_RGBA32
    );
    if (!surface)
    {
        CGImageRelease(image);
        return nullptr;
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(
        surface->pixels,
        width,
        height,
        8,
        surface->pitch,
        colorSpace,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(colorSpace);
    if (!context)
    {
        SDL_FreeSurface(surface);
        CGImageRelease(image);
        return nullptr;
    }

    CGContextDrawImage(
        context,
        CGRectMake(
            0,
            0,
            static_cast<CGFloat>(width),
            static_cast<CGFloat>(height)
        ),
        image
    );
    CGContextRelease(context);
    CGImageRelease(image);

    if (outputWidth)
    {
        *outputWidth = static_cast<int>(width);
    }
    if (outputHeight)
    {
        *outputHeight = static_cast<int>(height);
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(
        renderer,
        surface
    );
    SDL_FreeSurface(surface);
    return texture;
}

}

SDL_Texture* LoadGuiMacTexture(
    SDL_Renderer* renderer,
    const std::filesystem::path& path,
    int* width,
    int* height
)
{
    if (!renderer || path.empty())
    {
        return nullptr;
    }

    SDL_Texture* texture = Lower(path.extension().string()) == ".bmp"
        ? LoadBitmap(renderer, path, width, height)
        : LoadImageIoTexture(renderer, path, width, height);
    if (texture)
    {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    return texture;
}
