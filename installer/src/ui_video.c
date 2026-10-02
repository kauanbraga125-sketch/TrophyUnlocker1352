#include "ui_video.h"
#include <orbis/libkernel.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#define UI_W 1920
#define UI_H 1080
#define UI_BPP 4
#define UI_BUFFERS 2
#define UI_ALIGN 0x200000
#define UI_FLIP_VSYNC 1
#define UI_USER_MAIN 0xFF
#define UI_BUS_MAIN 0

/* Keep the video ABI local so the C build does not depend on C++-style enum headers. */
typedef struct {
    int32_t format;
    int32_t tmode;
    int32_t aspect;
    uint32_t width;
    uint32_t height;
    uint32_t pixelPitch;
    uint64_t reserved[2];
} UiVideoOutBufferAttribute;

extern int32_t sceVideoOutOpen(int32_t userId, int32_t busType, int32_t index, const void *param);
extern int32_t sceVideoOutRegisterBuffers(int32_t handle, int32_t startIndex, void * const *addresses, int32_t bufferNum, const UiVideoOutBufferAttribute *attribute);
extern int32_t sceVideoOutSubmitFlip(int32_t handle, int32_t bufferIndex, uint32_t flipMode, int64_t flipArg);
extern int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t flipRate);
extern int32_t sceVideoOutIsFlipPending(int32_t handle);
extern void sceVideoOutSetBufferAttribute(void *attribute, uint32_t pixelFormat, uint32_t tilingMode, uint32_t aspectRatio, uint32_t width, uint32_t height, uint32_t pitchInPixel);

static int g_ready = 0;
static int32_t g_video = -1;
static void *g_mem = NULL;
static off_t g_mem_off = 0;
static size_t g_mem_size = 0;
static uint32_t *g_fb[UI_BUFFERS] = {0};
static int g_buf = 0;
static int64_t g_frame = 1;

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0x80000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void fill(uint32_t *fb, uint32_t c) {
    size_t total = (size_t)UI_W * (size_t)UI_H;
    for (size_t i = 0; i < total; ++i) fb[i] = c;
}

static void rect(uint32_t *fb, int x, int y, int w, int h, uint32_t c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > UI_W) w = UI_W - x;
    if (y + h > UI_H) h = UI_H - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; ++yy) {
        uint32_t *row = fb + (size_t)yy * UI_W + x;
        for (int xx = 0; xx < w; ++xx) row[xx] = c;
    }
}

