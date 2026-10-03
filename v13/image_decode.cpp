#include "image_decode.h"
#include <cstring>

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "stb_image.h"
#pragma clang diagnostic pop

namespace tu {
SDL_Surface* decode_png_rgba(const std::vector<uint8_t>& bytes) {
    if (bytes.empty() || bytes.size() > static_cast<size_t>(0x7fffffff)) return nullptr;
    int width=0, height=0, channels=0;
    stbi_uc* pixels=stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 4);
    if (!pixels || width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        if (pixels) stbi_image_free(pixels);
        return nullptr;
    }

    SDL_Surface* surface=SDL_CreateRGBSurface(
        0, width, height, 32,
        0x000000ffu, 0x0000ff00u, 0x00ff0000u, 0xff000000u);
    if (!surface) {
        stbi_image_free(pixels);
        return nullptr;
    }

    if (SDL_LockSurface(surface) < 0) {
        SDL_FreeSurface(surface);
        stbi_image_free(pixels);
        return nullptr;
    }
    const size_t row_bytes=static_cast<size_t>(width)*4;
    for (int y=0; y<height; ++y) {
        std::memcpy(static_cast<uint8_t*>(surface->pixels)+static_cast<size_t>(y)*surface->pitch,
                    pixels+static_cast<size_t>(y)*row_bytes, row_bytes);
    }
    SDL_UnlockSurface(surface);
    stbi_image_free(pixels);
    return surface;
}
}
