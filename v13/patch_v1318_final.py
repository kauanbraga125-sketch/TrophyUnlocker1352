#!/usr/bin/env python3
from pathlib import Path
import sys

path=Path(sys.argv[1])
s=path.read_text()

def rep(old,new):
    global s
    if old not in s:
        raise SystemExit('V13.18 patch anchor not found: '+old[:180])
    s=s.replace(old,new,1)

# Load the last selected CUSA from the app's own /data state file. This is a
# tiny read and does not touch game/trophy data.
rep('''    tu::Library library(fs);''','''    tu::Library library(fs);
    std::string saved_selection;
#ifndef TU_HOST_PREVIEW
    {
        std::vector<uint8_t> state_bytes;
        if (!fs.read("/data/TrophyUnlocker1352/ui-state-v1.txt",64,state_bytes) && !state_bytes.empty()) {
            std::string candidate(reinterpret_cast<const char*>(state_bytes.data()),state_bytes.size());
            size_t end=candidate.find_first_of("\\r\\n\\t ");
            if (end != std::string::npos) candidate.resize(end);
            if (tu::is_cusa(candidate)) saved_selection=candidate;
        }
    }
#endif''')

# Selection persistence state. Writes are debounced so scrolling stays as fast
# as V13.17; state is saved only after the user has stopped moving.
rep('''    bool full_redraw=true, nav_redraw=false;
    uint32_t last_input_ms=0, next_cover_warm_ms=0;
    int cover_warm_index=0;''','''    bool full_redraw=true, nav_redraw=false;
    uint32_t last_input_ms=0, next_cover_warm_ms=0;
    int cover_warm_index=0;
    bool selection_restored=saved_selection.empty(), selection_dirty=false;
    uint32_t selection_save_at_ms=0;''')

# Preserve the current CUSA when the user manually refreshes the library.
rep('''    auto start_scan = [&]() {
        manual_load=library.load_manual(); library.begin_scan();
        selected=0; scan_finished=false; message="Buscando jogos... voce pode usar a busca manual.";''','''    auto start_scan = [&]() {
        if (!library.games.empty() && selected >= 0 && selected < int(library.games.size()))
            saved_selection=library.games[selected].id;
        manual_load=library.load_manual(); library.begin_scan();
        selected=0; selection_restored=saved_selection.empty();
        scan_finished=false; message="Atualizando biblioteca...";''')

# Remember navigation, but never write to disk on the input path itself.
rep('''                if (key == 15 && n) selected=(selected+n-1)%n;
                else if (key == 16 && n) selected=(selected+1)%n;''','''                if (key == 15 && n) {
                    selected=(selected+n-1)%n;
                    selection_dirty=true; selection_save_at_ms=SDL_GetTicks()+650;
                }
                else if (key == 16 && n) {
                    selected=(selected+1)%n;
                    selection_dirty=true; selection_save_at_ms=SDL_GetTicks()+650;
                }''')

# Restore only after all three installed-app roots have been discovered, because
# that is the point at which Library has done its one-time CUSA sort.
rep('''        if (!library.games.empty()) selected=std::max(0,std::min(selected,int(library.games.size())-1));

#ifndef TU_HOST_PREVIEW''','''        if (!library.games.empty()) selected=std::max(0,std::min(selected,int(library.games.size())-1));
        if (!selection_restored && library.roots.size() >= 3) {
            for (size_t i=0;i<library.games.size();++i) {
                if (library.games[i].id == saved_selection) {
                    selected=int(i);
                    full_redraw=true;
                    nav_redraw=false;
                    break;
                }
            }
            selection_restored=true;
        }

#ifndef TU_HOST_PREVIEW
        if (selection_dirty && !library.games.empty()) {
            uint32_t now=SDL_GetTicks();
            if (int32_t(now-selection_save_at_ms) >= 0) {
                const std::string current=library.games[selected].id;
                if (!fs.save("/data/TrophyUnlocker1352/ui-state-v1.txt",current+"\\n"))
                    saved_selection=current;
                selection_dirty=false;
            }
        }
''')

# A quiet, human-readable library status instead of development-looking text.
rep('''            char total_line[96];
            snprintf(total_line,sizeof(total_line),"%zu jogos instalados",library.games.size());
            text(total_line,72,108,520,2);''','''            char total_line[128];
            if (library.busy())
                snprintf(total_line,sizeof(total_line),"%zu jogos  |  atualizando biblioteca...",library.games.size());
            else
                snprintf(total_line,sizeof(total_line),"%zu jogos instalados",library.games.size());
            text(total_line,72,108,620,2);''')

# Keep diagnostics available through OPTIONS for support, but remove the
# development/debug wording from the normal home screen.
s=s.replace('            text("OPTIONS   Diagnostico",1460,779,350,2);\n','',1)
s=s.replace('            text("TRIANGULO Renovar acesso",1460,831,350,2);','            text("TRIANGULO Renovar acesso",1460,789,350,2);',1)

path.write_text(s)
print('[OK] V13.18 final stable polish applied')
