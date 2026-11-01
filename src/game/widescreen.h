#ifndef WR_WIDESCREEN_H
#define WR_WIDESCREEN_H

/* game.widescreen (RECOMP_WIDESCREEN): read it and register the frame
 * shape with the presenter; before the window opens. */
void wr_widescreen_init(void);
/* The setting changed (the settings overlay). */
void wr_widescreen_set(int on);
int wr_widescreen_on(void);
/* A frame of the menus (1) or of a mission (0), as wr_picture_frame. */
void wr_widescreen_frame(int menu);

#endif
