#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#ifdef TU_HOST_PREVIEW
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdlib>
#else
#include <orbis/Sysmodule.h>
#include <proto-include.h>
#endif
#include "library.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <deque>

static const int W = 1920, H = 1080;
static FT_Library ft;
static FT_Face faces[3];
static SDL_Renderer* renderer;

static uint32_t next_codepoint(const char*& p) {
    unsigned char c = static_cast<unsigned char>(*p++);
    if (c < 128) return c;
    unsigned count; uint32_t value, minimum;
    if ((c & 0xe0) == 0xc0) { count=1; value=c&31; minimum=0x80; }
    else if ((c & 0xf0) == 0xe0) { count=2; value=c&15; minimum=0x800; }
    else if ((c & 0xf8) == 0xf0) { count=3; value=c&7; minimum=0x10000; }
    else return '?';
    for (unsigned i=0; i<count; ++i) {
        unsigned char q = static_cast<unsigned char>(*p);
        if (!q || (q&0xc0) != 0x80) return '?';
        ++p; value=(value<<6)|(q&63);
    }
    return value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) ? '?' : value;
}
static SDL_Texture* make_text(const std::string& text, int font) {
    if (text.empty()) return nullptr;
    FT_Face face = faces[font]; int width=12;
    const char* p=text.c_str();
    while (*p && width < 3400) {
        if (!FT_Load_Char(face, next_codepoint(p), FT_LOAD_DEFAULT)) width += face->glyph->advance.x>>6;
    }
    width=std::min(3400, std::max(32, width));
    int font_h=int(face->size->metrics.y_ppem), height=font_h*2+12;
    SDL_Surface* s=SDL_CreateRGBSurface(0,width,height,32,0x00ff0000,0x0000ff00,0x000000ff,0xff000000);
    if (!s) return nullptr;
    SDL_FillRect(s,nullptr,0);
    if (SDL_LockSurface(s) < 0) { SDL_FreeSurface(s); return nullptr; }
    int xoff=4; p=text.c_str();
    while (*p && xoff < width-4) {
        if (FT_Load_Char(face,next_codepoint(p),FT_LOAD_RENDER)) continue;
        FT_GlyphSlot g=face->glyph;
        for (int y=0; y<int(g->bitmap.rows); ++y) for (int x=0; x<int(g->bitmap.width); ++x) {
            int pitch=g->bitmap.pitch;
            int row=pitch < 0 ? int(g->bitmap.rows)-1-y : y;
            unsigned char a=g->bitmap.buffer[row*(pitch < 0 ? -pitch : pitch)+x];
            int px=xoff+x+g->bitmap_left, py=font_h+y-g->bitmap_top+2;
            if (px < 0 || py < 0 || px >= width || py >= height) continue;
            uint32_t* pixels=reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(s->pixels)+py*s->pitch);
            pixels[px]=(uint32_t(a)<<24)|0x00ffffff;
        }
        xoff += g->advance.x>>6;
    }
    SDL_UnlockSurface(s);
    SDL_Texture* texture=SDL_CreateTextureFromSurface(renderer,s);
    SDL_FreeSurface(s);
    if (texture) SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
    return texture;
}
struct TextCache { std::string text; int font; SDL_Texture* texture; };
static std::deque<TextCache> text_cache;
static void text(const std::string& str,int x,int y,int maxwidth,int font=1) {
    SDL_Texture* texture=nullptr;
    for (const TextCache& entry : text_cache) if (entry.font == font && entry.text == str) { texture=entry.texture; break; }
    if (!texture) {
        texture=make_text(str,font);
        if (!texture) return;
        if (text_cache.size() >= 96) { SDL_DestroyTexture(text_cache.front().texture); text_cache.pop_front(); }
        text_cache.push_back({str,font,texture});
    }
    int w,h; SDL_QueryTexture(texture,nullptr,nullptr,&w,&h);
    if (w > maxwidth) { h=h*maxwidth/w; w=maxwidth; }
    SDL_Rect target={x,y,w,h}; SDL_RenderCopy(renderer,texture,nullptr,&target);
}
static void fill(SDL_Rect rect,uint8_t r,uint8_t g,uint8_t b) {
    SDL_SetRenderDrawColor(renderer,r,g,b,255); SDL_RenderFillRect(renderer,&rect);
}
static void border(SDL_Rect rect,bool focused=false) {
    SDL_SetRenderDrawColor(renderer,focused ? 244 : 60,focused ? 244 : 74,focused ? 248 : 96,255);
    for (int i=0; i<(focused ? 4 : 2); ++i) { SDL_Rect r={rect.x-i,rect.y-i,rect.w+i*2,rect.h+i*2}; SDL_RenderDrawRect(renderer,&r); }
}
static void placeholder(SDL_Rect rect) {
    fill(rect,30,39,55);
    SDL_Rect cup={rect.x+rect.w/2-rect.w/10,rect.y+rect.h/4,rect.w/5,rect.h/5};
    fill(cup,225,190,70);
    SDL_Rect stem={rect.x+rect.w/2-rect.w/28,cup.y+cup.h,rect.w/14,rect.h/10};
    fill(stem,225,190,70);
    fill({rect.x+rect.w/2-rect.w/8,stem.y+stem.h,rect.w/4,rect.h/18},225,190,70);
}
struct Cover { std::string path; SDL_Texture* texture=nullptr; };
static Cover covers[3];
static void draw_cover(tu::FileSystem& fs,const tu::Game& game,SDL_Rect rect,bool focused,int slot) {
    Cover& cover=covers[slot];
    if (cover.path != game.icon) {
        if (cover.texture) SDL_DestroyTexture(cover.texture);
        cover.texture=nullptr; cover.path=game.icon;
        std::vector<uint8_t> bytes;
        if (!game.icon.empty() && !fs.read(game.icon,4*1024*1024,bytes) && tu::valid_png(bytes)) {
            SDL_RWops* rw=SDL_RWFromConstMem(bytes.data(),int(bytes.size()));
            SDL_Surface* image=rw ? IMG_Load_RW(rw,1) : nullptr;
            if (image) { cover.texture=SDL_CreateTextureFromSurface(renderer,image); SDL_FreeSurface(image); }
        }
    }
    placeholder(rect);
    if (cover.texture) {
        int w,h; SDL_QueryTexture(cover.texture,nullptr,nullptr,&w,&h);
        if (w > 0 && h > 0) {
            int rw=rect.w, rh=h*rw/w;
            if (rh > rect.h) { rh=rect.h; rw=w*rh/h; }
            SDL_Rect target={rect.x+(rect.w-rw)/2,rect.y+(rect.h-rh)/2,rw,rh};
            SDL_RenderCopy(renderer,cover.texture,nullptr,&target);
        }
    }
    border(rect,focused);
}
static std::string error_text(int status) {
    if (!status) return "Acessivel";
    if (status == -2) return "Pasta ausente ou isolada";
    if (status == -13 || status == -1) return "Acesso negado";
    if (status == -20) return "Nao e uma pasta";
    char line[80]; snprintf(line,sizeof(line),"Falha de leitura (%d)",status); return line;
}
enum Screen { LIBRARY, DETAILS, BROWSER, CUSA_INPUT, DIAGNOSTICS };
struct Browser {
    bool roots=true;
    std::string path;
    std::vector<tu::DirEntry> entries;
    int selected=0,status=0;
    void locations() {
        roots=true; path.clear(); selected=0; status=0; entries.clear();
        for (const char* root : {"/user/app","/user/appmeta","/mnt/ext0/user/app","/mnt/ext1/user/app","/mnt/usb0","/mnt/usb1","/data","/"}) entries.push_back({root,4});
    }
    void open(tu::FileSystem& fs,const std::string& target) {
        path=target; roots=false; selected=0;
        status=fs.list(path,entries);
        if (!status) {
            entries.erase(std::remove_if(entries.begin(),entries.end(),[](const tu::DirEntry& e) { return e.type != 4 && e.type != 10 && e.type != 0; }),entries.end());
            std::sort(entries.begin(),entries.end(),[](const tu::DirEntry& a,const tu::DirEntry& b) { return a.name < b.name; });
        }
    }
};
static int button(const SDL_Event& e) {
    if (e.type == SDL_JOYBUTTONDOWN) return e.jbutton.button;
    if (e.type != SDL_KEYDOWN || e.key.repeat) return -1;
    switch (e.key.keysym.sym) {
        case SDLK_RETURN: case SDLK_x: return 0;
        case SDLK_ESCAPE: case SDLK_o: return 1;
        case SDLK_s: return 2;
        case SDLK_t: return 3;
        case SDLK_q: return 4;
        case SDLK_e: return 5;
        case SDLK_d: return 9;
        case SDLK_UP: return 13;
        case SDLK_DOWN: return 14;
        case SDLK_LEFT: return 15;
        case SDLK_RIGHT: return 16;
        default: return -1;
    }
}
int main() {
    setvbuf(stdout,nullptr,_IONBF,0);
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)) for (;;) SDL_Delay(1000);
#ifndef TU_HOST_PREVIEW
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) < 0) for (;;) SDL_Delay(1000);
    const char* fontpath="/app0/assets/fonts/VeraMono.ttf";
