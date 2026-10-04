#!/usr/bin/env python3
from pathlib import Path
import sys

path=Path(sys.argv[1])
s=path.read_text()

def rep(old,new):
    global s
    if old not in s:
        raise SystemExit("V13.22 patch anchor not found: "+old[:220])
    s=s.replace(old,new,1)

# Native pad API is used as the authoritative L2 source on PS4.
rep('''#include <orbis/Sysmodule.h>
#include <proto-include.h>
#endif''','''#include <orbis/Sysmodule.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <proto-include.h>
#endif''')

# Do not depend on a joystick-button index for L2. In SDL, triggers are normally axes.
# Keep a host-preview keyboard shortcut and add an SDL axis fallback.
rep('''static int button(const SDL_Event& e) {
    if (e.type == SDL_JOYBUTTONDOWN) return e.jbutton.button;
    if (e.type != SDL_KEYDOWN || e.key.repeat) return -1;''','''static int button(const SDL_Event& e) {
    static bool l2_axis_down=false;
    if (e.type == SDL_JOYAXISMOTION && e.jaxis.axis == 4) {
        bool down=e.jaxis.value > 12000;
        if (down && !l2_axis_down) { l2_axis_down=true; return 17; }
        if (e.jaxis.value < 6000) l2_axis_down=false;
        return -1;
    }
    if (e.type == SDL_JOYBUTTONDOWN) return e.jbutton.button;
    if (e.type != SDL_KEYDOWN || e.key.repeat) return -1;''')

rep('''        case SDLK_l: return 6;''','''        case SDLK_l: return 17;''')
rep('''                else if (key == 6 && n) {''','''                else if (key == 17 && n) {''')

# Open a native ScePad handle. This avoids ambiguity in SDL trigger mappings.
rep('''#endif
    tu::FileSystem& fs=tu::native_filesystem();''','''#endif
#ifndef TU_HOST_PREVIEW
    int native_pad=-1;
    int32_t native_user=0;
    scePadInit();
    if (sceUserServiceGetInitialUser(&native_user) >= 0)
        native_pad=scePadOpen(native_user,0,0,nullptr);
    bool native_l2_down=false;
#endif
    tu::FileSystem& fs=tu::native_filesystem();''')

# Convert a native L2 rising edge into the same internal fake-unlock action.
# A press must be released before it can count again, so "L2 twice" is genuinely two presses.
rep('''    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {''','''    for (;;) {
#ifndef TU_HOST_PREVIEW
        if (native_pad >= 0) {
            OrbisPadData pad_state = {};
            if (scePadReadState(native_pad,&pad_state) >= 0) {
                bool l2_now=(pad_state.buttons & ORBIS_PAD_BUTTON_L2) != 0;
                if (l2_now && !native_l2_down) {
                    SDL_Event l2_event = {};
                    l2_event.type=SDL_KEYDOWN;
                    l2_event.key.repeat=0;
                    l2_event.key.keysym.sym=SDLK_l;
                    SDL_PushEvent(&l2_event);
                }
                native_l2_down=l2_now;
            }
        }
#endif
        SDL_Event event;
        while (SDL_PollEvent(&event)) {''')

# Footer clarifies that L2 is detected natively.
rep('''            text("X real   L2 fake local (2x)   QUADRADO reverter (2x)   TRIANGULO diagnostico   O voltar",80,977,1780,2);''',
'''            text("X real   L2 fake local (2x)   QUADRADO reverter (2x)   TRIANGULO diagnostico   O voltar",80,977,1780,2);''')

path.write_text(s)
print("[OK] V13.22 native L2 detection applied")
