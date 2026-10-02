#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <orbis/Sysmodule.h>
#include <proto-include.h>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <unistd.h>
#include <vector>

#define W 1920
#define H 1080

struct GameEntry {
    std::string id;
    std::string icon;
    bool external;
    SDL_Texture* texture;
    GameEntry() : external(false), texture(NULL) {}
};

static FT_Library g_ft;
static FT_Face g_title;
static FT_Face g_body;

static SDL_Texture* make_text(SDL_Renderer* renderer, const char* text, FT_Face face)
{
    if (!text || !*text) return NULL;
    int font_h = (int)face->size->metrics.y_ppem;
    int w = (int)strlen(text) * (int)face->size->metrics.x_ppem + 48;
    int h = font_h * 2 + 10;
    if (w < 64) w = 64;
    if (w > 3000) w = 3000;
    SDL_Surface* s = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);
    if (!s) return NULL;
    SDL_FillRect(s, NULL, SDL_MapRGBA(s->format, 0, 0, 0, 0));
    int xoff = 4;
    FT_GlyphSlot slot = face->glyph;
    for (size_t n = 0; n < strlen(text); ++n) {
        FT_UInt gi = FT_Get_Char_Index(face, (unsigned char)text[n]);
        if (FT_Load_Glyph(face, gi, FT_LOAD_DEFAULT) != 0) continue;
        if (FT_Render_Glyph(slot, ft_render_mode_normal) != 0) continue;
        for (int y = 0; y < (int)slot->bitmap.rows; ++y) {
            for (int x = 0; x < (int)slot->bitmap.width; ++x) {
                unsigned char a = slot->bitmap.buffer[y * slot->bitmap.width + x];
                if (!a) continue;
                int px = xoff + x + (slot->metrics.horiBearingX / 64);
                int py = font_h + y - slot->bitmap_top + 2;
                if (px < 0 || py < 0 || px >= s->w || py >= s->h) continue;
                ((uint32_t*)s->pixels)[py * s->w + px] = ((uint32_t)a << 24) | 0x00FFFFFF;
            }
        }
        xoff += slot->advance.x >> 6;
        if (xoff >= s->w - 8) break;
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(s);
    return t;
}

static void draw_tex(SDL_Renderer* r, SDL_Texture* t, int x, int y, int maxw)
{
    if (!t) return;
    int tw=0, th=0;
    SDL_QueryTexture(t, NULL, NULL, &tw, &th);
    if (maxw > 0 && tw > maxw) { th = th * maxw / tw; tw = maxw; }
    SDL_Rect dst={x,y,tw,th};
    SDL_RenderCopy(r,t,NULL,&dst);
}

static void fill(SDL_Renderer* r, SDL_Rect rc, unsigned char rr, unsigned char gg, unsigned char bb)
{
    SDL_SetRenderDrawColor(r,rr,gg,bb,255);
    SDL_RenderFillRect(r,&rc);
}

static void border(SDL_Renderer* r, SDL_Rect rc, unsigned char rr, unsigned char gg, unsigned char bb, int n)
{
    SDL_SetRenderDrawColor(r,rr,gg,bb,255);
    for(int i=0;i<n;i++) { SDL_Rect z={rc.x-i,rc.y-i,rc.w+i*2,rc.h+i*2}; SDL_RenderDrawRect(r,&z); }
}

static bool is_cusa(const char* s)
{
    if (!s || strlen(s)!=9 || strncmp(s,"CUSA",4)!=0) return false;
    for(int i=4;i<9;i++) if(!isdigit((unsigned char)s[i])) return false;
    return true;
}

static bool readable(const char* p) { return p && access(p,R_OK)==0; }

static bool has_game(const std::vector<GameEntry>& games,const char* id)
{
    for(size_t i=0;i<games.size();i++) if(games[i].id==id) return true;
    return false;
}

static void find_icon(GameEntry& g)
{
    char p[256];
    snprintf(p,sizeof(p),"/user/appmeta/%s/icon0.png",g.id.c_str());
    if(readable(p)) { g.icon=p; return; }
    snprintf(p,sizeof(p),"/user/appmeta/external/%s/icon0.png",g.id.c_str());
    if(readable(p)) g.icon=p;
}

