#!/usr/bin/env python3
from pathlib import Path
import re
import sys

path=Path(sys.argv[1])
s=path.read_text()

# V13.14 is visual only: keep trophy/hijack logic untouched.
# Use a normal packaged asset path for the in-app background. sce_sys/pic0.png
# remains the PS4 shell background, while this copy is reliable at runtime.
s=s.replace('/app0/sce_sys/pic0.png','/app0/assets/images/background.png')

# Remove the old global top/bottom bars and technical title/version chrome.
old='''        fill_alpha({0,0,W,128},20,27,39,220);\n        text("TROPHY UNLOCKER 13.52",80,22,1000,0);\n        text("V13.10 - CAPAS + VINCULO MANUAL DE TROFEUS",82,77,1200);\n        fill_alpha({0,950,W,130},16,22,32,225);\n'''
if old not in s:
    raise SystemExit('V13.14 header anchor not found')
s=s.replace(old,'',1)

# Replace only the rendered library screen. Event handling and all trophy code stay as-is.
pattern=re.compile(r'''        if \(screen == LIBRARY\) \{\n            fill_alpha\(\{70,160,1270,760\},18,24,35,205\);.*?\n        \} else if \(screen == DETAILS\) \{''',re.S)
m=pattern.search(s)
if not m:
    raise SystemExit('V13.14 library render block not found')

new=r'''        if (screen == LIBRARY) {
            // Clean library layout: artwork first, technical details only in diagnostics.
            text("BIBLIOTECA",72,56,520,0);
            char total_line[96];
            snprintf(total_line,sizeof(total_line),"%zu jogos instalados",library.games.size());
            text(total_line,72,108,520,2);

            // Main content glass panel.
            fill_alpha({55,155,1325,785},7,14,30,145);
            border({55,155,1325,785});

            if (library.games.empty()) {
                fill_alpha({265,305,900,360},8,30,61,210);
                border({265,305,900,360});
                text(library.busy() ? "PROCURANDO JOGOS..." : "NENHUM JOGO ENCONTRADO",335,365,760,0);
                text(library.busy() ? "A biblioteca esta sendo atualizada." : "Atualize a biblioteca ou procure um jogo instalado.",335,455,760);
                text("R1  Atualizar biblioteca",335,545,760,2);
                text("QUADRADO  Procurar jogos",335,595,760,2);
            } else {
                int n=int(library.games.size());
                const tu::Game& game=library.games[selected];

                // Previous card.
                if (n > 1) {
                    const tu::Game& prev=library.games[(selected+n-1)%n];
                    fill_alpha({105,260,300,500},8,39,82,205);
                    border({105,260,300,500});
                    fill_alpha({120,275,270,380},11,31,61,230);
                    draw_cover(fs,prev,{135,292,240,340},false,0);
                    text(prev.title.empty() ? prev.id : prev.title,128,675,255,2);
                    text("SELECIONAR",128,720,255,2);
                }

                // Selected card: larger blue-backed tile with a clear action.
                fill_alpha({455,185,525,650},7,55,119,230);
                border({455,185,525,650},true);
                fill_alpha({480,210,475,455},10,27,55,235);
                draw_cover(fs,game,{525,225,385,425},true,1);
                text(game.title.empty() ? game.id : game.title,495,690,445,0);
                text(game.id,497,747,440,2);
                fill({585,785,265,62},28,108,232);
                border({585,785,265,62},true);
                text("X  ABRIR",640,796,190,1);

                // Next card.
                if (n > 2) {
                    const tu::Game& next=library.games[(selected+1)%n];
                    fill_alpha({1030,260,300,500},8,39,82,205);
                    border({1030,260,300,500});
                    fill_alpha({1045,275,270,380},11,31,61,230);
                    draw_cover(fs,next,{1060,292,240,340},false,2);
                    text(next.title.empty() ? next.id : next.title,1053,675,255,2);
                    text("SELECIONAR",1053,720,255,2);
                }
            }

            // Human-readable status panel; raw errno values stay in OPTIONS diagnostics.
            fill_alpha({1420,155,445,785},7,14,30,185);
            border({1420,155,445,785});
            text("SISTEMA",1460,195,360,0);
            text(access.acknowledged() ? "Acesso GoldHEN  -  ativo" : "Acesso GoldHEN  -  verificar",1460,260,350,2);

            auto storage_line = [&](size_t index,const char* label) {
                char line[128];
                if (index >= library.roots.size()) {
                    snprintf(line,sizeof(line),"%s  -  nenhum jogo encontrado",label);
                } else {
                    const tu::RootResult& root=library.roots[index];
                    if (!root.status && root.matches > 0)
                        snprintf(line,sizeof(line),"%s  -  %d jogo%s detectado%s",label,root.matches,root.matches==1?"":"s",root.matches==1?"":"s");
                    else if (!root.status || root.status == -2)
                        snprintf(line,sizeof(line),"%s  -  nenhum jogo encontrado",label);
                    else
                        snprintf(line,sizeof(line),"%s  -  indisponivel",label);
                }
                return std::string(line);
            };

            fill({1455,337,7,50},37,126,238);
            text(storage_line(0,"Interno"),1482,338,335,2);
            fill({1455,420,7,50},50,71,103);
            text(storage_line(1,"Externo 1"),1482,421,335,2);
            fill({1455,503,7,50},50,71,103);
            text(storage_line(2,"Externo 2"),1482,504,335,2);

            text("ACOES",1460,620,350,2);
            text("R1        Atualizar biblioteca",1460,675,350,2);
            text("QUADRADO  Procurar jogos",1460,727,350,2);
            text("OPTIONS   Diagnostico",1460,779,350,2);
            text("TRIANGULO Renovar acesso",1460,831,350,2);

            // No bottom stripe: only a discreet control hint over the artwork.
            text("ESQ/DIR navegar     X abrir",72,987,700,2);
        } else if (screen == DETAILS) {'''

s=s[:m.start()]+new+s[m.end():]

path.write_text(s)
print('[OK] V13.14 clean library redesign applied')
