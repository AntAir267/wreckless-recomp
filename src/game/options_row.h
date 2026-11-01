#ifndef WR_OPTIONS_ROW_H
#define WR_OPTIONS_ROW_H

#include <stdint.h>

/* The row's item (the cursor's index in the screen's layout: the rows'
 * own are 10 per row, EXIT 60) and its row in the screen's code (the rows
 * are 0 to 5 and EXIT 6; recomp_hooks.c, sub_000DDDC0). */
#define WR_OPTIONS_PC_ITEM 70
#define WR_OPTIONS_PC_ROW  7

/* The PC SETTINGS page (pc_page.c): elements of the same layout, off the
 * screen (x + WR_PC_AWAY) until the page is open, when the OPTIONS rows go
 * there instead. Rows 0-6 a setting each (a label, its value and the value's
 * two arrows), 7 MORE SETTINGS, 8 QUIT GAME, 9 EXIT. Items 100 + 10 x row. */
#define WR_PC_AWAY        4096.0f
#define WR_PC_SETTINGS    7
#define WR_PC_ROW_MORE    7
#define WR_PC_ROW_QUIT    8
#define WR_PC_ROW_EXIT    9
#define WR_PC_ROWS        10
#define WR_PC_ITEM(r)     (100 + 10 * (r))
#define WR_PG_Y0          112                 /* row 0's y, then a row every WR_PG_DY */
#define WR_PG_DY          27
#define WR_PG_LX          130                 /* the labels' x, as OPTIONS' */
#define WR_PG_VX          380                 /* the values'; their arrows 20 before, 70 after */
#define WR_E_OPT_FIRST    2                   /* the OPTIONS rows to PcSetting */
#define WR_E_OPT_LAST     24
#define WR_E_PC_FIRST     25
#define WR_E_PC_LABEL(r)  (WR_E_PC_FIRST + 4 * (r))       /* r 0-6 */
#define WR_E_PC_VALUE(r)  (WR_E_PC_FIRST + 4 * (r) + 1)
#define WR_E_PC_MORE      (WR_E_PC_FIRST + 4 * WR_PC_SETTINGS)
#define WR_E_PC_QUIT      (WR_E_PC_MORE + 1)
#define WR_E_PC_EXIT      (WR_E_PC_MORE + 2)
#define WR_E_COUNT        (WR_E_PC_EXIT + 1)

/* The layout at L (sub_000A68A0's argument, not yet relocated): for the
 * OPTIONS screen's, the address of a copy with the PC SETTINGS row added,
 * and its label drawn; else 0 and nothing changed. */
uint32_t wr_options_row_patch(uint32_t L);

/* 1 while the screen last loaded has the row. */
int wr_options_row_on(void);

#endif
