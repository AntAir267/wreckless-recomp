/*
 * picture.h - the TV picture (picture.c): the game's output as a
 * television showed it, through the display's gamma ramp.
 */
#ifndef WR_PICTURE_H
#define WR_PICTURE_H

#include <stdint.h>

/* The ramp the game loads into the DAC (the hook on D3D's writer). */
void wr_picture_game_ramp(const uint8_t rgb[768]);
/* The setting changed (the settings overlay): 1 the TV picture, 0 the
 * console's output as it is. */
void wr_picture_set_tv(int on);
/* A frame of the menus (1: the menu loop, sub_000DF100) or of a mission,
 * demo or cutscene (0: sub_000DB330); the menus get their own curve. */
void wr_picture_frame(int menu);
/* The menu brightness setting changed. */
void wr_picture_set_menu(int on);

#endif
