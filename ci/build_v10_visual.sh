#!/usr/bin/env bash
set -euxo pipefail

ROOT="$PWD"
DEPS="$ROOT/.deps_v10_visual"
WORK="$ROOT/v10_visual_work"
DIST="$ROOT/dist_v10"
OO_VERSION=v0.5.4
OO_ASSET=toolchain-llvm-18.tar.gz

rm -rf "$DEPS" "$WORK" "$DIST"
mkdir -p "$DEPS" "$WORK" "$DIST"

sudo apt-get update
sudo apt-get install -y clang-18 lld-18 llvm-18 make curl tar unzip file
sudo ln -sf /usr/bin/clang-18 /usr/local/bin/clang
sudo ln -sf /usr/bin/clang++-18 /usr/local/bin/clang++
sudo ln -sf /usr/bin/ld.lld-18 /usr/local/bin/ld.lld
sudo ln -sf /usr/bin/llvm-ar-18 /usr/local/bin/llvm-ar
sudo ln -sf /usr/bin/llvm-ranlib-18 /usr/local/bin/llvm-ranlib
command -v docker

cd "$DEPS"
curl -fL --retry 5 --retry-all-errors \
  "https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/${OO_VERSION}/${OO_ASSET}" \
  -o "$OO_ASSET"
echo "3c7cd5bb593ca74fa1c13fd59f3938dc0fc07985167f7275063019e63abe4526  $OO_ASSET" | sha256sum -c -
tar xzf "$OO_ASSET"
OO_PS4_TOOLCHAIN="$(find "$DEPS" -type f -name link.x -printf '%h\n' | head -n1)"
test -f "$OO_PS4_TOOLCHAIN/link.x"
chmod +x "$OO_PS4_TOOLCHAIN/bin/linux/"* || true
export OO_PS4_TOOLCHAIN

# Start from the official OpenOrbis SDL2 sample. This avoids our old direct framebuffer path.
cp -a "$OO_PS4_TOOLCHAIN/samples/SDL2" "$WORK/app"
cd "$WORK/app"

