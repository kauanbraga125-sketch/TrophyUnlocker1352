#!/usr/bin/env python3
from pathlib import Path
import sys

path=Path(sys.argv[1])
s=path.read_text()

def rep(old,new):
    global s
    if old not in s:
        raise SystemExit('V13.15 patch anchor not found: '+old[:140])
    s=s.replace(old,new,1)

# Dedicated display face for action buttons. Keep all existing text faces unchanged.
rep('static FT_Face faces[3];','static FT_Face faces[4];')

rep('''#ifndef TU_HOST_PREVIEW
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) < 0) for (;;) SDL_Delay(1000);
    const char* fontpath="/app0/assets/fonts/VeraMono.ttf";
#else
    const char* fontpath=std::getenv("TU_PREVIEW_FONT");
    if (!fontpath) return 2;
#endif
    if (FT_Init_FreeType(&ft)) for (;;) SDL_Delay(1000);
    for (int i=0; i<3; ++i) {
        if (FT_New_Face(ft,fontpath,0,&faces[i])) for (;;) SDL_Delay(1000);
        FT_Set_Pixel_Sizes(faces[i],0,i == 0 ? 40 : i == 1 ? 27 : 21);
    }''','''#ifndef TU_HOST_PREVIEW
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) < 0) for (;;) SDL_Delay(1000);
    const char* fontpath="/app0/assets/fonts/VeraMono.ttf";
    const char* action_fontpath="/app0/assets/fonts/Bebas-Regular.ttf";
#else
    const char* fontpath=std::getenv("TU_PREVIEW_FONT");
    if (!fontpath) return 2;
    const char* action_fontpath=std::getenv("TU_PREVIEW_ACTION_FONT");
    if (!action_fontpath) action_fontpath=fontpath;
#endif
    if (FT_Init_FreeType(&ft)) for (;;) SDL_Delay(1000);
    for (int i=0; i<3; ++i) {
        if (FT_New_Face(ft,fontpath,0,&faces[i])) for (;;) SDL_Delay(1000);
        FT_Set_Pixel_Sizes(faces[i],0,i == 0 ? 40 : i == 1 ? 27 : 21);
    }
    if (FT_New_Face(ft,action_fontpath,0,&faces[3])) for (;;) SDL_Delay(1000);
    FT_Set_Pixel_Sizes(faces[3],0,38);''')