static void glyph(char c, uint8_t r[7]) {
    memset(r, 0, 7);
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    switch (c) {
        case 'A': {uint8_t v[7]={14,17,17,31,17,17,17}; memcpy(r,v,7); break;}
        case 'B': {uint8_t v[7]={30,17,17,30,17,17,30}; memcpy(r,v,7); break;}
        case 'C': {uint8_t v[7]={14,17,16,16,16,17,14}; memcpy(r,v,7); break;}
        case 'D': {uint8_t v[7]={30,17,17,17,17,17,30}; memcpy(r,v,7); break;}
        case 'E': {uint8_t v[7]={31,16,16,30,16,16,31}; memcpy(r,v,7); break;}
        case 'F': {uint8_t v[7]={31,16,16,30,16,16,16}; memcpy(r,v,7); break;}
        case 'G': {uint8_t v[7]={14,17,16,23,17,17,15}; memcpy(r,v,7); break;}
        case 'H': {uint8_t v[7]={17,17,17,31,17,17,17}; memcpy(r,v,7); break;}
        case 'I': {uint8_t v[7]={31,4,4,4,4,4,31}; memcpy(r,v,7); break;}
        case 'J': {uint8_t v[7]={7,2,2,2,2,18,12}; memcpy(r,v,7); break;}
        case 'K': {uint8_t v[7]={17,18,20,24,20,18,17}; memcpy(r,v,7); break;}
        case 'L': {uint8_t v[7]={16,16,16,16,16,16,31}; memcpy(r,v,7); break;}
        case 'M': {uint8_t v[7]={17,27,21,21,17,17,17}; memcpy(r,v,7); break;}
        case 'N': {uint8_t v[7]={17,25,21,19,17,17,17}; memcpy(r,v,7); break;}
        case 'O': {uint8_t v[7]={14,17,17,17,17,17,14}; memcpy(r,v,7); break;}
        case 'P': {uint8_t v[7]={30,17,17,30,16,16,16}; memcpy(r,v,7); break;}
        case 'Q': {uint8_t v[7]={14,17,17,17,21,18,13}; memcpy(r,v,7); break;}
        case 'R': {uint8_t v[7]={30,17,17,30,20,18,17}; memcpy(r,v,7); break;}
        case 'S': {uint8_t v[7]={15,16,16,14,1,1,30}; memcpy(r,v,7); break;}
        case 'T': {uint8_t v[7]={31,4,4,4,4,4,4}; memcpy(r,v,7); break;}
        case 'U': {uint8_t v[7]={17,17,17,17,17,17,14}; memcpy(r,v,7); break;}
        case 'V': {uint8_t v[7]={17,17,17,17,17,10,4}; memcpy(r,v,7); break;}
        case 'W': {uint8_t v[7]={17,17,17,21,21,21,10}; memcpy(r,v,7); break;}
        case 'X': {uint8_t v[7]={17,17,10,4,10,17,17}; memcpy(r,v,7); break;}
        case 'Y': {uint8_t v[7]={17,17,10,4,4,4,4}; memcpy(r,v,7); break;}
        case 'Z': {uint8_t v[7]={31,1,2,4,8,16,31}; memcpy(r,v,7); break;}
        case '0': {uint8_t v[7]={14,17,19,21,25,17,14}; memcpy(r,v,7); break;}
        case '1': {uint8_t v[7]={4,12,4,4,4,4,14}; memcpy(r,v,7); break;}
        case '2': {uint8_t v[7]={14,17,1,2,4,8,31}; memcpy(r,v,7); break;}
        case '3': {uint8_t v[7]={30,1,1,14,1,1,30}; memcpy(r,v,7); break;}
        case '4': {uint8_t v[7]={2,6,10,18,31,2,2}; memcpy(r,v,7); break;}
        case '5': {uint8_t v[7]={31,16,16,30,1,1,30}; memcpy(r,v,7); break;}
        case '6': {uint8_t v[7]={14,16,16,30,17,17,14}; memcpy(r,v,7); break;}
        case '7': {uint8_t v[7]={31,1,2,4,8,8,8}; memcpy(r,v,7); break;}
        case '8': {uint8_t v[7]={14,17,17,14,17,17,14}; memcpy(r,v,7); break;}
        case '9': {uint8_t v[7]={14,17,17,15,1,1,14}; memcpy(r,v,7); break;}
        case ':': {uint8_t v[7]={0,4,4,0,4,4,0}; memcpy(r,v,7); break;}
        case '.': {uint8_t v[7]={0,0,0,0,0,4,4}; memcpy(r,v,7); break;}
        case ',': {uint8_t v[7]={0,0,0,0,4,4,8}; memcpy(r,v,7); break;}
        case '-': {uint8_t v[7]={0,0,0,31,0,0,0}; memcpy(r,v,7); break;}
        case '_': {uint8_t v[7]={0,0,0,0,0,0,31}; memcpy(r,v,7); break;}
        case '/': {uint8_t v[7]={1,1,2,4,8,16,16}; memcpy(r,v,7); break;}
        case '[': {uint8_t v[7]={14,8,8,8,8,8,14}; memcpy(r,v,7); break;}
        case ']': {uint8_t v[7]={14,2,2,2,2,2,14}; memcpy(r,v,7); break;}
        case '(': {uint8_t v[7]={2,4,8,8,8,4,2}; memcpy(r,v,7); break;}
        case ')': {uint8_t v[7]={8,4,2,2,2,4,8}; memcpy(r,v,7); break;}
        case '|': {uint8_t v[7]={4,4,4,4,4,4,4}; memcpy(r,v,7); break;}
        case '+': {uint8_t v[7]={0,4,4,31,4,4,0}; memcpy(r,v,7); break;}
        case '!': {uint8_t v[7]={4,4,4,4,4,0,4}; memcpy(r,v,7); break;}
        case '?': {uint8_t v[7]={14,17,1,2,4,0,4}; memcpy(r,v,7); break;}
        case '%': {uint8_t v[7]={17,18,4,8,19,17,0}; memcpy(r,v,7); break;}
        case ' ': default: break;
    }
}

