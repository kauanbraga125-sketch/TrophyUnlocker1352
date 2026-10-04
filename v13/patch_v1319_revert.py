#!/usr/bin/env python3
from pathlib import Path
import sys

path=Path(sys.argv[1])
s=path.read_text()

def rep(old,new):
    global s
    if old not in s:
        raise SystemExit("V13.19 patch anchor not found: "+old[:180])
    s=s.replace(old,new,1)

# Add a two-press safety latch for local visual-state reversal.
rep('''    int trophy_selected=0, catalog_selected=0;
''','''    int trophy_selected=0, catalog_selected=0;
    int revert_armed_trophy=-1;
    uint32_t revert_armed_until=0;
''')

# SQUARE first arms the selected unlocked trophy; SQUARE again confirms.
rep('''                else if (key == 1) screen=DETAILS;
                else if (key == 3 && !library.games.empty()) {
''','''                else if (key == 1) { screen=DETAILS; revert_armed_trophy=-1; }
                else if (key == 2 && n) {
                    tu::Trophy& tr=trophy_result.set.trophies[trophy_selected];
                    if (!tr.unlocked) {
                        message="Esse trofeu ja esta BLOQUEADO.";
                        revert_armed_trophy=-1;
                    } else {
                        uint32_t now=SDL_GetTicks();
                        if (revert_armed_trophy != tr.id || int32_t(revert_armed_until-now) <= 0) {
                            revert_armed_trophy=tr.id;
                            revert_armed_until=now+4500;
                            message="Confirmacao: pressione QUADRADO novamente para reverter SOMENTE o estado local deste trofeu.";
                        } else {
                            message="Criando backup e revertendo a marcacao visual local...";
                            tu::TrophyVisualRevertResult rr=tu::revert_visual_trophy(trophy_result.set.database_id,tr.id);
                            message=rr.detail;
                            revert_armed_trophy=-1;
                            tu::TrophyLoadResult refreshed=load_selected_trophies();
                            if (!refreshed.status) {
                                trophy_result=refreshed;
                                if (trophy_selected >= int(trophy_result.set.trophies.size())) trophy_selected=0;
                            }
                        }
                    }
                }
                else if (key == 3 && !library.games.empty()) {
''')

# Moving selection cancels an armed revert so it cannot hit a different trophy.
rep('''                if (key == 13 && n) trophy_selected=(trophy_selected+n-1)%n;
                else if (key == 14 && n) trophy_selected=(trophy_selected+1)%n;
''','''                if (key == 13 && n) { trophy_selected=(trophy_selected+n-1)%n; revert_armed_trophy=-1; }
                else if (key == 14 && n) { trophy_selected=(trophy_selected+1)%n; revert_armed_trophy=-1; }
''')

# Explain the new control without implying it can distinguish fake from genuine unlocks.
old='''            text("X desbloquear   TRIANGULO diagnostico npbind   CIMA/BAIXO escolher   O voltar",80,977,1780,2);'''
new='''            text("X desbloquear real   QUADRADO reverter estado local (2x)   TRIANGULO diagnostico   O voltar",80,977,1780,2);'''
rep(old,new)

path.write_text(s)
print("[OK] V13.19 visual-state revert UI applied")