# Replace the three-slot cover loader with an LRU texture cache. Previously every
# horizontal move changed all three slots, forcing up to 3 PNG reads/decodes.
# Now visible covers are reused by path, and +/-2 neighbors are warmed ahead.
old_cover='''struct Cover { std::string path; SDL_Texture* texture=nullptr; };
static Cover covers[3];
static void draw_cover(tu::FileSystem& fs,const tu::Game& game,SDL_Rect rect,bool focused,int slot) {
    Cover& cover=covers[slot];
    if (cover.path != game.icon) {
        if (cover.texture) SDL_DestroyTexture(cover.texture);
        cover.texture=nullptr; cover.path=game.icon;
        std::vector<uint8_t> bytes;
        if (!game.icon.empty() && !fs.read(game.icon,12*1024*1024,bytes) && tu::valid_png(bytes)) {
            SDL_Surface* image=tu::decode_png_rgba(bytes);
            if (image) { cover.texture=SDL_CreateTextureFromSurface(renderer,image); SDL_FreeSurface(image); }
        }
    }
    placeholder(rect);
    if (cover.texture) {
        int w,h; SDL_QueryTexture(cover.texture,nullptr,nullptr,&w,&h);
        if (w > 0 && h > 0) {
            int rw=rect.w, rh=h*rw/w;
            if (rh > rect.h) { rh=rect.h; rw=w*rh/h; }
            SDL_Rect target={rect.x+(rect.w-rw)/2,rect.y+(rect.h-rh)/2,rw,rh};
            SDL_RenderCopy(renderer,cover.texture,nullptr,&target);
        }
    }
    border(rect,focused);
}'''
new_cover='''struct CoverCacheEntry {
    std::string path;
    SDL_Texture* texture=nullptr;
    uint64_t stamp=0;
};
static std::vector<CoverCacheEntry> cover_cache;
static uint64_t cover_stamp=0;
static const size_t COVER_CACHE_LIMIT=18;

static void clear_cover_cache() {
    for (CoverCacheEntry& e : cover_cache) if (e.texture) SDL_DestroyTexture(e.texture);
    cover_cache.clear();
    cover_stamp=0;
}

static SDL_Texture* cached_cover_texture(tu::FileSystem& fs,const std::string& path) {
    if (path.empty()) return nullptr;
    ++cover_stamp;
    for (CoverCacheEntry& e : cover_cache) {
        if (e.path == path) { e.stamp=cover_stamp; return e.texture; }
    }

    std::vector<uint8_t> bytes;
    if (fs.read(path,12*1024*1024,bytes) || !tu::valid_png(bytes)) return nullptr;
    SDL_Surface* image=tu::decode_png_rgba(bytes);
    if (!image) return nullptr;
    SDL_Texture* texture=SDL_CreateTextureFromSurface(renderer,image);
    SDL_FreeSurface(image);
    if (!texture) return nullptr;

    if (cover_cache.size() >= COVER_CACHE_LIMIT) {
        size_t oldest=0;
        for (size_t i=1;i<cover_cache.size();++i)
            if (cover_cache[i].stamp < cover_cache[oldest].stamp) oldest=i;
        if (cover_cache[oldest].texture) SDL_DestroyTexture(cover_cache[oldest].texture);
        cover_cache.erase(cover_cache.begin()+oldest);
    }
    cover_cache.push_back({path,texture,cover_stamp});
    return texture;
}

static void draw_cover(tu::FileSystem& fs,const tu::Game& game,SDL_Rect rect,bool focused,int slot) {
    (void)slot;
    SDL_Texture* texture=cached_cover_texture(fs,game.icon);
    placeholder(rect);
    if (texture) {
        int w,h; SDL_QueryTexture(texture,nullptr,nullptr,&w,&h);
        if (w > 0 && h > 0) {
            int rw=rect.w, rh=h*rw/w;
            if (rh > rect.h) { rh=rect.h; rw=w*rh/h; }
            SDL_Rect target={rect.x+(rect.w-rw)/2,rect.y+(rect.h-rh)/2,rw,rh};
            SDL_RenderCopy(renderer,texture,nullptr,&target);
        }
    }
    border(rect,focused);
}'''
rep(old_cover,new_cover)

rep('''        for (Cover& cover : covers) { if (cover.texture) SDL_DestroyTexture(cover.texture); cover.texture=nullptr; cover.path.clear(); }''','''        clear_cover_cache();''')

# Preload the next off-screen covers. After the first frame, a normal left/right
# move should need zero PNG decoding for the three visible cards.
rep('''                if (n > 2) {
                    const tu::Game& next=library.games[(selected+1)%n];
                    fill_alpha({1030,260,300,500},8,39,82,205);
                    border({1030,260,300,500});
                    fill_alpha({1045,275,270,380},11,31,61,230);
                    draw_cover(fs,next,{1060,292,240,340},false,2);
                    text(next.title.empty() ? next.id : next.title,1053,675,255,2);
                    text("SELECIONAR",1053,720,255,2);
                }
            }''','''                if (n > 2) {
                    const tu::Game& next=library.games[(selected+1)%n];
                    fill_alpha({1030,260,300,500},8,39,82,205);
                    border({1030,260,300,500});
                    fill_alpha({1045,275,270,380},11,31,61,230);
                    draw_cover(fs,next,{1060,292,240,340},false,2);
                    text(next.title.empty() ? next.id : next.title,1053,675,255,2);
                    text("SELECIONAR",1053,720,255,2);
                }
                if (n > 3) {
                    static int warm_side=0;
                    int offset=(warm_side++ & 1) ? 2 : -2;
                    int index=(selected+n+offset)%n;
                    cached_cover_texture(fs,library.games[index].icon);
                }
            }''')

# Cleaner action typography using the display face.
rep('text("X  ABRIR",640,796,190,1);','text("X  ABRIR",626,792,215,3);')

path.write_text(s)
print('[OK] V13.15 cover cache + prewarm + action font applied')