static int scan_root(const char* root,bool external,std::vector<GameEntry>& games)
{
    DIR* d=opendir(root);
    if(!d) return -errno;
    int count=0;
    struct dirent* ent;
    while((ent=readdir(d))!=NULL) {
        if(!is_cusa(ent->d_name)) continue;
        count++;
        if(games.size()>=512 || has_game(games,ent->d_name)) continue;
        GameEntry g;
        g.id=ent->d_name;
        g.external=external;
        find_icon(g);
        games.push_back(g);
    }
    closedir(d);
    return count;
}

static int probe_dir(const char* p)
{
    DIR* d=opendir(p);
    if(!d) return -errno;
    closedir(d);
    return 0;
}

static SDL_Texture* cover_texture(SDL_Renderer* r,GameEntry& g)
{
    if(g.texture) return g.texture;
    if(g.icon.empty()) return NULL;
    SDL_Surface* s=IMG_Load(g.icon.c_str());
    if(!s) return NULL;
    g.texture=SDL_CreateTextureFromSurface(r,s);
    SDL_FreeSurface(s);
    return g.texture;
}

static void draw_cover(SDL_Renderer* r,GameEntry& g,SDL_Rect rc,bool focused)
{
    fill(r,rc,23,31,44);
    SDL_Texture* t=cover_texture(r,g);
    if(t) {
        int tw=0,th=0; SDL_QueryTexture(t,NULL,NULL,&tw,&th);
        float scale=std::min((float)(rc.w-24)/(float)tw,(float)(rc.h-24)/(float)th);
        int dw=(int)(tw*scale), dh=(int)(th*scale);
        SDL_Rect dst={rc.x+(rc.w-dw)/2,rc.y+(rc.h-dh)/2,dw,dh};
        SDL_RenderCopy(r,t,NULL,&dst);
    } else {
        SDL_Rect cup={rc.x+rc.w/2-rc.w/10,rc.y+rc.h/4,rc.w/5,rc.h/5};
        fill(r,cup,225,190,70);
        SDL_Rect stem={rc.x+rc.w/2-rc.w/28,cup.y+cup.h,rc.w/14,rc.h/10};
        fill(r,stem,225,190,70);
        SDL_Rect base={rc.x+rc.w/2-rc.w/8,stem.y+stem.h,rc.w/4,rc.h/18};
        fill(r,base,225,190,70);
    }
    border(r,rc,focused?245:95,focused?245:105,focused?250:120,focused?5:2);
}

static void trim_textures(std::vector<GameEntry>& games,int sel)
{
    if(games.empty()) return;
    int n=(int)games.size();
    int l=(sel+n-1)%n, rr=(sel+1)%n;
    for(int i=0;i<n;i++) if(i!=sel && i!=l && i!=rr && games[i].texture) {
        SDL_DestroyTexture(games[i].texture); games[i].texture=NULL;
    }
}

