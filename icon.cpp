#include "icon.hpp"
#include "app_icon_data.hpp"

#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

bool SetApplicationIcon(SDL_Window* window)
{
    int32_t width = 0;
    int32_t height = 0;
    int32_t channels = 0;
    unsigned char* pixels = stbi_load_from_memory(ApplicationIcon, sizeof(ApplicationIcon), &width, &height, &channels, 4);
    if (!pixels)
    {
        return false;
    }
    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels, width * 4);
    const bool success = surface && SDL_SetWindowIcon(window, surface);
    SDL_DestroySurface(surface);
    stbi_image_free(pixels);
    return success;
}
