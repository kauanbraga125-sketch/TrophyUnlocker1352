#!/usr/bin/env python3
from pathlib import Path
import sys

path=Path(sys.argv[1])
s=path.read_text()

def rep(old,new):
    global s
    if old not in s:
        raise SystemExit('audio patch anchor not found: '+old[:120])
    s=s.replace(old,new,1)

rep('#include "unlock.h"\n','#include "unlock.h"\n#include "audio.h"\n')
rep('if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)) for (;;) SDL_Delay(1000);','if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|SDL_INIT_AUDIO)) for (;;) SDL_Delay(1000);')
rep('SDL_Joystick* joystick=SDL_NumJoysticks() > 0 ? SDL_JoystickOpen(0) : nullptr;\n',r'''SDL_Joystick* joystick=SDL_NumJoysticks() > 0 ? SDL_JoystickOpen(0) : nullptr;
    tu::AudioEngine audio;
#ifndef TU_HOST_PREVIEW
    audio.init("/app0/assets/audio");
#else
    audio.init("assets/audio");
#endif
''')
rep('''            int key=button(event);
            if (key < 0) continue;
            if (screen == LIBRARY) {''',r'''            int key=button(event);
            if (key < 0) continue;
            if (key == 1) audio.play(tu::SoundId::Back);
            else if (key == 0 && screen != TROPHIES) audio.play(tu::SoundId::Confirm);
            else if (key == 13 || key == 14 || key == 15 || key == 16) audio.play(tu::SoundId::Move);
            if (screen == LIBRARY) {''')
rep('''                    if (tr.unlocked) {
                        message="Esse trofeu ja esta desbloqueado.";
                    } else {
                        message="Executando Trophy Hijack e pedindo desbloqueio ao PS4...";
                        tu::UnlockResult ur=tu::unlock_trophy_for_game(library.games[selected],tr.id);
                        message=ur.detail;
                        tu::TrophyLoadResult refreshed=load_selected_trophies();''',r'''                    if (tr.unlocked) {
                        audio.play(tu::SoundId::Confirm);
                        message="Esse trofeu ja esta desbloqueado.";
                    } else {
                        message="Executando Trophy Hijack e pedindo desbloqueio ao PS4...";
                        tu::UnlockResult ur=tu::unlock_trophy_for_game(library.games[selected],tr.id);
                        if (!ur.status && !ur.already_unlocked) audio.play(tu::SoundId::Unlock);
                        else audio.play(tu::SoundId::Confirm);
                        message=ur.detail;
                        tu::TrophyLoadResult refreshed=load_selected_trophies();''')
path.write_text(s)
