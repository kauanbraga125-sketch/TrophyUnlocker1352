#!/usr/bin/env python3
from pathlib import Path
import sys

path=Path(sys.argv[1])
s=path.read_text()

def rep(old,new):
    global s
    if old not in s:
        raise SystemExit('diag patch anchor not found: '+old[:120])
    s=s.replace(old,new,1)

rep('#include "audio.h"\n','#include "audio.h"\n#include "npbind_diag.h"\n')
rep('enum Screen { LIBRARY, DETAILS, TROPHIES, TROPHY_LINK, BROWSER, CUSA_INPUT, DIAGNOSTICS };',
    'enum Screen { LIBRARY, DETAILS, TROPHIES, TROPHY_LINK, NP_DIAGNOSTIC, BROWSER, CUSA_INPUT, DIAGNOSTICS };')
rep('    tu::TrophyCatalogResult catalog_result;\n    int trophy_selected=0, catalog_selected=0;\n',
    '    tu::TrophyCatalogResult catalog_result;\n    tu::NpbindDiagnostic np_diag;\n    int trophy_selected=0, catalog_selected=0;\n')

rep('''                else if (key == 1) screen=DETAILS;
                else if (key == 0 && n && !library.games.empty()) {''',
'''                else if (key == 1) screen=DETAILS;
                else if (key == 3 && !library.games.empty()) {
                    np_diag=tu::inspect_npbind_for_game(library.games[selected],fs);
                    screen=NP_DIAGNOSTIC;
                    message=np_diag.status ? "Diagnostico npbind terminou com erro." : "Diagnostico npbind concluido em modo somente leitura.";
                }
                else if (key == 0 && n && !library.games.empty()) {''')

rep('''            } else if (screen == BROWSER) {
                int n=int(browser.entries.size());''',
'''            } else if (screen == NP_DIAGNOSTIC) {
                if (key == 1) screen=TROPHIES;
                else if (key == 3 && !library.games.empty()) {
                    np_diag=tu::inspect_npbind_for_game(library.games[selected],fs);
                    message=np_diag.status ? "Leitura npbind repetida com erro." : "Leitura npbind atualizada.";
                }
            } else if (screen == BROWSER) {
                int n=int(browser.entries.size());''')

rep('            text("X desbloquear   CIMA/BAIXO escolher   O voltar",80,977,1780,2);',
    '            text("X desbloquear   TRIANGULO diagnostico npbind   CIMA/BAIXO escolher   O voltar",80,977,1780,2);')

rep('''            text("X vincular   CIMA/BAIXO escolher   ESQ/DIR pagina   O voltar",80,977,1780,2);
        } else if (screen == BROWSER) {''',
'''            text("X vincular   CIMA/BAIXO escolher   ESQ/DIR pagina   O voltar",80,977,1780,2);
        } else if (screen == NP_DIAGNOSTIC) {
            fill({70,160,1780,750},18,24,35); border({70,160,1780,750});
            text("DIAGNOSTICO NPBIND - SOMENTE LEITURA",105,187,1640,0);
            text("Mostra NPWR, cabecalho, entradas e NP Service Label sem alterar arquivos.",105,245,1640,2);
            int y=300;
            for(size_t i=0;i<np_diag.lines.size() && i<15;++i,y+=40)
                text(np_diag.lines[i],110,y,1640,2);
            text("TRIANGULO reler   O voltar aos trofeus",80,977,1780,2);
        } else if (screen == BROWSER) {''')

s=s.replace('Trophy Unlocker V13.10','Trophy Unlocker V13.12 DIAG')
path.write_text(s)
print('[OK] V13.12 read-only npbind diagnostics applied')
