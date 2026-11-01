/*
 * options_row.c - PC SETTINGS, a row of the game's own OPTIONS screen that
 * opens the settings overlay (Double-S.T.E.A.L 2's options_row.c, which
 * has the same job on that game's later version of this screen engine).
 *
 * The screen is data plus code. data\Screen\optionUS.xpr holds a layout
 * (the data section's first 0x5DC bytes) and two textures: scr_main2.tga
 * (512x512 DXT1, the background, at +0x600) and option_moji.tga (512x512
 * DXT5 at +0x2B100: the title, the labels, the values, the arrows, and the
 * art of a CONTROLLER-TYPE row only another language's screen shows)
 * [verified: the file's XPR header]. The layout, offsets from its start:
 *   +00 +04  textures: count, offset (8 bytes each: name, texture)
 *   +08 +0C  pictures: count, offset (24: texture, u0 | v0 << 16,
 *            u1 | v1 << 16 in 1/4096, width | height << 16 on screen, 0, 1)
 *   +10 +14  picture sets: count, offset (none here)
 *   +18 +1C  texts: count, offset (8: a name, a style)
 *   +20 +24  pages: count, offset (offsets of the page records)
 *   +28      the string pool
 * A page: name, element count and offset (from the page), a second list's
 * count and offset (none here). An element, 32 bytes: name, kind | picture
 * << 16, flags | x << 16 (flags 0x20: selectable, 0x01: centred), y, colour,
 * item (u16 at +0x14), parent (u16 at +0x1E, 0xFFFF none) [verified:
 * sub_000A68A0 relocates exactly these; sub_000A6F40 selects the
 * selectable element whose item is the cursor's]. Bunkasha's tool left
 * bytes of its source text in the fields an element does not use. This is
 * DS2's format four bytes shorter in the header and with no navigation
 * links: the rows here are code (recomp_hooks.c, sub_000DDDC0).
 *
 * When the layout is about to be relocated (sub_000A68A0, its one caller
 * the screen loader sub_000A7CB0), a copy with the row added is built and
 * relocated instead: a picture and an element, PcSetting (item 70) under
 * MUSIC TEST at y 288, and EXIT one row lower. The copy goes into rows 96 to
 * 99 of option_moji.tga, which no picture uses; the label into the right
 * half of the VIBRATION cell (256, 320), empty too. Both are the screen
 * resource's own memory, freed with it. The label is cut from the screen's
 * own lettering at run time from the player's copy of the game (nothing of
 * it is in this repository): P of CONTROLLER-TYPE, C of CONTROLLER, S and E
 * of SE VOLUME, T of MUSIC TEST twice, I, N and G of ENGINE VOLUME, S of
 * MUSIC, set as the labels are (a column of outline between letters, the
 * labels' word space) [verified: work/opt/label.py, the same cuts].
 *
 * The same copy carries the PC SETTINGS page the row opens (pc_page.c):
 * text elements in the game's font (kind 2, a text record each, as the
 * volumes' numbers are) for the labels and values,
 * the values selectable with the volumes' arrows tied to them, MORE
 * SETTINGS and QUIT GAME, and a second EXIT; all placed WR_PC_AWAY to the
 * right of the screen until the page opens. The elements' places are
 * copied into their objects when the screen is built (an object's x and y
 * are floats at +0x0C and +0x10; a text's string is at +0x24) [verified:
 * runs exp1, exp2: moving a def after that changes nothing, the objects
 * hold 130.0, 128.0 for VIBRATION].
 */
#include "options_row.h"

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOGP "[WR] "

extern ptrdiff_t g_xbox_mem_offset;
#define GP(a) ((uint8_t *)((uintptr_t)(uint32_t)(a) + g_xbox_mem_offset))

static uint32_t rd32(uint32_t a) { uint32_t v; memcpy(&v, GP(a), 4); return v; }
static void wr32(uint32_t a, uint32_t v) { memcpy(GP(a), &v, 4); }
static void wr16(uint32_t a, uint16_t v) { memcpy(GP(a), &v, 2); }

/* What this layout is (anything else is left alone). */
#define L_SIZE    0x5DCu
#define N_PIC     13
#define N_ELEM    24
#define E_SOUND_TEST 6
#define E_EXIT    13
#define E_ARROW_L 16                  /* Icon_Option3a; 17 Icon_Option3b */
#define N_TEXT    (4 + 2 * WR_PC_SETTINGS + 2)
#define AWAY      ((int)WR_PC_AWAY)
#define TEX1      0x2B100u            /* option_moji.tga's texels, from the layout */
#define TEX_W     512

