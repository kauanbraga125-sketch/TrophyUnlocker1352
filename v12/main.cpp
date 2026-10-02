#include <SDL2/SDL.h>
#include <orbis/Sysmodule.h>
#include <proto-include.h>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <stdint.h>
#include <string>
#include <vector>

#define W 1920
#define H 1080

struct GameEntry {
    std::string id;
    bool external;
    GameEntry() : external(false) {}
};

static FT_Library g_ft;
static FT_Face g_title;
static FT_Face g_body;
static FT_Face g_small;

static SDL_Texture* make_text(SDL_Renderer* renderer, const char* text, FT_Face face)
{
    if (!text || !*text) return NULL;
    int font_h = (int)face->size->metrics.y_ppem;
    int char_w = (int)face->size->metrics.x_ppem;
    int w = (int)strlen(text) * (char_w > 0 ? char_w : 18) + 48;
    int h = font_h * 2 + 12;
    if (w < 64) w = 64;
    if (w > 3400) w = 3400;

    SDL_Surface* s = SDL_CreateRGBSurface(0, w, h, 32, 0, 0, 0, 0);
    if (!s) return NULL;
    SDL_FillRect(s, NULL, SDL_MapRGBA(s->format, 0, 0, 0, 0));

    int xoff = 4;
    FT_GlyphSlot slot = face->glyph;
    size_t len = strlen(text);
    for (size_t n = 0; n < len; ++n) {
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
    int tw = 0, th = 0;
    SDL_QueryTexture(t, NULL, NULL, &tw, &th);
    if (maxw > 0 && tw > maxw) {
        th = th * maxw / tw;
        tw = maxw;
    }
    SDL_Rect dst = { x, y, tw, th };
    SDL_RenderCopy(r, t, NULL, &dst);
}

static void fill(SDL_Renderer* r, SDL_Rect rc, uint8_t rr, uint8_t gg, uint8_t bb)
{
    SDL_SetRenderDrawColor(r, rr, gg, bb, 255);
    SDL_RenderFillRect(r, &rc);
}

static void border(SDL_Renderer* r, SDL_Rect rc, uint8_t rr, uint8_t gg, uint8_t bb, int n)
{
    SDL_SetRenderDrawColor(r, rr, gg, bb, 255);
    for (int i = 0; i < n; ++i) {
        SDL_Rect z = { rc.x - i, rc.y - i, rc.w + i * 2, rc.h + i * 2 };
        SDL_RenderDrawRect(r, &z);
    }
}

static bool is_cusa(const char* s)
{
    if (!s || strlen(s) != 9 || strncmp(s, "CUSA", 4) != 0) return false;
    for (int i = 4; i < 9; ++i) if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

static bool already_added(const std::vector<GameEntry>& games, const char* id)
{
    for (size_t i = 0; i < games.size(); ++i) if (games[i].id == id) return true;
    return false;
}

static int scan_root(const char* root, bool external, std::vector<GameEntry>& games)
{
    DIR* d = opendir(root);
    if (!d) return -errno;
    int count = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (!is_cusa(ent->d_name)) continue;
        ++count;
        if (games.size() >= 512 || already_added(games, ent->d_name)) continue;
        GameEntry g;
        g.id = ent->d_name;
        g.external = external;
        games.push_back(g);
    }
    closedir(d);
    return count;
}

static int probe_dir(const char* path)
{
    DIR* d = opendir(path);
    if (!d) return -errno;
    closedir(d);
    return 0;
}

static void draw_placeholder_cover(SDL_Renderer* r, SDL_Rect rc, bool focused)
{
    fill(r, rc, 30, 39, 55);
    SDL_Rect cup = { rc.x + rc.w / 2 - rc.w / 10, rc.y + rc.h / 4, rc.w / 5, rc.h / 5 };
    SDL_Rect stem = { rc.x + rc.w / 2 - rc.w / 28, cup.y + cup.h, rc.w / 14, rc.h / 10 };
    SDL_Rect base = { rc.x + rc.w / 2 - rc.w / 8, stem.y + stem.h, rc.w / 4, rc.h / 18 };
    fill(r, cup, 225, 190, 70);
    fill(r, stem, 225, 190, 70);
    fill(r, base, 225, 190, 70);
    border(r, rc, focused ? 244 : 78, focused ? 244 : 90, focused ? 248 : 110, focused ? 5 : 2);
}

struct Diagnostics {
    int internal_count;
    int external_count;
    int appmeta_status;
    int data_status;
    int total_games;
    bool ran;
    Diagnostics() : internal_count(-999), external_count(-999), appmeta_status(-999), data_status(-999), total_games(0), ran(false) {}
};

static void clear_diag_textures(SDL_Texture* lines[], int n)
{
    for (int i = 0; i < n; ++i) {
        if (lines[i]) SDL_DestroyTexture(lines[i]);
        lines[i] = NULL;
    }
}

static void build_diag_textures(SDL_Renderer* r, const Diagnostics& d, SDL_Texture* lines[], int n)
{
    clear_diag_textures(lines, n);
    char buf[160];
    snprintf(buf, sizeof(buf), "/user/app: %d", d.internal_count);
    lines[0] = make_text(r, buf, g_small);
    snprintf(buf, sizeof(buf), "HD externo: %d", d.external_count);
    lines[1] = make_text(r, buf, g_small);
    snprintf(buf, sizeof(buf), "/user/appmeta: %d", d.appmeta_status);
    lines[2] = make_text(r, buf, g_small);
    snprintf(buf, sizeof(buf), "/data: %d", d.data_status);
    lines[3] = make_text(r, buf, g_small);
    snprintf(buf, sizeof(buf), "Jogos detectados: %d", d.total_games);
    lines[4] = make_text(r, buf, g_small);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) for (;;);
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) < 0) for (;;);
    if (FT_Init_FreeType(&g_ft) != 0) for (;;);
    if (FT_New_Face(g_ft, "/app0/assets/fonts/VeraMono.ttf", 0, &g_title) != 0) for (;;);
    if (FT_New_Face(g_ft, "/app0/assets/fonts/VeraMono.ttf", 0, &g_body) != 0) for (;;);
    if (FT_New_Face(g_ft, "/app0/assets/fonts/VeraMono.ttf", 0, &g_small) != 0) for (;;);
    FT_Set_Pixel_Sizes(g_title, 0, 42);
    FT_Set_Pixel_Sizes(g_body, 0, 26);
    FT_Set_Pixel_Sizes(g_small, 0, 21);

    SDL_Window* win = SDL_CreateWindow("Trophy Unlocker V12", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H, 0);
    if (!win) for (;;);
    SDL_Surface* surf = SDL_GetWindowSurface(win);
    SDL_Renderer* r = SDL_CreateSoftwareRenderer(surf);
    if (!r) for (;;);
    if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);

    SDL_Texture* header = make_text(r, "TROPHY UNLOCKER 13.52", g_title);
    SDL_Texture* subtitle = make_text(r, "V12 - INTERFACE ORGANIZADA", g_body);
    SDL_Texture* footer_library = make_text(r, "ESQUERDA/DIREITA navegar    X detalhes    TRIANGULO diagnostico", g_small);
    SDL_Texture* footer_details = make_text(r, "O voltar", g_small);
    SDL_Texture* diag_title = make_text(r, "DIAGNOSTICO", g_body);
    SDL_Texture* library_title = make_text(r, "BIBLIOTECA", g_body);
    SDL_Texture* waiting = make_text(r, "Pressione TRIANGULO para executar o diagnostico de acesso.", g_body);
    SDL_Texture* none_found = make_text(r, "Nenhum CUSA visivel. Consulte o painel de diagnostico ao lado.", g_body);
    SDL_Texture* detail_title = make_text(r, "DETALHES DO JOGO", g_title);
    SDL_Texture* trophy_note = make_text(r, "Trofeus ainda nao sao alterados nesta versao.", g_body);

    SDL_Texture* diag_lines[5] = {0};
    SDL_Texture* selected_id = NULL;
    SDL_Texture* selected_location = NULL;

    std::vector<GameEntry> games;
    Diagnostics diag;
    int sel = 0;
    int oldsel = -1;
    int screen = 0;

    for (;;) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type != SDL_JOYBUTTONDOWN) continue;
            if (screen == 0) {
                if (ev.jbutton.button == 15 && !games.empty()) {
                    sel = (sel + (int)games.size() - 1) % (int)games.size();
                } else if (ev.jbutton.button == 16 && !games.empty()) {
                    sel = (sel + 1) % (int)games.size();
                } else if (ev.jbutton.button == 0 && !games.empty()) {
                    screen = 1;
                } else if (ev.jbutton.button == 3) {
                    games.clear();
                    sel = 0;
                    oldsel = -1;
                    diag.internal_count = scan_root("/user/app", false, games);
                    diag.external_count = scan_root("/mnt/ext0/user/app", true, games);
                    diag.appmeta_status = probe_dir("/user/appmeta");
                    diag.data_status = probe_dir("/data");
                    std::sort(games.begin(), games.end(), [](const GameEntry& a, const GameEntry& b) { return a.id < b.id; });
                    diag.total_games = (int)games.size();
                    diag.ran = true;
                    build_diag_textures(r, diag, diag_lines, 5);
                }
            } else if (ev.jbutton.button == 1) {
                screen = 0;
            }
        }

        if (!games.empty() && sel != oldsel) {
            if (selected_id) SDL_DestroyTexture(selected_id);
            if (selected_location) SDL_DestroyTexture(selected_location);
            selected_id = make_text(r, games[sel].id.c_str(), g_title);
            selected_location = make_text(r, games[sel].external ? "Armazenamento: HD externo" : "Armazenamento: interno", g_body);
            oldsel = sel;
        }

        SDL_SetRenderDrawColor(r, 11, 16, 24, 255);
        SDL_RenderClear(r);

        SDL_Rect header_bg = { 0, 0, W, 128 };
        fill(r, header_bg, 20, 27, 39);
        draw_tex(r, header, 80, 24, 900);
        draw_tex(r, subtitle, 82, 79, 1100);

        if (screen == 0) {
            SDL_Rect library_panel = { 70, 160, 1270, 760 };
            SDL_Rect diag_panel = { 1380, 160, 470, 760 };
            fill(r, library_panel, 18, 24, 35);
            fill(r, diag_panel, 18, 24, 35);
            border(r, library_panel, 60, 74, 96, 2);
            border(r, diag_panel, 60, 74, 96, 2);
            draw_tex(r, library_title, 105, 185, 500);
            draw_tex(r, diag_title, 1415, 185, 380);

            if (!diag.ran) {
                draw_tex(r, waiting, 150, 420, 1100);
            } else if (games.empty()) {
                draw_tex(r, none_found, 150, 420, 1100);
            } else {
                int n = (int)games.size();
                int left = (sel + n - 1) % n;
                int right = (sel + 1) % n;
                SDL_Rect lc = { 145, 330, 275, 390 };
                SDL_Rect cc = { 510, 255, 390, 540 };
                SDL_Rect rc = { 990, 330, 275, 390 };
                draw_placeholder_cover(r, lc, false);
                draw_placeholder_cover(r, cc, true);
                draw_placeholder_cover(r, rc, false);
                draw_tex(r, selected_id, 565, 810, 650);
            }

            if (!diag.ran) {
                SDL_Texture* p = make_text(r, "Aguardando teste...", g_small);
                draw_tex(r, p, 1420, 250, 380);
                SDL_DestroyTexture(p);
            } else {
                for (int i = 0; i < 5; ++i) draw_tex(r, diag_lines[i], 1420, 250 + i * 82, 390);
            }

            SDL_Rect footer = { 0, 950, W, 130 };
            fill(r, footer, 16, 22, 32);
            draw_tex(r, footer_library, 85, 985, 1700);
        } else {
            SDL_Rect detail_panel = { 90, 170, 1740, 720 };
            fill(r, detail_panel, 18, 24, 35);
            border(r, detail_panel, 60, 74, 96, 2);
            draw_tex(r, detail_title, 130, 205, 700);

            if (!games.empty()) {
                SDL_Rect cover = { 160, 325, 390, 520 };
                draw_placeholder_cover(r, cover, true);
                draw_tex(r, selected_id, 660, 340, 850);
                draw_tex(r, selected_location, 660, 445, 900);
                draw_tex(r, trophy_note, 660, 650, 1000);
            }

            SDL_Rect footer = { 0, 950, W, 130 };
            fill(r, footer, 16, 22, 32);
            draw_tex(r, footer_details, 85, 985, 700);
        }

        SDL_RenderPresent(r);
        SDL_UpdateWindowSurface(win);
        SDL_Delay(16);
    }

    return 0;
}
