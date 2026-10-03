#!/usr/bin/env python3
from pathlib import Path
import re
import sys

path=Path(sys.argv[1])
s=path.read_text()

def rep(old,new):
    global s
    if old not in s:
        raise SystemExit('visual patch anchor not found: '+old[:140])
    s=s.replace(old,new,1)

rep('#include "npbind_diag.h"\n', '#include "npbind_diag.h"\n#include "image_decode.h"\n')

rep('''        if (!game.icon.empty() && !fs.read(game.icon,12*1024*1024,bytes) && tu::valid_png(bytes)) {
            SDL_RWops* rw=SDL_RWFromConstMem(bytes.data(),int(bytes.size()));
            SDL_Surface* image=rw ? IMG_Load_RW(rw,1) : nullptr;
            if (image) { cover.texture=SDL_CreateTextureFromSurface(renderer,image); SDL_FreeSurface(image); }
        }''', '''        if (!game.icon.empty() && !fs.read(game.icon,12*1024*1024,bytes) && tu::valid_png(bytes)) {
            SDL_Surface* image=tu::decode_png_rgba(bytes);
            if (image) { cover.texture=SDL_CreateTextureFromSurface(renderer,image); SDL_FreeSurface(image); }
        }''')

rep('''static void fill(SDL_Rect rect,uint8_t r,uint8_t g,uint8_t b) {
    SDL_SetRenderDrawColor(renderer,r,g,b,255); SDL_RenderFillRect(renderer,&rect);
}''', '''static void fill(SDL_Rect rect,uint8_t r,uint8_t g,uint8_t b) {
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer,r,g,b,255); SDL_RenderFillRect(renderer,&rect);
}
static void fill_alpha(SDL_Rect rect,uint8_t r,uint8_t g,uint8_t b,uint8_t a) {
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,r,g,b,a); SDL_RenderFillRect(renderer,&rect);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
}''')

rep('''    renderer=surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!renderer) for (;;) SDL_Delay(1000);
    IMG_Init(IMG_INIT_PNG);''', '''    renderer=surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!renderer) for (;;) SDL_Delay(1000);
    SDL_Texture* background_texture=nullptr;
    IMG_Init(IMG_INIT_PNG);''')

rep('''    tu::FileSystem& fs=tu::native_filesystem();
    tu::Library library(fs);''', '''    tu::FileSystem& fs=tu::native_filesystem();
#ifndef TU_HOST_PREVIEW
    {
        std::vector<uint8_t> background_bytes;
        if (!fs.read("/app0/sce_sys/pic0.png",12*1024*1024,background_bytes) && tu::valid_png(background_bytes)) {
            SDL_Surface* background_surface=tu::decode_png_rgba(background_bytes);
            if (background_surface) {
                background_texture=SDL_CreateTextureFromSurface(renderer,background_surface);
                SDL_FreeSurface(background_surface);
            }
        }
    }
#endif
    tu::Library library(fs);''')

rep('''        SDL_SetRenderDrawColor(renderer,11,16,24,255); SDL_RenderClear(renderer);
        fill({0,0,W,128},20,27,39);''', '''        SDL_SetRenderDrawColor(renderer,11,16,24,255); SDL_RenderClear(renderer);
        if (background_texture) SDL_RenderCopy(renderer,background_texture,nullptr,nullptr);
        fill_alpha({0,0,W,128},20,27,39,220);''')

# Let the cosmic background remain visible behind the UI while preserving contrast.
s=s.replace('fill({0,950,W,130},16,22,32);','fill_alpha({0,950,W,130},16,22,32,225);')
s=re.sub(r'fill\((\{[^;]+?\}),18,24,35\);', r'fill_alpha(\1,18,24,35,205);', s)

s=s.replace('Trophy Unlocker V13.12 DIAG','Trophy Unlocker V13.13 VISUAL')
s=s.replace('V13.12 DIAG','V13.13 - CAPAS + FUNDO')
path.write_text(s)
print('[OK] V13.13 real PNG covers + packaged pic0 background applied')
