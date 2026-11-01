/*
 * settings.h - this title's side of the runtime's settings overlay
 * (settings.c). Without the overlay (XBOXRECOMP_OVERLAY off) these do
 * nothing.
 */
#ifndef WR_SETTINGS_H
#define WR_SETTINGS_H

/* Before the title's runtime defaults: the rows it hides, settings.toml in
 * `save_dir` read and applied. */
void wr_settings_init(const char *save_dir);
/* A runtime default of the title's (main.c's env_default): also the
 * setting's default in the overlay. */
void wr_settings_default(const char *name, const char *value);
/* After the enhancements layer read enhance.toml: its values become the
 * defaults, and the settings take what the environment holds. */
void wr_settings_finish(void);

/* Whether the game's OPTIONS screen gets the PC SETTINGS row (the overlay
 * built in, interface.menu_entry not off), and opening the overlay from it. */
int wr_pc_setting_row(void);
void wr_pc_setting_open(void);

#endif
