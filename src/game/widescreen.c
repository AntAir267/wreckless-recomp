/*
 * widescreen.c - game.widescreen: missions, replays and cutscenes in 16:9.
 *
 * The game has no widescreen mode (NOTES.md, "Video modes"), but its camera
 * already knows how to see a 16:9 picture: the chase scenes' split view
 * (view mode 1 at 0x0016D860: two panels two thirds wide and half high, so
 * 16:9 each) gets the camera's aspect pair (0.5625, 4/3) from
 * sub_000213C0, where the one full view (mode 0) gets (0.75, 1)
 * [verified: disassembly of sub_000213C0 and sub_00021400]. With the
 * setting on, the full view gets the panels' pair too: the frame is drawn
 * as the Xbox did widescreen, 16:9 squeezed into the same 640x480 buffer,
 * the same height of view and more of it at the sides, and the window shows
 * those frames 16:9 (nv2a_present_frame_aspect_fn). The menus stay 4:3
 * between bars. A frame is a mission's while the mission loop runs (the
 * hooks of picture.c's wr_picture_frame).
 *
 * The HUD is drawn in the same 640x480 frame, so it would be stretched a
 * third wider with it. Every piece of it (the timer, the speed meter, the
 * map, the car icons, messages, the pause menu) is drawn by one vertex
 * program, its x a fraction of the screen's width, in a handful of draws a
 * frame [verified: RECOMP_TRACE=vk_draws of an A-1 frame, run wide2: the
 * map at 0.71875..0.94375, 460..604 px, DispPos.txt's place for it]. The
 * presenter narrows each of those draws by 3/4 about its own middle
 * (nv2a_hud_squeeze): every element where it was, in its own shape.
 */
#include "widescreen.h"

#include <stdint.h>
#include <stdlib.h>

#include "env/recomp_env_game.h"
#include "platform/recomp_env.h"

void nv2a_present_frame_aspect_fn(double (*fn)(uint32_t flip));
void nv2a_present_window_aspect(double aspect);
void nv2a_hud_squeeze(uint32_t vs_hash, float k);

/* The HUD's vertex program (nv2a_vsh_program_hash). */
#define HUD_PROGRAM 0x0F208BABu

static volatile int s_on, s_mission;

static void hud_update(void)
{
    nv2a_hud_squeeze(HUD_PROGRAM, s_on && s_mission ? 0.75f : 1.0f);
}

static double frame_aspect(uint32_t flip)
{
    (void)flip;
    return s_on && s_mission ? 16.0 / 9.0 : 0.0;
}

void wr_widescreen_init(void)
{
    const char *e = recomp_env(RENV_WIDESCREEN);

    s_on = e && e[0] == '1';
    nv2a_present_frame_aspect_fn(frame_aspect);
    if (s_on)
        nv2a_present_window_aspect(16.0 / 9.0);
    hud_update();
}

void wr_widescreen_set(int on)
{
    s_on = on != 0;
    hud_update();
}

int wr_widescreen_on(void)
{
    return s_on;
}

void wr_widescreen_frame(int menu)
{
    if (s_mission != !menu) {
        s_mission = !menu;
        hud_update();
    }
}
