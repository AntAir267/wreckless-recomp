/*
 * picture.c - the TV picture: the game's output as a television showed it.
 *
 * The game's 2D is a texture times a vertex colour of 0x7F under a plain
 * MODULATE, and its 3D is exposed for the same range, so its frames are
 * dark on a PC monitor (a play report: "the menus are too dark"). A capture of the
 * Xbox game (World of Longplays' longplay, YouTube qVFlzdsHdLM) is brighter
 * in the midtones by a curve: this build's frames with each value taken to
 * the power 0.8 match it within a few levels on the title (logo 70/58/27
 * against 69/51/30, background 96/51 against 94/46), the car select
 * (G 50 -> 69 against 67, B 82 -> 102 against 102) and the street at
 * mission A-1's READY? (road 100/99/88 against 100/89/88, wall 90/92/81
 * against 98/88/82) [verified: per-region means, 2026-10-26, NOTES.md
 * "The picture"]. The blacks of the capture sit a little higher (its TV
 * path); that part is not copied.
 *
 * So the curve goes on top of whatever ramp the game loads (only the
 * identity, at start), through the runtime's gamma pass (xbox_gamma.h),
 * which the window and the frame dumps both apply. Off by default: a real
 * Xbox was checked and its menus are that dark too, so the console's
 * output as it is stays the default; RECOMP_TV_PICTURE=1 (game.tv_picture
 * = true, the settings row) turns the curve on.
 */
#include "picture.h"

#include <math.h>
#include <string.h>

#include "xbox_gamma.h"
#include "platform/recomp_env.h"

#define WR_TV_GAMMA   0.8
/* The menus only: a player asked for them a little brighter than the console's
 * (2026-10-26). Their 2D is drawn at half intensity (vertex colour 0x7F);
 * 0.7 lifts the midtones by about 1.4x and leaves white white. */
#define WR_MENU_GAMMA 0.7

static uint8_t s_game[768];
static int s_have;
static int s_tv = -1;
static int s_menu_on = -1;          /* the setting */
static int s_in_menu;               /* the frames now are the menus' */

static int tv_on(void)
{
    if (s_tv < 0) {
        const char *e = recomp_env(RENV_TV_PICTURE);
        s_tv = e && e[0] == '1';
    }
    return s_tv;
}

static int menu_on(void)
{
    if (s_menu_on < 0) {
        const char *e = recomp_env(RENV_MENU_BRIGHTNESS);
        s_menu_on = !(e && e[0] == '0');
    }
    return s_menu_on;
}

static void push(void)
{
    static uint8_t lut_tv[256], lut_menu[256];
    static int made;
    const uint8_t *lut = NULL;
    uint8_t out[768];
    int i;

    if (!made) {
        for (i = 0; i < 256; i++) {
            lut_tv[i] = (uint8_t)lround(255.0 * pow(i / 255.0, WR_TV_GAMMA));
            lut_menu[i] = (uint8_t)lround(255.0 * pow(i / 255.0, WR_MENU_GAMMA));
        }
        made = 1;
    }
    if (s_in_menu && menu_on())
        lut = lut_menu;
    else if (tv_on())
        lut = lut_tv;
    for (i = 0; i < 768; i++)
        out[i] = lut ? lut[s_game[i]] : s_game[i];
    xbox_GammaSetRamp(out);
}

void wr_picture_frame(int menu)
{
    menu = menu != 0;
    if (menu == s_in_menu)
        return;
    s_in_menu = menu;
    if (s_have)
        push();
}

void wr_picture_set_menu(int on)
{
    s_menu_on = on ? 1 : 0;
    if (s_have)
        push();
}

void wr_picture_game_ramp(const uint8_t rgb[768])
{
    memcpy(s_game, rgb, 768);
    s_have = 1;
    push();
}

void wr_picture_set_tv(int on)
{
    s_tv = on ? 1 : 0;
    if (s_have)
        push();
}
