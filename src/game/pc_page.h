#ifndef WR_PC_PAGE_H
#define WR_PC_PAGE_H

#include <stdint.h>

/* The PC SETTINGS page of the OPTIONS screen (pc_page.c); fe is the front
 * end (the frame's ecx). Open: 0 when the screen has no page (its layout
 * was not patched). Input: one menu event while the page is open. Frame:
 * after the front end's frame, the values brought up to date. */
int  wr_pc_page_open(uint32_t fe);
int  wr_pc_page_is_open(uint32_t fe);
void wr_pc_page_input(uint32_t fe, uint32_t ev);
void wr_pc_page_frame(uint32_t fe);
/* The page's words over the screen; after its sprites are flushed. */
void wr_pc_page_draw(void);

/* The screen's own sound (3 a move, 1 confirm, 2 back) and cursor. */
void wr_menu_sound(uint32_t sound);
void wr_cursor_select(uint32_t fe, uint32_t item);

#endif