#else
    const char* fontpath=std::getenv("TU_PREVIEW_FONT");
    if (!fontpath) return 2;
#endif
    if (FT_Init_FreeType(&ft)) for (;;) SDL_Delay(1000);
    for (int i=0; i<3; ++i) {
        if (FT_New_Face(ft,fontpath,0,&faces[i])) for (;;) SDL_Delay(1000);
        FT_Set_Pixel_Sizes(faces[i],0,i == 0 ? 40 : i == 1 ? 27 : 21);
    }
    SDL_Window* win=SDL_CreateWindow("Trophy Unlocker V13.1",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,W,H,0);
    if (!win) for (;;) SDL_Delay(1000);
    SDL_Surface* surface=SDL_GetWindowSurface(win);
    renderer=surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!renderer) for (;;) SDL_Delay(1000);
    IMG_Init(IMG_INIT_PNG);
    SDL_Joystick* joystick=SDL_NumJoysticks() > 0 ? SDL_JoystickOpen(0) : nullptr;
    tu::FileSystem& fs=tu::native_filesystem();
    tu::Library library(fs);
    tu::AccessResult access;
    Browser browser; browser.locations();
    Screen screen=LIBRARY, return_screen=LIBRARY;
    int selected=0, digit=0;
    std::string cusa="CUSA00000", message="Buscando jogos nos locais conhecidos...";
    int first_frame=0;
    bool pending_access=false, scan_finished=false;
    int manual_load=-2, diagnostic_save=-2;

    auto start_scan = [&]() {
        manual_load=library.load_manual(); library.begin_scan();
        selected=0; scan_finished=false; message="Buscando jogos... voce pode usar a busca manual.";
        for (Cover& cover : covers) { if (cover.texture) SDL_DestroyTexture(cover.texture); cover.texture=nullptr; cover.path.clear(); }
    };
    auto added = [&](int index) {
        if (index < 0) {
            message=index == -22 ? "Selecione a pasta CUSA ou a pasta que contem param.sfo." : "Nao foi possivel adicionar: "+error_text(index);
            return;
        }
        selected=index; int saved=library.save_manual();
        message=saved ? "Adicionado nesta sessao. Nao foi possivel salvar a lista em /data." : "Jogo adicionado. Selecao manual salva.";
        screen=DETAILS;
    };
    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_JOYDEVICEADDED && !joystick) joystick=SDL_JoystickOpen(event.jdevice.which);
            if (event.type == SDL_JOYDEVICEREMOVED && joystick && event.jdevice.which == SDL_JoystickInstanceID(joystick)) { SDL_JoystickClose(joystick); joystick=nullptr; }
            int key=button(event);
            if (key < 0) continue;
            if (screen == LIBRARY) {
                int n=int(library.games.size());
                if (key == 15 && n) selected=(selected+n-1)%n;
                else if (key == 16 && n) selected=(selected+1)%n;
                else if (key == 0 && n) screen=DETAILS;
                else if (key == 2) { browser.locations(); screen=BROWSER; message="Escolha um local para procurar o jogo."; }
                else if (key == 3) { pending_access=true; message="Preparando acesso pelo GoldHEN..."; }
                else if (key == 5) start_scan();
                else if (key == 9) screen=DIAGNOSTICS;
            } else if (screen == DETAILS) {
                if (key == 1) screen=LIBRARY;
                else if (key == 2) { browser.locations(); screen=BROWSER; }
            } else if (screen == BROWSER) {
                int n=int(browser.entries.size());
                if (key == 13 && n) browser.selected=(browser.selected+n-1)%n;
                else if (key == 14 && n) browser.selected=(browser.selected+1)%n;
                else if (key == 0 && n) {
                    std::string target=browser.roots ? browser.entries[browser.selected].name : tu::join_path(browser.path,browser.entries[browser.selected].name);
                    browser.open(fs,target); message=browser.status ? error_text(browser.status)+". O volta; TRIANGULO informa o CUSA." : "QUADRADO adiciona a pasta aberta; X entra na pasta selecionada.";
                } else if (key == 1) {
                    if (browser.roots) screen=LIBRARY;
                    else if (browser.path == "/") browser.locations();
                    else browser.open(fs,tu::parent_path(browser.path));
                } else if (key == 4) browser.locations();
                else if (key == 2 && !browser.roots && !browser.status) added(library.add_folder(browser.path));
                else if (key == 3) { cusa="CUSA00000"; digit=0; return_screen=BROWSER; screen=CUSA_INPUT; message="Use as setas para informar os cinco numeros do CUSA."; }
            } else if (screen == CUSA_INPUT) {
                if (key == 15) digit=(digit+4)%5;
                else if (key == 16) digit=(digit+1)%5;
                else if (key == 13) cusa[4+digit]=char('0'+(cusa[4+digit]-'0'+1)%10);
                else if (key == 14) cusa[4+digit]=char('0'+(cusa[4+digit]-'0'+9)%10);
                else if (key == 1) screen=return_screen;
                else if (key == 0) added(library.add_id(cusa));
            } else if (screen == DIAGNOSTICS && key == 1) screen=LIBRARY;
        }
        // First show the known-working SDL interface. No privilege call runs at boot.
        if (first_frame == 1) start_scan();
        if (pending_access) {
            access=tu::request_goldhen_access(); pending_access=false; start_scan();
            message=access.acknowledged() ? "GoldHEN respondeu. Conferindo o acesso real as pastas..." : "Pedido de acesso sem confirmacao. Use OPTIONS para ver o resultado.";
        }
        // One directory or one game's metadata per frame, never a whole library loop.
        if (library.busy() && screen == LIBRARY) library.scan_step();
        if (!library.busy() && !scan_finished && first_frame > 1) {
            scan_finished=true;
            message=library.games.empty() ? "Nenhum jogo visivel. TRIANGULO prepara o acesso; QUADRADO abre a busca manual." : "Biblioteca pronta. X abre detalhes; QUADRADO permite procurar outro jogo.";
            diagnostic_save=fs.save("/data/TrophyUnlocker1352/v13-diagnostic.txt",library.diagnostic(access));
        }
        if (!library.games.empty()) selected=std::max(0,std::min(selected,int(library.games.size())-1));

        SDL_SetRenderDrawColor(renderer,11,16,24,255); SDL_RenderClear(renderer);
        fill({0,0,W,128},20,27,39);
        text("TROPHY UNLOCKER 13.52",80,22,1000,0);
        text("V13.1 - CORRECAO DE ACESSO",82,77,1200);
        fill({0,950,W,130},16,22,32);
        if (screen == LIBRARY) {
            fill({70,160,1270,760},18,24,35); border({70,160,1270,760});
            fill({1380,160,470,760},18,24,35); border({1380,160,470,760});
            text("BIBLIOTECA",105,182,650);
            text(library.busy() ? "BUSCANDO..." : "ACESSO E BUSCA",1410,182,410);
            if (library.games.empty()) {
                text(library.busy() ? "Procurando jogos..." : "Nenhum jogo visivel ainda",160,385,1090,0);
                text("TRIANGULO  Preparar acesso GoldHEN",160,475,1090);
                text("QUADRADO   Procurar pastas ou informar CUSA",160,530,1090);
                text("A busca manual continua disponivel mesmo sem resultados.",160,620,1090,2);
            } else {
                int n=int(library.games.size()); const tu::Game& game=library.games[selected];
                if (n > 1) draw_cover(fs,library.games[(selected+n-1)%n],{145,330,275,365},false,0);
                draw_cover(fs,game,{490,270,420,470},true,1);
                if (n > 2) draw_cover(fs,library.games[(selected+1)%n],{990,330,275,365},false,2);
                text(game.title.empty() ? game.id : game.title,120,775,1150,0);
                text(game.id+"  |  "+tu::game_status(game),120,844,1150,2);
            }
            char count[80]; snprintf(count,sizeof(count),"%zu jogo(s) na lista",library.games.size());
            text(count,1410,260,410);
            text(access.attempted ? (access.acknowledged() ? "GoldHEN: resposta OK" : "GoldHEN: ver OPTIONS") : "GoldHEN: TRIANGULO",1410,325,410,2);
            for (size_t i=0; i<library.roots.size() && i<4; ++i) {
                const tu::RootResult& root=library.roots[i];
                const char* labels[]={"Interno","Externo 0","Externo 1","Metadados"};
                char line[100]; snprintf(line,sizeof(line),"%s: %d",labels[i],root.status ? root.status : root.matches);
                text(line,1410,400+int(i)*57,410,2);
            }
            text("R1  Atualizar lista",1410,690,410,2);
            text("OPTIONS  Diagnostico",1410,747,410,2);
            text("QUADRADO  Busca manual",1410,804,410,2);
            text("ESQ/DIR navegar   X detalhes   QUADRADO buscar manual   TRIANGULO acesso",80,977,1780,2);
        } else if (screen == DETAILS) {
            fill({90,160,1740,750},18,24,35); border({90,160,1740,750});
            text("DETALHES DO JOGO",130,193,1400,0);
            if (!library.games.empty()) {
                const tu::Game& game=library.games[selected];
                draw_cover(fs,game,{150,330,420,470},true,1);
                text(game.title.empty() ? game.id : game.title,645,305,1120,0);
                text(game.id,645,382,1100);
                text(tu::game_status(game),645,460,1100);
                text("Local:",645,542,1000,2);
                text(game.path.empty() ? "Ainda nao localizado" : game.path,645,585,1110,2);
                text("Esta etapa identifica jogos e seus arquivos.",645,701,1100,2);
                text("O desbloqueio de trofeus ainda nao esta integrado a esta UI.",645,748,1100,2);
            }
            text("O voltar   QUADRADO procurar pasta do jogo",80,977,1780,2);
        } else if (screen == BROWSER) {
            fill({90,160,1740,750},18,24,35); border({90,160,1740,750});
            text("BUSCA MANUAL",130,188,1400,0);
            text(browser.roots ? "Escolha onde procurar" : browser.path,130,263,1620);
            if (browser.status) {
                text(error_text(browser.status),160,395,1450,0);
                text("Volte com O e tente outro local, ou informe o CUSA com TRIANGULO.",160,486,1530);
                text("Informar um CUSA nao libera pastas bloqueadas.",160,561,1450,2);
            } else if (browser.entries.empty()) {
                text("Nenhuma subpasta visivel.",160,406,1450);
                text("QUADRADO seleciona esta pasta. O volta.",160,477,1450);
            } else {
                int begin=(browser.selected/8)*8;
                for (int row=0; row<8 && begin+row<int(browser.entries.size()); ++row) {
                    int index=begin+row, y=341+row*62;
                    if (index == browser.selected) fill({125,y-2,1670,58},36,61,84);
                    text((index == browser.selected ? ">  " : "   ")+browser.entries[index].name,148,y,1615);
                }
            }
            text("CIMA/BAIXO escolher   X abrir   QUADRADO usar pasta aberta   TRIANGULO CUSA   O voltar   L1 locais",80,977,1780,2);
        } else if (screen == CUSA_INPUT) {
            fill({90,160,1740,750},18,24,35); border({90,160,1740,750});
            text("INFORMAR CUSA",130,193,1400,0);
            text("Digite os cinco numeros do identificador do jogo.",220,320,1480);
            text("CUSA",450,446,340,0);
            for (int i=0; i<5; ++i) {
                SDL_Rect cell={780+i*120,415,92,110}; fill(cell,30,39,55); border(cell,i == digit);
                text(std::string(1,cusa[4+i]),cell.x+30,cell.y+29,60,0);
            }
            text("Procure o CUSA na pasta do jogo ou nos dados de instalacao.",220,623,1490);
            text("Se os arquivos estiverem bloqueados, a entrada sera marcada como manual.",220,705,1490,2);
            text("ESQ/DIR escolher numero   CIMA/BAIXO alterar   X adicionar   O cancelar",80,977,1780,2);
        } else {
            fill({90,160,1740,750},18,24,35); border({90,160,1740,750});
            text("DIAGNOSTICO DE ACESSO",130,187,1500,0);
            char line[200];
            snprintf(line,sizeof(line),"GoldHEN: solicitado=%d  SDK bruto=%lld  CF=%llu",int(access.attempted),(long long)access.sdk,(unsigned long long)access.sdk_carry);
            text(line,140,271,1590,2);
            if (access.jailbreak_attempted) snprintf(line,sizeof(line),"Acesso: comando enviado  retorno bruto=%lld  CF=%llu",(long long)access.jailbreak,(unsigned long long)access.jailbreak_carry);
            else snprintf(line,sizeof(line),"Acesso: comando ainda nao enviado");
            text(line,140,316,1590,2);
            for (size_t i=0; i<library.roots.size(); ++i) {
                const tu::RootResult& root=library.roots[i];
                snprintf(line,sizeof(line),"%s   resultado=%d   CUSA=%d",root.path.c_str(),root.status,root.matches);
                text(line,140,380+int(i)*49,1580,2);
            }
            snprintf(line,sizeof(line),"Lista manual: %d   Salvar diagnostico: %d",manual_load,diagnostic_save);
            text(line,140,731,1580,2);
            text("-2: ausente/isolada   -13: negado   0: leitura OK",140,779,1580,2);
            text("Log: /data/TrophyUnlocker1352/v13-diagnostic.txt",140,828,1580,2);
            text("O voltar",80,977,1780,2);
        }
        text(message,80,1030,1780,2);
        SDL_RenderPresent(renderer); SDL_UpdateWindowSurface(win);
#ifdef TU_HOST_PREVIEW
        // Host-only visual fixture, excluded from the PS4 executable.
        if (first_frame == 24) {
            const char* requested=std::getenv("TU_PREVIEW_SCREEN");
            if (requested && !strcmp(requested,"browser")) { browser.locations(); screen=BROWSER; }
            if (requested && !strcmp(requested,"cusa")) screen=CUSA_INPUT;
            if (requested && !strcmp(requested,"details")) screen=DETAILS;
            if (requested && !strcmp(requested,"diagnostics")) screen=DIAGNOSTICS;
        }
        if (first_frame == 28) {
            const char* output=std::getenv("TU_PREVIEW_OUTPUT");
            if (output) SDL_SaveBMP(surface,output);
            return 0;
        }
#endif
        if (first_frame < 100) ++first_frame;
        SDL_Delay(16);
    }
}