/* What is added. */
#define PIC_PC    N_PIC               /* 13: (256, 320)-(512, 352) of option_moji.tga */
#define E_PC      N_ELEM              /* 24 */
#define PC_X      130                 /* the labels' */
#define PC_Y      288                 /* MUSIC TEST's 256 + a row */
#define EXIT_Y    322                 /* 290 + a row */
#define LABEL_X   256
#define LABEL_Y   320
#define COPY_ROW  96                  /* the layout's copy: rows 96-127, 16 KB, empty */
#define COPY_ROOM (32 / 4 * (TEX_W / 4) * 16)

/* The PC SETTINGS page's places (pc_page.c). */
#define PG_Y0     WR_PG_Y0
#define PG_DY     WR_PG_DY
#define PG_VX     WR_PG_VX
#define PG_EXIT_Y 342
#define PG_LABEL  0xFF40E070u         /* the labels' green */
#define PG_VALUE  0xFFFFD030u         /* the values' yellow, as ON and OFF */
#define PG_STYLE  0x0080              /* the texts' font: see the texts below */

/* ── DXT5 ──────────────────────────────────────────────────────────── */

typedef struct { uint8_t r, g, b, a; } px;

static void c565(uint16_t c, px *o)
{
    int r = c >> 11 & 31, g = c >> 5 & 63, b = c & 31;
    o->r = (uint8_t)(r << 3 | r >> 2);
    o->g = (uint8_t)(g << 2 | g >> 4);
    o->b = (uint8_t)(b << 3 | b >> 2);
    o->a = 255;
}

static uint16_t to565(px p)
{
    return (uint16_t)((p.r * 31 + 127) / 255 << 11 | (p.g * 63 + 127) / 255 << 5 | (p.b * 31 + 127) / 255);
}

static px mix(px a, px b, int wa, int wb)
{
    px o;
    int w = wa + wb;
    o.r = (uint8_t)((a.r * wa + b.r * wb) / w);
    o.g = (uint8_t)((a.g * wa + b.g * wb) / w);
    o.b = (uint8_t)((a.b * wa + b.b * wb) / w);
    o.a = 255;
    return o;
}

static void palette(uint16_t c0, uint16_t c1, px pal[4])
{
    c565(c0, &pal[0]);
    c565(c1, &pal[1]);
    pal[2] = mix(pal[0], pal[1], 2, 1);
    pal[3] = mix(pal[0], pal[1], 1, 2);
}

/* A whole DXT5 texture, w x h, to RGBA. */
static px *dxt5_decode(const uint8_t *src, int w, int h)
{
    px *img = calloc((size_t)w * h, sizeof *img);
    int bx, by, i;

    if (!img)
        return NULL;
    for (by = 0; by < h / 4; by++)
        for (bx = 0; bx < w / 4; bx++) {
            const uint8_t *b = src + ((size_t)by * (w / 4) + bx) * 16, *c = b + 8;
            uint16_t c0 = (uint16_t)(c[0] | c[1] << 8), c1 = (uint16_t)(c[2] | c[3] << 8);
            uint32_t idx = (uint32_t)c[4] | (uint32_t)c[5] << 8 | (uint32_t)c[6] << 16 | (uint32_t)c[7] << 24;
            uint8_t al[8];
            uint64_t ai = 0;
            px pal[4];

            palette(c0, c1, pal);
            al[0] = b[0];
            al[1] = b[1];
            if (al[0] > al[1])
                for (i = 1; i < 7; i++)
                    al[i + 1] = (uint8_t)(((7 - i) * al[0] + i * al[1]) / 7);
            else {
                for (i = 1; i < 5; i++)
                    al[i + 1] = (uint8_t)(((5 - i) * al[0] + i * al[1]) / 5);
                al[6] = 0;
                al[7] = 255;
            }
            for (i = 0; i < 6; i++)
                ai |= (uint64_t)b[2 + i] << (8 * i);
            for (i = 0; i < 16; i++) {
                px p = pal[idx >> (2 * i) & 3];
                p.a = al[ai >> (3 * i) & 7];
                img[(size_t)(by * 4 + i / 4) * w + bx * 4 + i % 4] = p;
            }
        }
    return img;
}

