#!/usr/bin/env python3
from pathlib import Path
import sys

path = Path(sys.argv[1])
s = path.read_text()

def rep(old, new):
    global s
    if old not in s:
        raise SystemExit('patch anchor not found: ' + old[:80])
    s = s.replace(old, new, 1)

rep('#include "library.h"\n', '#include "library.h"\n#include "trophies.h"\n#include "trophy_mount.h"\n')
rep('enum Screen { LIBRARY, DETAILS, BROWSER, CUSA_INPUT, DIAGNOSTICS };',
    'enum Screen { LIBRARY, DETAILS, TROPHIES, BROWSER, CUSA_INPUT, DIAGNOSTICS };')
rep('int manual_load=-2, diagnostic_save=-2;\n',
    'int manual_load=-2, diagnostic_save=-2;\n    tu::TrophyLoadResult trophy_result;\n    tu::TrophyMountProbe trophy_probe;\n    int trophy_selected=0;\n')

rep('''            } else if (screen == DETAILS) {
                if (key == 1) screen=LIBRARY;
                else if (key == 2) { browser.locations(); screen=BROWSER; }
            } else if (screen == BROWSER) {''', '''            } else if (screen == DETAILS) {
                if (key == 1) screen=LIBRARY;
                else if (key == 2) { browser.locations(); screen=BROWSER; }
                else if (key == 0 && !library.games.empty()) {
                    message="Lendo os trofeus locais deste jogo...";
                    trophy_result=tu::load_trophies_for_game(library.games[selected],fs);
                    trophy_probe=tu::TrophyMountProbe();
                    trophy_selected=0;
                    if (!trophy_result.status) {
                        screen=TROPHIES;
                        char line[128];
                        snprintf(line,sizeof(line),"%zu trofeu(s) encontrados em %s",trophy_result.set.trophies.size(),trophy_result.set.np_communication_id.c_str());
                        message=line;
                    } else {
                        message="Trofeus nao encontrados: "+trophy_result.detail;
                    }
                }
            } else if (screen == TROPHIES) {
                int n=int(trophy_result.set.trophies.size());
                if (key == 13 && n) trophy_selected=(trophy_selected+n-1)%n;
                else if (key == 14 && n) trophy_selected=(trophy_selected+1)%n;
                else if (key == 1) screen=DETAILS;
                else if (key == 2 && !trophy_result.set.np_communication_id.empty()) {
                    message="Testando credenciais, dispositivos e montagem protegida...";
                    trophy_probe=tu::probe_trophy_container(trophy_result.user_id,trophy_result.set.np_communication_id);
                    if (!trophy_probe.status)
                        message="Container de trofeus OK: montagem protegida validada sem alterar trofeus.";
                    else
                        message="Teste interrompido em "+trophy_probe.stage+": "+trophy_probe.detail;
                }
            } else if (screen == BROWSER) {''')

rep('''                text("Esta etapa identifica jogos e seus arquivos.",645,701,1100,2);
                text("O desbloqueio de trofeus ainda nao esta integrado a esta UI.",645,748,1100,2);
            }
            text("O voltar   QUADRADO procurar pasta do jogo",80,977,1780,2);
        } else if (screen == BROWSER) {''', '''                text("Jogo identificado. Agora podemos consultar o banco local de trofeus.",645,701,1100,2);
                text("X  Abrir lista de trofeus",645,748,1100,2);
            }
            text("X trofeus   O voltar   QUADRADO procurar pasta do jogo",80,977,1780,2);
        } else if (screen == TROPHIES) {
            fill({70,160,1120,750},18,24,35); border({70,160,1120,750});
            fill({1225,160,625,750},18,24,35); border({1225,160,625,750});
            text(trophy_result.set.title.empty() ? "TROFEUS" : trophy_result.set.title,105,187,1040,0);
            text(trophy_result.set.np_communication_id,105,247,1040,2);
            int n=int(trophy_result.set.trophies.size());
            int begin=n ? (trophy_selected/9)*9 : 0;
            for (int row=0; row<9 && begin+row<n; ++row) {
                int index=begin+row, y=310+row*61;
                const tu::Trophy& tr=trophy_result.set.trophies[index];
                if (index == trophy_selected) fill({95,y-3,1070,56},36,61,84);
                char prefix[96];
                snprintf(prefix,sizeof(prefix),"%s  #%02d  %-8s  ",tr.unlocked ? "[OK]" : "[  ]",tr.id,tu::trophy_grade_name(tr.grade));
                text(std::string(prefix)+tr.title,115,y,1020,2);
            }
            if (n) {
                const tu::Trophy& tr=trophy_result.set.trophies[trophy_selected];
                text("TROFEU SELECIONADO",1260,190,540);
                text(tr.title,1260,270,540,0);
                char info[160];
                snprintf(info,sizeof(info),"ID %d   Grupo %d   %s",tr.id,tr.group,tu::trophy_grade_name(tr.grade));
                text(info,1260,355,540,2);
                text(tr.unlocked ? "Estado: desbloqueado" : "Estado: bloqueado",1260,410,540);
                text("Descricao:",1260,493,540,2);
                text(tr.description.empty() ? "Sem descricao" : tr.description,1260,540,540,2);
            }
            text("QUADRADO = testar trophy.img (sem desbloquear)",1260,680,540,2);
            if (!trophy_probe.stage.empty()) {
                text("Ultimo teste: "+trophy_probe.stage,1260,735,540,2);
                char probe_line[160];
                snprintf(probe_line,sizeof(probe_line),"status=%d  mount=%d  decrypt=%d",trophy_probe.status,trophy_probe.mount_status,trophy_probe.decrypt_status);
                text(probe_line,1260,780,540,2);
                text(trophy_probe.detail,1260,825,540,2);
            } else {
                text("Ainda nao testado.",1260,750,540,2);
            }
            text("CIMA/BAIXO escolher   QUADRADO testar container   O voltar",80,977,1780,2);
        } else if (screen == BROWSER) {''')

rep('''            if (requested && !strcmp(requested,"details")) screen=DETAILS;
            if (requested && !strcmp(requested,"diagnostics")) screen=DIAGNOSTICS;''', '''            if (requested && !strcmp(requested,"details")) screen=DETAILS;
            if (requested && !strcmp(requested,"trophies")) screen=TROPHIES;
            if (requested && !strcmp(requested,"diagnostics")) screen=DIAGNOSTICS;''')

s = s.replace('Trophy Unlocker V13.1', 'Trophy Unlocker V13.6.1')
s = s.replace('V13.1 - CORRECAO DE ACESSO', 'V13.6.1 - MONTAGEM PROTEGIDA SEGURA')
path.write_text(s)
