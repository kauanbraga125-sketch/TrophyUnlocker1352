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

rep('#include "library.h"\n', '#include "library.h"\n#include "trophies.h"\n#include "unlock.h"\n')

rep('enum Screen { LIBRARY, DETAILS, BROWSER, CUSA_INPUT, DIAGNOSTICS };', r'''static const char* TROPHY_LINKS_PATH="/data/TrophyUnlocker1352/trophy-links.txt";
static std::string trophy_link_for_game(tu::FileSystem& fs,const std::string& cusa) {
    std::vector<uint8_t> bytes;
    if (fs.read(TROPHY_LINKS_PATH,65536,bytes)) return "";
    std::string data(bytes.begin(),bytes.end());
    size_t at=0;
    while (at < data.size()) {
        size_t end=data.find('\n',at); if (end == std::string::npos) end=data.size();
        std::string line=data.substr(at,end-at); if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t tab=line.find('\t');
        if (tab != std::string::npos && line.substr(0,tab) == cusa) {
            std::string np=line.substr(tab+1);
            return tu::valid_np_communication_id(np) ? np : "";
        }
        at=end+1;
    }
    return "";
}
static int save_trophy_link(tu::FileSystem& fs,const std::string& cusa,const std::string& np) {
    if (!tu::is_cusa(cusa) || !tu::valid_np_communication_id(np)) return -22;
    std::vector<uint8_t> bytes;
    std::string data;
    if (!fs.read(TROPHY_LINKS_PATH,65536,bytes)) data.assign(bytes.begin(),bytes.end());
    std::string out; size_t at=0;
    while (at < data.size()) {
        size_t end=data.find('\n',at); if (end == std::string::npos) end=data.size();
        std::string line=data.substr(at,end-at); if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t tab=line.find('\t');
        if (!line.empty() && (tab == std::string::npos || line.substr(0,tab) != cusa)) out += line+"\n";
        at=end+1;
    }
    out += cusa+"\t"+np+"\n";
    return fs.save(TROPHY_LINKS_PATH,out);
}
enum Screen { LIBRARY, DETAILS, TROPHIES, TROPHY_LINK, BROWSER, CUSA_INPUT, DIAGNOSTICS };''')

rep('int manual_load=-2, diagnostic_save=-2;\n', r'''int manual_load=-2, diagnostic_save=-2;
    tu::TrophyLoadResult trophy_result;
    tu::TrophyCatalogResult catalog_result;
    int trophy_selected=0, catalog_selected=0;

    auto load_selected_trophies = [&]() {
        tu::TrophyLoadResult result;
        if (library.games.empty()) return result;
        const tu::Game& game=library.games[selected];
        std::string linked=trophy_link_for_game(fs,game.id);
        if (tu::valid_np_communication_id(linked)) return tu::load_trophies(linked);
        return tu::load_trophies_for_game(game,fs);
    };
''')

rep('''            } else if (screen == DETAILS) {
                if (key == 1) screen=LIBRARY;
                else if (key == 2) { browser.locations(); screen=BROWSER; }
            } else if (screen == BROWSER) {''', r'''            } else if (screen == DETAILS) {
                if (key == 1) screen=LIBRARY;
                else if (key == 0 && !library.games.empty()) {
                    message="Lendo os trofeus deste jogo instalado...";
                    trophy_result=load_selected_trophies();
                    trophy_selected=0;
                    if (!trophy_result.status) {
                        screen=TROPHIES;
                        char line[160];
                        snprintf(line,sizeof(line),"%zu trofeu(s) encontrados em %s",trophy_result.set.trophies.size(),trophy_result.set.np_communication_id.c_str());
                        message=line;
                    } else {
                        catalog_result=tu::load_all_trophy_sets();
                        catalog_selected=0;
                        if (!catalog_result.status && !catalog_result.sets.empty()) {
                            screen=TROPHY_LINK;
                            message="Associacao automatica falhou. Escolha o conjunto correto e aperte X para vincular.";
                        } else {
                            message="Trofeus nao encontrados e catalogo indisponivel: "+trophy_result.detail;
                        }
                    }
                }
            } else if (screen == TROPHIES) {
                int n=int(trophy_result.set.trophies.size());
                if (key == 13 && n) trophy_selected=(trophy_selected+n-1)%n;
                else if (key == 14 && n) trophy_selected=(trophy_selected+1)%n;
                else if (key == 1) screen=DETAILS;
                else if (key == 0 && n && !library.games.empty()) {
                    tu::Trophy& tr=trophy_result.set.trophies[trophy_selected];
                    if (tr.unlocked) {
                        message="Esse trofeu ja esta desbloqueado.";
                    } else {
                        message="Executando Trophy Hijack e pedindo desbloqueio ao PS4...";
                        tu::UnlockResult ur=tu::unlock_trophy_for_game(library.games[selected],tr.id);
                        message=ur.detail;
                        tu::TrophyLoadResult refreshed=load_selected_trophies();
                        if (!refreshed.status) {
                            trophy_result=refreshed;
                            if (trophy_selected >= int(trophy_result.set.trophies.size())) trophy_selected=0;
                        }
                    }
                }
            } else if (screen == TROPHY_LINK) {
                int n=int(catalog_result.sets.size());
                if (key == 13 && n) catalog_selected=(catalog_selected+n-1)%n;
                else if (key == 14 && n) catalog_selected=(catalog_selected+1)%n;
                else if (key == 15 && n) catalog_selected=std::max(0,catalog_selected-9);
                else if (key == 16 && n) catalog_selected=std::min(n-1,catalog_selected+9);
                else if (key == 1) screen=DETAILS;
                else if (key == 0 && n && !library.games.empty()) {
                    const tu::TrophySet& chosen=catalog_result.sets[catalog_selected];
                    int saved=save_trophy_link(fs,library.games[selected].id,chosen.np_communication_id);
                    trophy_result=tu::load_trophies(chosen.np_communication_id);
                    trophy_selected=0;
                    if (!trophy_result.status) {
                        screen=TROPHIES;
                        message=saved ? "Conjunto vinculado nesta sessao; nao foi possivel salvar o vinculo." : "Vinculo CUSA -> NPWR salvo. Abrindo trofeus.";
                    } else {
                        message="O conjunto escolhido nao pode ser carregado: "+trophy_result.detail;
                    }
                }
            } else if (screen == BROWSER) {''')