static void draw_char(uint32_t *fb, int x, int y, char c, int scale, uint32_t color) {
    uint8_t rows[7];
    if ((unsigned char)c >= 0x80) c = '?';
    glyph(c, rows);
    for (int yy = 0; yy < 7; ++yy) {
        for (int xx = 0; xx < 5; ++xx) {
            if (rows[yy] & (1u << (4 - xx)))
                rect(fb, x + xx * scale, y + yy * scale, scale, scale, color);
        }
    }
}

static void draw_text(uint32_t *fb, int x, int y, const char *text, int scale, uint32_t color, int max_chars) {
    int sx = x, cx = 0;
    int cw = 6 * scale;
    int lh = 9 * scale;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        unsigned char ch = *p;
        if (ch == '\r') continue;
        if (ch == '\n') { x = sx; y += lh; cx = 0; continue; }
        if (ch >= 0x80) ch = '?';
        if ((max_chars > 0 && cx >= max_chars) || x + cw >= UI_W - 70) {
            x = sx; y += lh; cx = 0;
        }
        if (y + 7 * scale >= UI_H - 60) break;
        draw_char(fb, x, y, (char)ch, scale, color);
        x += cw;
        ++cx;
    }
}

static void present(void) {
    if (!g_ready) return;
    for (int i = 0; i < 200 && sceVideoOutIsFlipPending(g_video) > 0; ++i) usleep(1000);
    (void)sceVideoOutSubmitFlip(g_video, g_buf, UI_FLIP_VSYNC, g_frame++);
    g_buf = (g_buf + 1) % UI_BUFFERS;
}

int ui_init(void) {
    if (g_ready) return 0;
    g_video = sceVideoOutOpen(UI_USER_MAIN, UI_BUS_MAIN, 0, NULL);
    if (g_video < 0) return -101;

    size_t fb_size = (size_t)UI_W * UI_H * UI_BPP;
    size_t need = fb_size * UI_BUFFERS;
    g_mem_size = (need + UI_ALIGN - 1) / UI_ALIGN * UI_ALIGN;
    if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), g_mem_size, UI_ALIGN, 3, &g_mem_off) < 0)
        return -102;
    if (sceKernelMapDirectMemory(&g_mem, g_mem_size, 0x33, 0, g_mem_off, UI_ALIGN) < 0)
        return -103;

    for (int i = 0; i < UI_BUFFERS; ++i)
        g_fb[i] = (uint32_t *)((uint8_t *)g_mem + fb_size * (size_t)i);

    UiVideoOutBufferAttribute attr;
    memset(&attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr, 0x80000000u, 1, 0, UI_W, UI_H, UI_W);
    void *buffers[UI_BUFFERS] = { g_fb[0], g_fb[1] };
    if (sceVideoOutRegisterBuffers(g_video, 0, buffers, UI_BUFFERS, &attr) < 0)
        return -104;
    (void)sceVideoOutSetFlipRate(g_video, 0);
    g_ready = 1;
    ui_render_message("TROPHY UNLOCKER 13.52 V5\nINICIANDO...");
    return 0;
}

void ui_render_message(const char *msg) {
    if (!g_ready || !msg) return;
    uint32_t *fb = g_fb[g_buf];
    uint32_t bg = rgb(18, 22, 30);
    uint32_t panel = rgb(31, 39, 52);
    uint32_t white = rgb(240, 243, 248);
    uint32_t accent = rgb(91, 173, 255);
    uint32_t muted = rgb(165, 174, 188);

    fill(fb, bg);
    rect(fb, 0, 0, UI_W, 150, panel);
    draw_text(fb, 90, 50, "TROPHY UNLOCKER 13.52", 5, white, 0);
    draw_text(fb, 92, 112, "MANAGER V5 - PS4 FW 13.52", 2, accent, 0);
    rect(fb, 90, 185, UI_W - 180, 4, accent);
    draw_text(fb, 100, 245, msg, 3, white, 82);
    rect(fb, 90, UI_H - 135, UI_W - 180, 2, rgb(70, 78, 92));
    draw_text(fb, 100, UI_H - 95, "D-PAD: ESCOLHER   X: ATIVAR   O: CANCELAR   PS: FECHAR APP", 2, muted, 0);
    present();
}
