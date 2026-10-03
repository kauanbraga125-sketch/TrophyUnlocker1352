#!/usr/bin/env python3
from pathlib import Path
import sys

path=Path(sys.argv[1])
s=path.read_text()

def rep(old,new):
    global s
    if old not in s:
        raise SystemExit('V13.17 patch anchor not found: '+old[:180])
    s=s.replace(old,new,1)

# Keep the UI interactive while the library continues its metadata enrichment.
# Directory discovery is allowed immediately; once games are visible, expensive
# background enrichment pauses briefly whenever the user is navigating.
rep('''        if (library.busy() && screen == LIBRARY) {
            library.scan_step();
            full_redraw=true;
            nav_redraw=false;
        }''','''        if (library.busy() && screen == LIBRARY) {
            uint32_t now=SDL_GetTicks();
            bool need_first_results=library.games.empty();
            bool user_is_navigating=(now-last_input_ms) < 120;
            if (need_first_results || !user_is_navigating) {
                library.scan_step();
                full_redraw=true;
                nav_redraw=false;
            }
        }''')

# Cover-mode rendering: fill the destination completely and crop only the
# overflow, instead of letterboxing the icon and leaving empty margins.
old='''    if (texture) {
        int w,h; SDL_QueryTexture(texture,nullptr,nullptr,&w,&h);
        if (w > 0 && h > 0) {
            int rw=rect.w, rh=h*rw/w;
            if (rh > rect.h) { rh=rect.h; rw=w*rh/h; }
            SDL_Rect target={rect.x+(rect.w-rw)/2,rect.y+(rect.h-rh)/2,rw,rh};
            SDL_RenderCopy(renderer,texture,nullptr,&target);
        }
    }
    border(rect,focused);'''
new='''    if (texture) {
        int w,h; SDL_QueryTexture(texture,nullptr,nullptr,&w,&h);
        if (w > 0 && h > 0) {
            SDL_Rect source={0,0,w,h};
            const int64_t lhs=int64_t(w)*rect.h;
            const int64_t rhs=int64_t(h)*rect.w;
            if (lhs > rhs) {
                source.w=int(int64_t(h)*rect.w/rect.h);
                source.x=(w-source.w)/2;
            } else if (lhs < rhs) {
                source.h=int(int64_t(w)*rect.h/rect.w);
                source.y=(h-source.h)/2;
            }
            SDL_RenderCopy(renderer,texture,&source,&rect);
        }
    }
    (void)focused;'''
rep(old,new)

# Remove the deliberate padding that V13.14 left around each cover. The image
# now occupies the whole artwork window; the card itself keeps its outer frame.
for old_rect,new_rect in [
    ('draw_cover(fs,prev,{135,292,240,340},false,0);','draw_cover(fs,prev,{120,275,270,380},false,0);'),
    ('draw_cover(fs,game,{525,225,385,425},true,1);','draw_cover(fs,game,{480,210,475,455},true,1);'),
    ('draw_cover(fs,next,{1060,292,240,340},false,2);','draw_cover(fs,next,{1045,275,270,380},false,2);')
]:
    if old_rect not in s:
        raise SystemExit('V13.17 cover layout anchor not found: '+old_rect)
    s=s.replace(old_rect,new_rect)

path.write_text(s)
print('[OK] V13.17 responsive startup + edge-to-edge covers applied')