rep('''                text("Esta etapa identifica jogos e seus arquivos.",645,701,1100,2);
                text("O desbloqueio de trofeus ainda nao esta integrado a esta UI.",645,748,1100,2);
            }
            text("O voltar   QUADRADO procurar pasta do jogo",80,977,1780,2);
        } else if (screen == BROWSER) {''', r'''                text("Jogo confirmado nos diretorios de instalacao do PS4.",645,701,1100,2);
                std::string linked=trophy_link_for_game(fs,game.id);
                text(linked.empty() ? "X  Abrir trofeus; se falhar, escolher conjunto manualmente" : "Vinculo salvo: "+linked+"   |   X abrir trofeus",645,748,1100,2);
            }
            text("X trofeus   O voltar",80,977,1780,2);
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
            text("Trophy Hijack somente para o jogo instalado selecionado.",1260,800,540,2);
            text("X desbloquear   CIMA/BAIXO escolher   O voltar",80,977,1780,2);
        } else if (screen == TROPHY_LINK) {
            fill({70,160,1180,750},18,24,35); border({70,160,1180,750});
            fill({1285,160,565,750},18,24,35); border({1285,160,565,750});
            text("VINCULAR TROFEUS AO JOGO",105,187,1100,0);
            if (!library.games.empty()) {
                const tu::Game& game=library.games[selected];
                text(game.title.empty() ? game.id : game.title,105,246,1100);
                text(game.id+"  |  associacao automatica nao encontrada",105,294,1100,2);
            }
            int n=int(catalog_result.sets.size());
            int begin=n ? (catalog_selected/9)*9 : 0;
            for (int row=0; row<9 && begin+row<n; ++row) {
                int index=begin+row, y=365+row*55;
                const tu::TrophySet& set=catalog_result.sets[index];
                if (index == catalog_selected) fill({95,y-3,1130,51},36,61,84);
                text(set.title.empty() ? set.np_communication_id : set.title,115,y,850,2);
                text(set.np_communication_id,965,y,235,2);
            }
            char counter[80]; snprintf(counter,sizeof(counter),"%d / %d",n ? catalog_selected+1 : 0,n);
            text("TODOS OS CONJUNTOS",1320,195,490);
            text(counter,1320,250,490,2);
            if (n) {
                const tu::TrophySet& set=catalog_result.sets[catalog_selected];
                text("Selecionado:",1320,340,490,2);
                text(set.title.empty() ? "Sem titulo" : set.title,1320,390,490,0);
                text(set.np_communication_id,1320,485,490);
                text("X salva este NPWR para o CUSA do jogo.",1320,575,490,2);
                text("O banco de trofeus nao e alterado.",1320,630,490,2);
            }
            text("X vincular   CIMA/BAIXO escolher   ESQ/DIR pagina   O voltar",80,977,1780,2);
        } else if (screen == BROWSER) {''')

rep('''        // First show the known-working SDL interface. No privilege call runs at boot.
        if (first_frame == 1) start_scan();''', r'''        // Prepare GoldHEN access before the first scan so appmeta/icon0.png can be read.
        if (first_frame == 1) {
            access=tu::request_goldhen_access();
            start_scan();
            message=access.acknowledged() ? "Acesso GoldHEN pronto. Buscando jogos, nomes e capas..." : "Buscando jogos. Se capas faltarem, TRIANGULO tenta preparar o acesso novamente.";
        }''')

rep('''            if (requested && !strcmp(requested,"details")) screen=DETAILS;
            if (requested && !strcmp(requested,"diagnostics")) screen=DIAGNOSTICS;''', r'''            if (requested && !strcmp(requested,"details")) screen=DETAILS;
            if (requested && !strcmp(requested,"trophies")) screen=TROPHIES;
            if (requested && !strcmp(requested,"link")) screen=TROPHY_LINK;
            if (requested && !strcmp(requested,"diagnostics")) screen=DIAGNOSTICS;''')

s = s.replace('Trophy Unlocker V13.1', 'Trophy Unlocker V13.10')
s = s.replace('V13.1 - CORRECAO DE ACESSO', 'V13.10 - CAPAS + VINCULO MANUAL DE TROFEUS')
s = s.replace('"QUADRADO   Procurar pastas ou informar CUSA"', '"Somente jogos realmente instalados entram nesta biblioteca."')
s = s.replace('"QUADRADO  Busca manual"', '"X  Abrir jogo selecionado"')
s = s.replace('"ESQ/DIR navegar   X detalhes   QUADRADO buscar manual   TRIANGULO acesso"', '"ESQ/DIR navegar   X selecionar jogo   TRIANGULO acesso   R1 atualizar"')
path.write_text(s)
