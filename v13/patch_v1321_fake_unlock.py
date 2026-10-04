#!/usr/bin/env python3
from pathlib import Path
import sys

path=Path(sys.argv[1])
s=path.read_text()

def rep(old,new):
    global s
    if old not in s:
        raise SystemExit("V13.21 patch anchor not found: "+old[:180])
    s=s.replace(old,new,1)

# Keyboard preview fallback for L2. On PS4 SDL, joystick button 6 is L2.
rep('''        case SDLK_e: return 5;
        case SDLK_d: return 9;''','''        case SDLK_e: return 5;
        case SDLK_l: return 6;
        case SDLK_d: return 9;''')

# Separate confirmation latch for the fake/local unlock.
rep('''    int revert_armed_trophy=-1;
    uint32_t revert_armed_until=0;
''','''    int revert_armed_trophy=-1;
    uint32_t revert_armed_until=0;
    int fake_armed_trophy=-1;
    uint32_t fake_armed_until=0;
''')

# Changing selection cancels both confirmation latches.
rep('''                if (key == 13 && n) { trophy_selected=(trophy_selected+n-1)%n; revert_armed_trophy=-1; }
                else if (key == 14 && n) { trophy_selected=(trophy_selected+1)%n; revert_armed_trophy=-1; }
''','''                if (key == 13 && n) { trophy_selected=(trophy_selected+n-1)%n; revert_armed_trophy=-1; fake_armed_trophy=-1; }
                else if (key == 14 && n) { trophy_selected=(trophy_selected+1)%n; revert_armed_trophy=-1; fake_armed_trophy=-1; }
''')

# L2 twice = fake/local unlock. It is intentionally separate from X/native unlock.
rep('''                else if (key == 3 && !library.games.empty()) {
                    np_diag=tu::inspect_npbind_for_game(library.games[selected],fs);''','''                else if (key == 6 && n) {
                    tu::Trophy& tr=trophy_result.set.trophies[trophy_selected];
                    if (tr.unlocked) {
                        message="Esse trofeu ja aparece como DESBLOQUEADO.";
                        fake_armed_trophy=-1;
                    } else {
                        uint32_t now=SDL_GetTicks();
                        if (fake_armed_trophy != tr.id || int32_t(fake_armed_until-now) <= 0) {
                            fake_armed_trophy=tr.id;
                            fake_armed_until=now+4500;
                            message="FAKE UNLOCK: pressione L2 novamente para confirmar a marcacao local.";
                        } else {
                            message="Criando backup e aplicando FAKE UNLOCK local...";
                            tu::TrophyFakeUnlockResult fr=tu::fake_unlock_trophy(trophy_result.set.database_id,tr.id);
                            message=fr.detail;
                            fake_armed_trophy=-1;
                            tu::TrophyLoadResult refreshed=load_selected_trophies();
                            if (!refreshed.status) {
                                trophy_result=refreshed;
                                if (trophy_selected >= int(trophy_result.set.trophies.size())) trophy_selected=0;
                            }
                        }
                    }
                }
                else if (key == 3 && !library.games.empty()) {
                    np_diag=tu::inspect_npbind_for_game(library.games[selected],fs);''')

# Leaving the trophy page cancels both armed actions.
rep('''                else if (key == 1) { screen=DETAILS; revert_armed_trophy=-1; }
''','''                else if (key == 1) { screen=DETAILS; revert_armed_trophy=-1; fake_armed_trophy=-1; }
''')

# Footer documents the three distinct actions.
rep('''            text("X desbloquear real   QUADRADO reverter estado local (2x)   TRIANGULO diagnostico   O voltar",80,977,1780,2);''',
'''            text("X real   L2 fake local (2x)   QUADRADO reverter (2x)   TRIANGULO diagnostico   O voltar",80,977,1780,2);''')

path.write_text(s)
print("[OK] V13.21 L2 fake unlock UI applied")
