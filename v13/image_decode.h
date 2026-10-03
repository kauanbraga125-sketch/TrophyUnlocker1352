#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>

namespace tu {
// Decodes PNG bytes to an owned 32-bit RGBA SDL surface.
// The caller owns the returned surface and must SDL_FreeSurface it.
SDL_Surface* decode_png_rgba(const std::vector<uint8_t>& bytes);
}