# Keep the official font assets and module dependencies but replace the sample game code.
rm -f SDL2/*.cpp SDL2/*.h
cat > SDL2/main.cpp <<'EOF'
#include <SDL2/SDL.h>
#include <orbis/Sysmodule.h>
#include <proto-include.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define W 1920
#define H 1080

struct Color { uint8_t r, g, b; };
static FT_Library g_ft;
static FT_Face g_title;
static FT_Face g_body;

static SDL_Texture* make_text(SDL_Renderer* renderer, const char* text, FT_Face face)
{
    int font_h = (int)face->size->metrics.y_ppem;
    int w = (int)strlen(text) * (int)face->size->metrics.x_ppem + 32;
    int h = font_h * 2 + 8;
    if (w < 64) w = 64;

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
        SDL_Rect z = { rc.x - i, rc.y - i, rc.w + i*2, rc.h + i*2 };
        SDL_RenderDrawRect(r, &z);
    }
}

static void draw_cover(SDL_Renderer* r, int game, SDL_Rect rc, int focused)
{
    static const uint8_t colors[3][3] = {
        {40, 105, 180}, {130, 55, 155}, {35, 145, 105}
    };
    fill(r, rc, colors[game][0], colors[game][1], colors[game][2]);

    SDL_Rect band = { rc.x, rc.y + rc.h - rc.h/4, rc.w, rc.h/4 };
    fill(r, band, 18, 22, 30);

    SDL_Rect trophy = { rc.x + rc.w/2 - rc.w/10, rc.y + rc.h/4, rc.w/5, rc.h/5 };
    fill(r, trophy, 225, 190, 70);
    SDL_Rect stem = { rc.x + rc.w/2 - rc.w/28, trophy.y + trophy.h, rc.w/14, rc.h/10 };
    fill(r, stem, 225, 190, 70);
    SDL_Rect base = { rc.x + rc.w/2 - rc.w/8, stem.y + stem.h, rc.w/4, rc.h/18 };
    fill(r, base, 225, 190, 70);

    border(r, rc, focused ? 245 : 100, focused ? 245 : 105, focused ? 250 : 115, focused ? 5 : 2);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0)
        for (;;);

    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) < 0)
        for (;;);
    if (FT_Init_FreeType(&g_ft) != 0)
        for (;;);
    if (FT_New_Face(g_ft, "/app0/assets/fonts/VeraMono.ttf", 0, &g_title) != 0)
        for (;;);
    if (FT_New_Face(g_ft, "/app0/assets/fonts/VeraMono.ttf", 0, &g_body) != 0)
        for (;;);
    FT_Set_Pixel_Sizes(g_title, 0, 46);
    FT_Set_Pixel_Sizes(g_body, 0, 28);

    SDL_Window* win = SDL_CreateWindow("Trophy Unlocker V10", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, W, H, 0);
    if (!win) for (;;);

    SDL_Surface* surf = SDL_GetWindowSurface(win);
    SDL_Renderer* r = SDL_CreateSoftwareRenderer(surf);
    if (!r) for (;;);

    SDL_Joystick* pad = NULL;
    if (SDL_NumJoysticks() > 0) pad = SDL_JoystickOpen(0);

    SDL_Texture* title = make_text(r, "TROPHY UNLOCKER 13.52", g_title);
    SDL_Texture* sub = make_text(r, "V10 VISUAL SMOKE - INTERFACE SEGURA", g_body);
    SDL_Texture* hint = make_text(r, "ESQUERDA / DIREITA: NAVEGAR     X: DETALHES     O: VOLTAR", g_body);
    SDL_Texture* notice = make_text(r, "CAPAS DE TESTE - NENHUM PRX E NENHUM TROFEU ALTERADO", g_body);
    SDL_Texture* gameText[3] = {
        make_text(r, "JOGO TESTE 1", g_title),
        make_text(r, "JOGO TESTE 2", g_title),
        make_text(r, "JOGO TESTE 3", g_title)
    };
    SDL_Texture* detail = make_text(r, "TELA DE DETALHES: AQUI ENTRARAO TROFEUS E PROGRESSO", g_body);

    int sel = 0;
    int details = 0;

    for (;;) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_JOYBUTTONDOWN) {
                if (ev.jbutton.button == 15) { sel = (sel + 2) % 3; details = 0; }
                else if (ev.jbutton.button == 16) { sel = (sel + 1) % 3; details = 0; }
                else if (ev.jbutton.button == 0) { details = 1; }
                else if (ev.jbutton.button == 1) { details = 0; }
            }
        }

        SDL_SetRenderDrawColor(r, 12, 16, 24, 255);
        SDL_RenderClear(r);

        SDL_Rect top = {0, 0, W, 150};
        fill(r, top, 20, 26, 38);
        draw_tex(r, title, 90, 35, 900);
        draw_tex(r, sub, 95, 100, 900);

        int left = (sel + 2) % 3;
        int right = (sel + 1) % 3;
        SDL_Rect lc = {250, 300, 300, 420};
        SDL_Rect cc = {730, 220, 460, 610};
        SDL_Rect rc = {1370, 300, 300, 420};
        draw_cover(r, left, lc, 0);
        draw_cover(r, sel, cc, 1);
        draw_cover(r, right, rc, 0);

        draw_tex(r, gameText[sel], 760, 850, 600);

        if (details) {
            SDL_Rect panel = {420, 880, 1080, 100};
            fill(r, panel, 25, 31, 44);
            border(r, panel, 95, 110, 140, 2);
            draw_tex(r, detail, 465, 905, 980);
        }

        draw_tex(r, hint, 90, 985, 1500);
        draw_tex(r, notice, 90, 1030, 1500);

        SDL_RenderPresent(r);
        SDL_UpdateWindowSurface(win);
        SDL_Delay(16);
    }

    return 0;
}
EOF

cat > Makefile <<'EOF'
TITLE       := Trophy Unlocker 13.52 V10 Visual
VERSION     := 01.00
TITLE_ID    := BREW13528
CONTENT_ID  := IV0000-BREW13528_00-TROPHYVISUALV100

LIBS        := -lc -lkernel -lc++ -lSceUserService -lSceVideoOut -lSceAudioOut -lScePad -lSceSysmodule -lSceFreeType -lSDL2 -lSDL2_image
EXTRAFLAGS  := -fexceptions -fcxx-exceptions
ASSETS      := $(wildcard assets/**/*)
LIBMODULES  := $(wildcard sce_module/*)
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
PROJDIR     := SDL2
INTDIR      := $(PROJDIR)/x64/Debug
CFILES      := $(wildcard $(PROJDIR)/*.c)
CPPFILES    := $(wildcard $(PROJDIR)/*.cpp)
OBJS        := $(patsubst $(PROJDIR)/%.c,$(INTDIR)/%.o,$(CFILES)) $(patsubst $(PROJDIR)/%.cpp,$(INTDIR)/%.o,$(CPPFILES))
CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c $(EXTRAFLAGS) -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o
_unused     := $(shell mkdir -p $(INTDIR))
CC          := clang
CCX         := clang++
LD          := ld.lld
CDIR        := linux

all: $(CONTENT_ID).pkg

$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png $(LIBMODULES) $(ASSETS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

sce_sys/param.sfo: Makefile
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

eboot.bin: $(INTDIR) $(OBJS)
	$(LD) $(INTDIR)/*.o -o $(INTDIR)/$(PROJDIR).elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(INTDIR)/$(PROJDIR).elf -out=$(INTDIR)/$(PROJDIR).oelf --eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/%.o: $(PROJDIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: $(PROJDIR)/%.cpp
	$(CCX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(CONTENT_ID).pkg pkg.gp4 sce_sys/param.sfo eboot.bin $(INTDIR)/*.o $(INTDIR)/*.elf $(INTDIR)/*.oelf
EOF

TOOL="$OO_PS4_TOOLCHAIN/bin/linux/PkgTool.Core"
mv "$TOOL" "$TOOL.real"
docker pull mcr.microsoft.com/dotnet/core/runtime:3.1-bionic
cat > "$TOOL" <<EOF
#!/usr/bin/env bash
set -e
exec docker run --rm --user "$(id -u):$(id -g)" \
  -e DOTNET_ROLL_FORWARD=Minor -e DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 \
  -v /home/runner/work:/home/runner/work -w "\$PWD" \
  mcr.microsoft.com/dotnet/core/runtime:3.1-bionic \
  "$OO_PS4_TOOLCHAIN/bin/linux/PkgTool.Core.real" "\$@"
EOF
chmod +x "$TOOL"
"$TOOL" version

make clean || true
make
PKG="IV0000-BREW13528_00-TROPHYVISUALV100.pkg"
test -s "$PKG"
"$TOOL" sfo_listentries sce_sys/param.sfo
"$TOOL" pkg_validate --verbose "$PKG"

cp "$PKG" "$DIST/Trophy_Unlocker_13.52_V10_VISUAL.pkg"
sha256sum "$DIST/Trophy_Unlocker_13.52_V10_VISUAL.pkg" > "$DIST/SHA256SUMS.txt"
file "$DIST/Trophy_Unlocker_13.52_V10_VISUAL.pkg"
ls -lh "$DIST"
