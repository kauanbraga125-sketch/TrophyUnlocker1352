#!/usr/bin/env python3
from pathlib import Path
import sys

path=Path(sys.argv[1])
s=path.read_text()

def rep(old,new):
    global s
    if old not in s:
        raise SystemExit('V13.16 patch anchor not found: '+old[:180])
    s=s.replace(old,new,1)

s=s.replace('fill_alpha({55,155,1325,785},7,14,30,145);','fill({55,155,1325,785},7,14,30);',1)

s=s.replace('''                if (n > 3) {
                    static int warm_side=0;
                    int offset=(warm_side++ & 1) ? 2 : -2;
                    int index=(selected+n+offset)%n;
                    cached_cover_texture(fs,library.games[index].icon);
                }
''','',1)

rep('''    int first_frame=0;
    bool pending_access=false, scan_finished=false;
    int manual_load=-2, diagnostic_save=-2;''','''    int first_frame=0;
    bool pending_access=false, scan_finished=false;
    int manual_load=-2, diagnostic_save=-2;
    bool full_redraw=true, nav_redraw=false;
    uint32_t last_input_ms=0, next_cover_warm_ms=0;
    int cover_warm_index=0;''')

rep('''        clear_cover_cache();''','''        clear_cover_cache();
        cover_warm_index=0;
        next_cover_warm_ms=0;
        full_redraw=true;
        nav_redraw=false;''')

rep('''            int key=button(event);
            if (key < 0) continue;
            if (key == 1) audio.play(tu::SoundId::Back);
            else if (key == 0 && screen != TROPHIES) audio.play(tu::SoundId::Confirm);
            else if (key == 13 || key == 14 || key == 15 || key == 16) audio.play(tu::SoundId::Move);
            if (screen == LIBRARY) {''','''            int key=button(event);
            if (key < 0) continue;
            if (key == 1) audio.play(tu::SoundId::Back);
            else if (key == 0 && screen != TROPHIES) audio.play(tu::SoundId::Confirm);
            else if (key == 13 || key == 14 || key == 15 || key == 16) audio.play(tu::SoundId::Move);
            last_input_ms=SDL_GetTicks();
            bool fast_library_nav=(screen == LIBRARY && (key == 15 || key == 16) && !library.games.empty());
            if (fast_library_nav) nav_redraw=true;
            else { full_redraw=true; nav_redraw=false; }
            if (screen == LIBRARY) {''')

rep('''        if (library.busy() && screen == LIBRARY) library.scan_step();''','''        if (library.busy() && screen == LIBRARY) {
            library.scan_step();
            full_redraw=true;
            nav_redraw=false;
        }''')

rep('''            scan_finished=true;
            message=library.games.empty() ?''','''            scan_finished=true;
            full_redraw=true;
            nav_redraw=false;
            cover_warm_index=0;
            next_cover_warm_ms=SDL_GetTicks()+120;
            message=library.games.empty() ?''')

anchor='''        if (!library.games.empty()) selected=std::max(0,std::min(selected,int(library.games.size())-1));

        SDL_SetRenderDrawColor(renderer,11,16,24,255); SDL_RenderClear(renderer);'''
fast=r'''        if (!library.games.empty()) selected=std::max(0,std::min(selected,int(library.games.size())-1));

#ifndef TU_HOST_PREVIEW
        if (!full_redraw && !nav_redraw && screen == LIBRARY && !library.busy() &&
            cover_warm_index < int(library.games.size())) {
            uint32_t now=SDL_GetTicks();
            if (now-last_input_ms > 180 && now >= next_cover_warm_ms) {
                cached_cover_texture(fs,library.games[cover_warm_index].icon);
                ++cover_warm_index;
                next_cover_warm_ms=SDL_GetTicks()+8;
            }
        }

        if (nav_redraw && !full_redraw && screen == LIBRARY && !library.games.empty()) {
            SDL_Rect dirty={55,155,1325,785};
            fill(dirty,7,14,30);
            border(dirty);

            int n=int(library.games.size());
            const tu::Game& game=library.games[selected];
            if (n > 1) {
                const tu::Game& prev=library.games[(selected+n-1)%n];
                fill_alpha({105,260,300,500},8,39,82,205);
                border({105,260,300,500});
                fill_alpha({120,275,270,380},11,31,61,230);
                draw_cover(fs,prev,{135,292,240,340},false,0);
                text(prev.title.empty() ? prev.id : prev.title,128,675,255,2);
                text("SELECIONAR",128,720,255,2);
            }

            fill_alpha({455,185,525,650},7,55,119,230);
            border({455,185,525,650},true);
            fill_alpha({480,210,475,455},10,27,55,235);
            draw_cover(fs,game,{525,225,385,425},true,1);
            text(game.title.empty() ? game.id : game.title,495,690,445,0);
            text(game.id,497,747,440,2);
            fill({585,785,265,62},28,108,232);
            border({585,785,265,62},true);
            text("X  ABRIR",626,792,215,3);

            if (n > 2) {
                const tu::Game& next=library.games[(selected+1)%n];
                fill_alpha({1030,260,300,500},8,39,82,205);
                border({1030,260,300,500});
                fill_alpha({1045,275,270,380},11,31,61,230);
                draw_cover(fs,next,{1060,292,240,340},false,2);
                text(next.title.empty() ? next.id : next.title,1053,675,255,2);
                text("SELECIONAR",1053,720,255,2);
            }

            SDL_RenderPresent(renderer);
            SDL_UpdateWindowSurfaceRects(win,&dirty,1);
            nav_redraw=false;
            SDL_Delay(1);
            continue;
        }

        if (!full_redraw && !nav_redraw) {
            SDL_Delay(2);
            continue;
        }
#else
        full_redraw=true;
#endif

        SDL_SetRenderDrawColor(renderer,11,16,24,255); SDL_RenderClear(renderer);'''
rep(anchor,fast)

rep('''        SDL_RenderPresent(renderer); SDL_UpdateWindowSurface(win);''','''        SDL_RenderPresent(renderer); SDL_UpdateWindowSurface(win);
        full_redraw=false;
        nav_redraw=false;''')

path.write_text(s)
print('[OK] V13.16 dirty-region/event-driven rendering applied')
