/*
 * pc_page.c - the PC SETTINGS page of the game's OPTIONS screen: this
 * port's settings drawn by the screen itself, in its font, colours, arrows
 * and EXIT, instead of the toolkit's overlay (which MORE SETTINGS still
 * opens, for the rest).
 *
 * The page's elements are in the screen's layout (options_row.c), off to
 * the right until PC SETTINGS is chosen; then the OPTIONS rows go there and
 * the page's come back (the x of each element object, +0x0C). The front
 * end's frame (sub_000DE940) takes the menu event before anything of the
 * screen sees it while the page is open (wr_pc_page_input, recomp_hooks.c):
 * up and down move between the rows, left and right (or A) change a
 * setting through the toolkit (xbox_SettingSet: live where it can be,
 * remembered for settings.toml), A on MORE SETTINGS opens the overlay, on
 * QUIT GAME asks once more and then quits, on EXIT (or B anywhere) goes back
 * to OPTIONS with the cursor on PC SETTINGS. The screen's own cursor
 * (sub_000A6F40) shows the row: a selected value pulses between its arrows,
 * as the volumes do. Sounds are the screen's: 3 a move or a change, 1
 * confirm, 2 back.
 *
 * The words are drawn by the game's own font call (CXBFont::DrawText,
 * sub_000E1C40 on the font at 0x001A6CA8, Font18.xpr: every Latin-1
 * character; the "REPLAY" and "DEMO" captions are drawn the same way),
 * after the screen's sprites are flushed (sub_0001E0D0 on the batch at
 * 0x00173FC0, wr_pc_page_draw). The layout's text elements stay, empty, as
 * the cursor's places: theirs is a small font for numbers with few letters
 * [verified: run page2, every word but its G, S, E, X and F in boxes].
 */
#include "options_row.h"
#include "settings.h"
#include "pc_page.h"

#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "recomp_types.h"

#if defined(XBOXRECOMP_HAVE_OVERLAY)
#include "xbox_settings.h"
#endif

void recomp_quit_request(const char *why);

extern void sub_000A6F40(void);         /* the cursor's select(item) */
extern void sub_000E1C40(void);         /* CXBFont::DrawText(x, y, colour, char *, flags) */
extern void sub_0001FB00(void);         /* the colour as the game draws it (cdecl) */
extern void sub_000A7680(void);         /* a text element's set(char *) */
extern void sub_000E4310(void);         /* the menu sound player's play */

void wr_menu_sound(uint32_t sound)
{
    PUSH32(g_esp, 0);
    PUSH32(g_esp, 0xFF);
    PUSH32(g_esp, 0);
    PUSH32(g_esp, sound);
    PUSH32(g_esp, 0x000DDE75u);
    g_ecx = 0x001CE150u;
    RECOMP_ABI_CALL(0x000E4310u, sub_000E4310);
}

void wr_cursor_select(uint32_t fe, uint32_t item)
{
    PUSH32(g_esp, item);
    PUSH32(g_esp, 0x000DDE60u);
    g_ecx = MEM32(fe + 0xC);
    RECOMP_ABI_CALL(0x000A6F40u, sub_000A6F40);
}

/* The screen's element objects, by index; the count must be the patched
 * layout's. */
static uint32_t elem(uint32_t fe, int i)
{
    uint32_t scr = MEM32(fe + 0xC);
    return MEM32(MEM32(scr + 8) + 4u * (uint32_t)i);
}

static int elems_ok(uint32_t fe)
{
    uint32_t scr = MEM32(fe + 0xC);
    return scr && wr_options_row_on() && MEM32(MEM32(scr + 4) + 4) == WR_E_COUNT;
}

/* A text element's string: copied onto the guest stack for the setter
 * (thiscall 0x000A7680, the string, ret 4; up to 127 characters). */