int main(void)
{
    std::vector<GameEntry> games;
    int internal_count=scan_root("/user/app",false,games);
    int external_count=scan_root("/mnt/ext0/user/app",true,games);
    int meta_status=probe_dir("/user/appmeta");
    int data_status=probe_dir("/data");
    std::sort(games.begin(),games.end(),[](const GameEntry&a,const GameEntry&b){return a.id<b.id;});

    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)!=0) for(;;);
    IMG_Init(IMG_INIT_PNG);
    if(sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL)<0) for(;;);
    if(FT_Init_FreeType(&g_ft)!=0) for(;;);
    if(FT_New_Face(g_ft,"/app0/assets/fonts/VeraMono.ttf",0,&g_title)!=0) for(;;);
    if(FT_New_Face(g_ft,"/app0/assets/fonts/VeraMono.ttf",0,&g_body)!=0) for(;;);
    FT_Set_Pixel_Sizes(g_title,0,42);
    FT_Set_Pixel_Sizes(g_body,0,25);

    SDL_Window* win=SDL_CreateWindow("Trophy Unlocker V11",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,W,H,0);
    if(!win) for(;;);
    SDL_Surface* surf=SDL_GetWindowSurface(win);
    SDL_Renderer* r=SDL_CreateSoftwareRenderer(surf);
    if(!r) for(;;);
    if(SDL_NumJoysticks()>0) SDL_JoystickOpen(0);

    SDL_Texture* header=make_text(r,"TROPHY UNLOCKER 13.52",g_title);
    SDL_Texture* subtitle=make_text(r,"V11 - BIBLIOTECA REAL / CAPAS DO PS4",g_body);
    SDL_Texture* controls=make_text(r,"ESQUERDA / DIREITA: NAVEGAR      X: DETALHES      O: VOLTAR",g_body);
    char status[256];
    snprintf(status,sizeof(status),"/user/app: %d   ext0: %d   appmeta: %d   /data: %d   jogos: %u",internal_count,external_count,meta_status,data_status,(unsigned)games.size());
    SDL_Texture* statust=make_text(r,status,g_body);

    int sel=0,details=0,oldsel=-1;
    SDL_Texture* idtext=NULL;
    for(;;) {
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) if(ev.type==SDL_JOYBUTTONDOWN) {
            if(ev.jbutton.button==15 && !games.empty()) { sel=(sel+(int)games.size()-1)%(int)games.size(); details=0; }
            else if(ev.jbutton.button==16 && !games.empty()) { sel=(sel+1)%(int)games.size(); details=0; }
            else if(ev.jbutton.button==0 && !games.empty()) details=1;
            else if(ev.jbutton.button==1) details=0;
        }
        if(!games.empty() && sel!=oldsel) {
            if(idtext) SDL_DestroyTexture(idtext);
            char line[128]; snprintf(line,sizeof(line),"%s   |   %s",games[sel].id.c_str(),games[sel].external?"HD EXTERNO":"INTERNO");
            idtext=make_text(r,line,g_title);
            trim_textures(games,sel);
            oldsel=sel;
        }

        SDL_SetRenderDrawColor(r,11,16,24,255); SDL_RenderClear(r);
        SDL_Rect top={0,0,W,145}; fill(r,top,20,27,39);
        draw_tex(r,header,110,28,820); draw_tex(r,subtitle,112,92,900); draw_tex(r,statust,110,154,1650);

        if(games.empty()) {
            SDL_Rect panel={180,300,1560,440}; fill(r,panel,24,31,44); border(r,panel,95,112,142,3);
            SDL_Texture* a=make_text(r,"A V11 NAO CONSEGUIU VER A BIBLIOTECA AINDA.",g_title);
            SDL_Texture* b=make_text(r,"MANDE UMA FOTO DESTA TELA: OS NUMEROS ACIMA DEFINEM O PROXIMO PASSO.",g_body);
            draw_tex(r,a,250,380,1300); draw_tex(r,b,250,500,1350); SDL_DestroyTexture(a); SDL_DestroyTexture(b);
        } else {
            int n=(int)games.size(); int l=(sel+n-1)%n, rr=(sel+1)%n;
            SDL_Rect lc={210,315,350,350}, cc={690,230,540,540}, rc={1360,315,350,350};
            draw_cover(r,games[l],lc,false); draw_cover(r,games[sel],cc,true); draw_cover(r,games[rr],rc,false);
            draw_tex(r,idtext,640,805,750);
            if(details) {
                SDL_Rect panel={290,900,1340,95}; fill(r,panel,25,32,45); border(r,panel,95,112,142,2);
                char d[384]; snprintf(d,sizeof(d),"CAPA:%s   CAMINHO:%s",games[sel].icon.empty()?"FALLBACK":"ICON0.PNG OK",games[sel].icon.empty()?"nao encontrado":games[sel].icon.c_str());
                SDL_Texture* dt=make_text(r,d,g_body); draw_tex(r,dt,340,930,1250); SDL_DestroyTexture(dt);
            }
        }
        draw_tex(r,controls,110,1018,1500);
        SDL_RenderPresent(r); SDL_UpdateWindowSurface(win); SDL_Delay(16);
    }
    return 0;
}