static int dist2(px a, px b)
{
    int dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

/* The colour half: the two colours farthest apart among the pixels that
 * show, the rest the nearest of the four. */
static void enc_color(const px p[16], uint8_t out[8])
{
    int i, j, best = -1, e0 = 0, e1 = 0, any = 0;
    uint16_t c0, c1;
    uint32_t idx = 0;
    px pal[4];

    for (i = 0; i < 16; i++) {
        if (!p[i].a)
            continue;
        any = 1;
        for (j = i; j < 16; j++) {
            int d = p[j].a ? dist2(p[i], p[j]) : -1;
            if (d > best) {
                best = d;
                e0 = i;
                e1 = j;
            }
        }
    }
    c0 = any ? to565(p[e0]) : 0;
    c1 = any ? to565(p[e1]) : 0;
    palette(c0, c1, pal);
    for (i = 0; i < 16; i++) {
        int k, bk = 0, bd = 1 << 30;
        for (k = 0; k < 4; k++)
            if (dist2(p[i], pal[k]) < bd) { bd = dist2(p[i], pal[k]); bk = k; }
        idx |= (uint32_t)bk << (2 * i);
    }
    out[0] = (uint8_t)c0; out[1] = (uint8_t)(c0 >> 8);
    out[2] = (uint8_t)c1; out[3] = (uint8_t)(c1 >> 8);
    out[4] = (uint8_t)idx; out[5] = (uint8_t)(idx >> 8);
    out[6] = (uint8_t)(idx >> 16); out[7] = (uint8_t)(idx >> 24);
}

/* The alpha half: eight steps between the extremes, or six between those
 * of the rest with 0 and 255 exact, whichever is nearer. */
static int enc_alpha_mode(const px p[16], uint8_t a0, uint8_t a1, uint8_t out[8])
{
    uint8_t al[8];
    uint64_t ai = 0;
    int i, k, err = 0;

    al[0] = a0;
    al[1] = a1;
    if (a0 > a1)
        for (i = 1; i < 7; i++)
            al[i + 1] = (uint8_t)(((7 - i) * a0 + i * a1) / 7);
    else {
        for (i = 1; i < 5; i++)
            al[i + 1] = (uint8_t)(((5 - i) * a0 + i * a1) / 5);
        al[6] = 0;
        al[7] = 255;
    }
    for (i = 0; i < 16; i++) {
        int bk = 0, bd = 1 << 30;
        for (k = 0; k < 8; k++) {
            int d = (p[i].a - al[k]) * (p[i].a - al[k]);
            if (d < bd) { bd = d; bk = k; }
        }
        err += bd;
        ai |= (uint64_t)bk << (3 * i);
    }
    out[0] = a0;
    out[1] = a1;
    for (i = 0; i < 6; i++)
        out[2 + i] = (uint8_t)(ai >> (8 * i));
    return err;
}

static void enc_alpha(const px p[16], uint8_t out[8])
{
    uint8_t lo = 255, hi = 0, mlo = 255, mhi = 0, b8[8];
    int i, e8, e6;

    for (i = 0; i < 16; i++) {
        uint8_t a = p[i].a;
        if (a < lo) lo = a;
        if (a > hi) hi = a;
        if (a != 0 && a != 255) {
            if (a < mlo) mlo = a;
            if (a > mhi) mhi = a;
        }
    }
    if (mlo > mhi)                    /* only 0 and 255 */
        mlo = mhi = 0;
    e8 = hi > lo ? enc_alpha_mode(p, hi, lo, b8) : 1 << 30;
    e6 = enc_alpha_mode(p, mlo, mhi, out);
    if (e8 < e6)
        memcpy(out, b8, 8);
}

/* An RGBA picture w x h (multiples of 4) into a DXT5 texture's blocks at
 * block (bx0, by0). */
static void dxt5_encode(const px *img, int w, int h, uint8_t *tex, int bx0, int by0)
{
    int bx, by, i;

    for (by = 0; by < h / 4; by++)
        for (bx = 0; bx < w / 4; bx++) {
            uint8_t *b = tex + ((size_t)(by0 + by) * (TEX_W / 4) + bx0 + bx) * 16;
            px blk[16];
            for (i = 0; i < 16; i++)
                blk[i] = img[(size_t)(by * 4 + i / 4) * w + bx * 4 + i % 4];
            enc_alpha(blk, b);
            enc_color(blk, b + 8);
        }
}

/* ── the label ─────────────────────────────────────────────────────── */

/* A letter: the 32 rows of a label cell from y, columns x0..x1 (its fill
 * from f0 to f1, an outline column either side and the shadow); 0 a word
 * space. */
typedef struct { short y, x0, x1, f0, f1; } glyph;

static const glyph k_pc[] = {
    { 256, 150, 163, 151, 161 },      /* P of CONTROLLER-TYPE */
    { 256,   3,  16,   4,  14 },      /* C of CONTROLLER-TYPE */
    {   0,   0,   0,   0,   0 },
    { 384,   2,  15,   3,  14 },      /* S of SE VOLUME */
    { 384,  15,  27,  16,  24 },      /* E of SE VOLUME */
    { 448,  69,  81,  70,  80 },      /* T of MUSIC TEST */
    { 448,  69,  81,  70,  80 },      /* T */
    { 416,  39,  46,  40,  44 },      /* I of ENGINE VOLUME */
    { 416,  46,  59,  47,  57 },      /* N of ENGINE VOLUME */
    { 416,  26,  39,  27,  37 },      /* G of ENGINE VOLUME */
    { 448,  33,  46,  34,  44 },      /* S of MUSIC TEST */
};

/* Pixels that show in the four cells the letters come from (their first
 * 180 columns), the US screen's: another language's labels are other
 * words. */
#define SRC_SHOWING 11123
#define SRC_SLACK   60

static int empty_rect(const px *img, int x0, int y0, int x1, int y1)
{
    int x, y;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (img[(size_t)y * TEX_W + x].a)
                return 0;
    return 1;
}

static int draw_label(uint32_t tex)
{
    static const short cells[] = { 256, 384, 416, 448 };
    px *src = dxt5_decode(GP(tex), TEX_W, TEX_W);
    px *lab = calloc(256 * 32, sizeof *lab);
    int ok = 0, i, k, x, y, showing = 0, at = 4, prev = -1;

    if (!src || !lab)
        goto out;
    for (i = 0; i < 4; i++)
        for (y = cells[i]; y < cells[i] + 32; y++)
            for (x = 0; x < 180; x++)
                showing += src[(size_t)y * TEX_W + x].a != 0;
    if (abs(showing - SRC_SHOWING) > SRC_SLACK
            || !empty_rect(src, 0, COPY_ROW, TEX_W, COPY_ROW + 32)
            || !empty_rect(src, LABEL_X, LABEL_Y, TEX_W, LABEL_Y + 32)) {
        fprintf(stderr, LOGP "optionUS.xpr: its pictures are not the ones expected"
                        " (%d showing); no PC SETTINGS row\n", showing);
        goto out;
    }
    for (i = 0; i < (int)(sizeof k_pc / sizeof k_pc[0]); i++) {
        const glyph *g = &k_pc[i];
        int dx;
        if (!g->y) {                  /* the labels' word space: 5 columns */
            prev += 3;
            continue;
        }
        if (prev >= 0)
            at = prev + 3;            /* a column of outline between fills */
        dx = at - g->f0;
        for (k = 0; k < 32; k++)
            for (x = g->x0; x <= g->x1; x++) {
                px s = src[(size_t)(g->y + k) * TEX_W + x], *d;
                int tx = x + dx, a, under, oa;
                if (tx < 0 || tx >= 256 || !s.a)
                    continue;
                /* Straight alpha, source over what is there. */
                d = &lab[(size_t)k * 256 + tx];
                a = s.a;
                under = d->a * (255 - a) / 255;
                oa = a + under;
                d->r = (uint8_t)((s.r * a + d->r * under + oa / 2) / oa);
                d->g = (uint8_t)((s.g * a + d->g * under + oa / 2) / oa);
                d->b = (uint8_t)((s.b * a + d->b * under + oa / 2) / oa);
                d->a = (uint8_t)oa;
            }
        prev = g->f1 + dx;
    }
    dxt5_encode(lab, 256, 32, GP(tex), LABEL_X / 4, LABEL_Y / 4);
    ok = 1;
out:
    free(src);
    free(lab);
    return ok;
}

/* ── the layout ────────────────────────────────────────────────────── */

static int name_is(uint32_t pool, uint32_t off, const char *want)
{
    return off < 0x200 && !strcmp((const char *)GP(pool + off), want);
}

/* The layout at L checked; its page record in *page. */
static int layout_is_options(uint32_t L, uint32_t *page)
{
    uint32_t pool, P, E;

    /* The counts and offsets first, before anything is read through them:
     * every screen's layout comes here. */
    if (rd32(L) != 2 || rd32(L + 8) != N_PIC || rd32(L + 0x10) != 0 || rd32(L + 0x18) != 4
            || rd32(L + 0x20) != 1 || rd32(L + 0x24) >= L_SIZE || rd32(L + 0x28) >= L_SIZE)
        return 0;
    pool = L + rd32(L + 0x28);
    if (rd32(L + rd32(L + 0x24)) >= L_SIZE)
        return 0;
    P = L + rd32(L + rd32(L + 0x24));
    if (!name_is(pool, rd32(P), "Option") || rd32(P + 4) != N_ELEM || rd32(P + 8) >= L_SIZE
            || rd32(P + 0xC) != 0)
        return 0;
    E = P + rd32(P + 8);
    if (E + 32 * N_ELEM != pool || !name_is(pool, rd32(E + 32 * E_SOUND_TEST), "SOUND_TEST")
            || !name_is(pool, rd32(E + 32 * E_EXIT), "Exit")
            || (rd32(E + 32 * E_EXIT + 12) & 0xFFFF) != 290
            || (rd32(E + 32 * E_EXIT + 20) & 0xFFFF) != 60)
        return 0;
    *page = P;
    return 1;
}

static int s_patched;

int wr_options_row_on(void)
{
    return s_patched;
}

uint32_t wr_options_row_patch(uint32_t L)
{
    static const char k_name[] = "PcSetting";
    uint32_t P, tex = L + TEX1, D = tex + COPY_ROW / 4 * (TEX_W / 4) * 16;
    uint32_t pool_off, pool_len, at, pics, texts, pages, rec, elems, pool, e, name_at, i;
    char nm[16];

    if (!layout_is_options(L, &P))
        return 0;
    s_patched = 0;
    if (!draw_label(tex))
        return 0;
    pool_off = rd32(L + 0x28);
    pool_len = L_SIZE - pool_off;

    /* Header, textures as they were; the pictures and one more. */
    at = 0x2C;
    memcpy(GP(D + at), GP(L + rd32(L + 4)), 16);
    at += 16;
    pics = at;
    memcpy(GP(D + pics), GP(L + rd32(L + 0xC)), N_PIC * 24);
    at += N_PIC * 24;
    wr32(D + at + 0, 1);                                  /* option_moji.tga */
    wr32(D + at + 4, (LABEL_X * 8) | (LABEL_Y * 8) << 16);
    wr32(D + at + 8, (TEX_W * 8) | ((LABEL_Y + 32) * 8) << 16);
    wr32(D + at + 12, 256 | 32 << 16);
    wr32(D + at + 16, 0);
    wr32(D + at + 20, 1);
    at += 24;
    /* The texts: the four volumes' numbers, and one for each text of the
     * page (a label and a value a setting row, MORE SETTINGS, QUIT GAME),
     * each as the first number's ("0", its style). */
    texts = at;
    memcpy(GP(D + texts), GP(L + rd32(L + 0x1C)), 4 * 8);
    for (i = 4; i < N_TEXT; i++) {
        memcpy(GP(D + texts + 8 * i), GP(L + rd32(L + 0x1C)), 8);
        /* Style 0x80, the save screen's names' (Font18.xpr, every Latin-1
         * letter); the volumes' 0x10 has digits and a few letters. */
        wr16(D + texts + 8 * i + 4, PG_STYLE);
    }
    at += N_TEXT * 8;
    pages = at;
    rec = at + 4;
    wr32(D + pages, rec);
    elems = rec + 0x14;
    wr32(D + rec + 0, rd32(P));                           /* "Option" */
    wr32(D + rec + 4, WR_E_COUNT);
    wr32(D + rec + 8, elems - rec);
    wr32(D + rec + 12, 0);
    wr32(D + rec + 16, 0);
    memcpy(GP(D + elems), GP(P + rd32(P + 8)), N_ELEM * 32);
    /* EXIT a row lower; the row itself, as the labels but selectable. */
    wr32(D + elems + 32 * E_EXIT + 12, EXIT_Y);
    e = D + elems + 32 * E_PC;
    memset(GP(e), 0, 32);
    wr32(e + 0, pool_len);                                /* the name, after the pool */
    wr32(e + 4, (uint32_t)PIC_PC << 16);
    wr32(e + 8, 0x60u | (uint32_t)PC_X << 16);       /* selectable, as EXIT's and the values' */
    wr32(e + 12, PC_Y);
    wr32(e + 16, 0xFFFFFFFFu);
    wr16(e + 0x14, WR_OPTIONS_PC_ITEM);
    wr16(e + 0x1E, 0xFFFF);
    /* The pool after the elements: the original's, "PcSetting", then the
     * page's names ("Pc25" ...). */
    pool = elems + 32 * WR_E_COUNT;
    if (pool + pool_len + sizeof k_name + 8 * (WR_E_COUNT - WR_E_PC_FIRST) > COPY_ROOM) {
        fprintf(stderr, LOGP "PC SETTINGS row: the layout copy does not fit\n");
        return 0;
    }
    memcpy(GP(D + pool), GP(L + pool_off), pool_len);
    memcpy(GP(D + pool + pool_len), k_name, sizeof k_name);
    /* The page, away to the right. */
    name_at = pool_len + sizeof k_name;
    for (i = WR_E_PC_FIRST; i < WR_E_COUNT; i++) {
        uint32_t row, y, def = D + elems + 32 * i;
        int k = i - WR_E_PC_FIRST;
        row = i < WR_E_PC_MORE ? (uint32_t)k / 4 : (uint32_t)(WR_PC_ROW_MORE + (i - WR_E_PC_MORE));
        y = PG_Y0 + PG_DY * row;
        if (i < WR_E_PC_MORE && k % 4 >= 2) {             /* an arrow: the volumes' */
            memcpy(GP(def), GP(D + elems + 32 * (E_ARROW_L + (k % 4 - 2))), 32);
            wr32(def + 8, (rd32(def + 8) & 0xFFFF)
                 | (uint32_t)(PG_VX + (k % 4 == 2 ? -20 : 70) + AWAY) << 16);
            wr32(def + 12, y);
            wr16(def + 0x1E, (uint16_t)WR_E_PC_VALUE(row));
        } else if (i == WR_E_PC_EXIT) {                   /* EXIT: OPTIONS' own */
            memcpy(GP(def), GP(D + elems + 32 * E_EXIT), 32);
            wr32(def + 8, (rd32(def + 8) & 0xFFFF) | (uint32_t)(320 + AWAY) << 16);
            wr32(def + 12, PG_EXIT_Y);
            wr16(def + 0x14, WR_PC_ITEM(WR_PC_ROW_EXIT));
        } else {                                          /* a text */
            int value = i < WR_E_PC_MORE && k % 4 == 1;
            uint32_t text = i < WR_E_PC_MORE ? 4 + 2 * row + (uint32_t)value
                                             : 4 + 2 * WR_PC_SETTINGS + (row - WR_PC_ROW_MORE);
            memset(GP(def), 0, 32);
            wr32(def + 4, 2u | text << 16);
            wr32(def + 8, (value || i >= WR_E_PC_MORE ? 0x60u : 0u)
                 | (uint32_t)((value ? PG_VX : PC_X) + AWAY) << 16);
            wr32(def + 12, y);
            wr32(def + 16, value ? PG_VALUE : PG_LABEL);
            if (value || i >= WR_E_PC_MORE)
                wr16(def + 0x14, (uint16_t)WR_PC_ITEM(row));
            wr16(def + 0x1E, 0xFFFF);
        }
        wr32(def + 0, name_at);
        snprintf(nm, sizeof nm, "Pc%u", i);
        memcpy(GP(D + pool + name_at), nm, strlen(nm) + 1);
        name_at += (uint32_t)strlen(nm) + 1;
    }
    /* The header last, as the original's. */
    wr32(D + 0x00, 2);
    wr32(D + 0x04, 0x2C);
    wr32(D + 0x08, N_PIC + 1);
    wr32(D + 0x0C, pics);
    wr32(D + 0x10, 0);
    wr32(D + 0x14, 0);
    wr32(D + 0x18, N_TEXT);
    wr32(D + 0x1C, texts);
    wr32(D + 0x20, 1);
    wr32(D + 0x24, pages);
    wr32(D + 0x28, pool);
    s_patched = 1;
    {
        static int told;
        if (!told++)
            fprintf(stderr, LOGP "OPTIONS: PC SETTINGS row added (layout copy at 0x%08X)\n", D);
    }
    return D;
}