static void set_text(uint32_t obj, const char *s)
{
    uint32_t sp0 = g_esp, buf, n = (uint32_t)strlen(s);

    if (n > 127)
        n = 127;
    g_esp -= 160;
    buf = g_esp;
    memcpy((void *)XBOX_PTR(buf), s, n);
    MEM8(buf + n) = 0;
    PUSH32(g_esp, buf);
    PUSH32(g_esp, 0x000DD37Cu);
    g_ecx = obj;
    RECOMP_ABI_CALL(0x000A7680u, sub_000A7680);
    g_esp = sp0;
}

static void move_x(uint32_t fe, int first, int last, float dx)
{
    int i;
    for (i = first; i <= last; i++)
        MEMF(elem(fe, i) + 0xC) = MEMF(elem(fe, i) + 0xC) + dx;
}

static int s_open, s_row, s_quit_armed;
static uint32_t s_screen, s_frames;
static char s_shown[WR_E_COUNT][48];    /* the words of each text place */
static int s_restart[WR_PC_SETTINGS];   /* the row's change waits for a restart */

static void text(uint32_t fe, int i, const char *s)
{
    (void)fe;
    snprintf(s_shown[i], sizeof s_shown[i], "%s", s);
}

static uint32_t f2u(float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

/* The game's DrawText: the string on the guest stack, the colour through
 * its own conversion first, as its callers do (0x00028667). */
static void draw_text(float x, float y, uint32_t colour, const char *s)
{
    uint32_t sp0 = g_esp, buf, c, n = (uint32_t)strlen(s);

    if (!n)
        return;
    if (n > 127)
        n = 127;
    g_esp -= 160;
    buf = g_esp;
    memcpy((void *)XBOX_PTR(buf), s, n);
    MEM8(buf + n) = 0;
    PUSH32(g_esp, colour);
    PUSH32(g_esp, 0x00028678u);
    RECOMP_ABI_CALL(0x0001FB00u, sub_0001FB00);
    g_esp += 4;
    c = g_eax;
    PUSH32(g_esp, 0);                   /* flags: left */
    PUSH32(g_esp, buf);
    PUSH32(g_esp, c);
    PUSH32(g_esp, f2u(y));
    PUSH32(g_esp, f2u(x));
    PUSH32(g_esp, 0x00028690u);
    g_ecx = 0x001A6CA8u;
    RECOMP_ABI_CALL(0x000E1C40u, sub_000E1C40);
    g_esp = sp0;
}

#if defined(XBOXRECOMP_HAVE_OVERLAY)

static const struct { const char *label; int id; } k_rows[WR_PC_SETTINGS] = {
    { "WIDESCREEN",     XS_TITLE_0 + 2 },       /* settings.c's k_title order */
    { "RESOLUTION",     XS_GRAPHICS_RENDER_SCALE },
    { "TEXTURE FILTER", XS_GRAPHICS_ANISO },
    { "ANTI-ALIASING",  XS_GRAPHICS_AA },
    { "BRIGHTER MENUS", XS_TITLE_0 },
    { "TV PICTURE",     XS_TITLE_0 + 1 },
    { "FULLSCREEN",     XS_DISPLAY_FULLSCREEN },
};

static void row_texts(uint32_t fe, int r)
{
    const xs_setting *s = xbox_SettingGet((xs_id)k_rows[r].id);
    char label[48], value[32];
    int v;

    if (!s)
        return;
    v = (int)(s->value + 0.5);
    if (s->type == XS_BOOL)
        snprintf(value, sizeof value, "%s", v ? "ON" : "OFF");
    else if (s->type == XS_CHOICE && s->choices) {
        int n = 0, k;
        while (s->choices[n])
            n++;
        snprintf(value, sizeof value, "%s", v >= 0 && v < n ? s->choices[v] : "?");
        for (k = 0; value[k]; k++)
            if (value[k] >= 'a' && value[k] <= 'z')
                value[k] = (char)(value[k] - 'a' + 'A');
    } else
        snprintf(value, sizeof value, "%dX", v);
    snprintf(label, sizeof label, "%s", k_rows[r].label);
    text(fe, WR_E_PC_LABEL(r), label);
    text(fe, WR_E_PC_VALUE(r), value);
    /* A setting read at start: what it is now shows after a restart, said
     * past the value's right arrow. */
    s_restart[r] = (s->flags & XS_RESTART) && s->value != s->launch;
}

static void change(int r, int dir, int wrap)
{
    const xs_setting *s = xbox_SettingGet((xs_id)k_rows[r].id);
    double v, lo = 0, hi = 1;

    if (!s || s->locked)
        return;
    if (s->type == XS_CHOICE && s->choices) {
        int n = 0;
        while (s->choices[n])
            n++;
        hi = n - 1;
    } else if (s->type == XS_INT) {
        lo = s->min;
        hi = s->max;
    }
    v = (double)(int)(s->value + 0.5) + dir;
    if (v > hi)
        v = wrap ? lo : hi;
    if (v < lo)
        v = wrap ? hi : lo;
    if (v != s->value)
        xbox_SettingSet((xs_id)k_rows[r].id, v);
}

static void flush(void)
{
    xbox_SettingsFlush();
}

#else

static void row_texts(uint32_t fe, int r) { (void)fe; (void)r; }
static void change(int r, int dir, int wrap) { (void)r; (void)dir; (void)wrap; }
static void flush(void) {}

#endif

/* The game's end, as the overlay's Quit game does it (overlay_ui.cpp,
 * quit_now): recomp_quit_request alone only logs on a desktop, its hook
 * being Android's, so the window's own close event follows: pads stopped,
 * the window and audio closed, exit(0) [verified: recomp_restart.c's
 * recomp_quit_request; fb_present_sdl.c's one recomp_quit_set_hook]. */
static void quit_game(void)
{
    SDL_Event q;

    recomp_quit_request("OPTIONS > PC SETTINGS > QUIT GAME");
    memset(&q, 0, sizeof q);
    q.type = SDL_QUIT;
    SDL_PushEvent(&q);
}

static void quit_text(uint32_t fe)
{
    text(fe, WR_E_PC_QUIT, s_quit_armed ? "QUIT GAME? PRESS A AGAIN" : "QUIT GAME");
}

static void select_row(uint32_t fe, int r)
{
    s_row = r;
    if (s_quit_armed && r != WR_PC_ROW_QUIT) {
        s_quit_armed = 0;
        quit_text(fe);
    }
    wr_cursor_select(fe, WR_PC_ITEM(r));
}

int wr_pc_page_open(uint32_t fe)
{
    int r;

    if (!elems_ok(fe))
        return 0;
    if (s_open && s_screen == MEM32(fe + 0xC))
        return 1;
    memset(s_shown, 0, sizeof s_shown);
    s_open = 1;
    s_screen = MEM32(fe + 0xC);
    s_quit_armed = 0;
    move_x(fe, WR_E_OPT_FIRST, WR_E_OPT_LAST, WR_PC_AWAY);
    move_x(fe, WR_E_PC_FIRST, WR_E_COUNT - 1, -WR_PC_AWAY);
    /* The layout's text places empty: the words are drawn (wr_pc_page_draw). */
    for (r = 0; r < WR_PC_SETTINGS; r++) {
        set_text(elem(fe, WR_E_PC_LABEL(r)), "");
        set_text(elem(fe, WR_E_PC_VALUE(r)), "");
    }
    set_text(elem(fe, WR_E_PC_MORE), "");
    set_text(elem(fe, WR_E_PC_QUIT), "");
    for (r = 0; r < WR_PC_SETTINGS; r++)
        row_texts(fe, r);
    text(fe, WR_E_PC_MORE, "MORE SETTINGS");
    quit_text(fe);
    select_row(fe, 0);
    return 1;
}

static void close_page(uint32_t fe)
{
    s_open = 0;
    s_quit_armed = 0;
    move_x(fe, WR_E_PC_FIRST, WR_E_COUNT - 1, WR_PC_AWAY);
    move_x(fe, WR_E_OPT_FIRST, WR_E_OPT_LAST, -WR_PC_AWAY);
    MEM32(fe + 0x70) = WR_OPTIONS_PC_ROW;
    wr_cursor_select(fe, WR_OPTIONS_PC_ITEM);
    flush();
}

int wr_pc_page_is_open(uint32_t fe)
{
    /* A screen built again (OPTIONS left and entered) starts on its rows. */
    if (s_open && (MEM32(fe + 0x20) != 13 || s_screen != MEM32(fe + 0xC) || !elems_ok(fe)))
        s_open = 0;
    return s_open;
}

/* The page's words, over the screen (after its sprites are flushed): the
 * labels green, the values yellow; the cursor's row brighter, its value
 * pulsing as the screen's selected items do. The rows' places are the
 * layout's (options_row.c's PG_*). */
#define PG_Y0     WR_PG_Y0
#define PG_DY     WR_PG_DY
#define PG_VX     WR_PG_VX
#define PG_LX     WR_PG_LX
void wr_pc_page_draw(void)
{
    int r;
    float pulse;

    if (!s_open)
        return;
    s_frames++;
    pulse = 0.75f + 0.25f * (float)((s_frames / 2) % 16) / 15.0f;
    for (r = 0; r < WR_PC_ROWS - 1; r++) {
        float y = (float)(PG_Y0 + PG_DY * r) + 4.0f;
        int sel = r == s_row;
        uint32_t green = sel ? 0xFF80FFA0u : 0xFF40D068u;
        if (r < WR_PC_SETTINGS) {
            uint32_t v = (uint32_t)(255.0f * (sel ? pulse : 0.8f));
            draw_text(PG_LX, y, green, s_shown[WR_E_PC_LABEL(r)]);
            draw_text(PG_VX, y, 0xFF000000u | v << 16 | (uint32_t)(v * 0.85f) << 8 | (uint32_t)(v * 0.2f),
                      s_shown[WR_E_PC_VALUE(r)]);
            if (s_restart[r])
                draw_text(PG_VX + 95, y, 0xFFB0B0B0u, "NEXT START");
        } else {
            int i = r == WR_PC_ROW_MORE ? WR_E_PC_MORE : WR_E_PC_QUIT;
            uint32_t v = (uint32_t)(255.0f * pulse);
            draw_text(PG_LX, y, sel ? 0xFF000000u | v << 16 | (uint32_t)(v * 0.85f) << 8 | (uint32_t)(v * 0.2f)
                                    : green, s_shown[i]);
        }
    }
}

void wr_pc_page_input(uint32_t fe, uint32_t ev)
{
    int r = s_row;

    switch (ev) {
    case 0x0B:                          /* up */
        if (r > 0) {
            select_row(fe, r - 1);
            wr_menu_sound(3);
        }
        break;
    case 0x0C:                          /* down */
        if (r < WR_PC_ROWS - 1) {
            select_row(fe, r + 1);
            wr_menu_sound(3);
        }
        break;
    case 0x0D:                          /* left */
    case 0x0E:                          /* right */
        if (r < WR_PC_SETTINGS) {
            change(r, ev == 0x0E ? 1 : -1, 0);
            row_texts(fe, r);
            wr_menu_sound(3);
        }
        break;
    case 1:                             /* A */
    case 2:
        if (r < WR_PC_SETTINGS) {
            change(r, 1, 1);
            row_texts(fe, r);
            wr_menu_sound(3);
        } else if (r == WR_PC_ROW_MORE) {
            wr_menu_sound(1);
            wr_pc_setting_open();
        } else if (r == WR_PC_ROW_QUIT) {
            wr_menu_sound(1);
            if (!s_quit_armed) {
                s_quit_armed = 1;
                quit_text(fe);
            } else {
                flush();
                quit_game();
            }
        } else {                        /* EXIT */
            wr_menu_sound(1);
            close_page(fe);
        }
        break;
    case 3:                             /* B */
    case 4:
        wr_menu_sound(2);
        close_page(fe);
        break;
    }
}

void wr_pc_page_frame(uint32_t fe)
{
    int r;

    if (!wr_pc_page_is_open(fe))
        return;
    /* The overlay (MORE SETTINGS) may have changed any of them. */
    for (r = 0; r < WR_PC_SETTINGS; r++)
        row_texts(fe, r);
}
