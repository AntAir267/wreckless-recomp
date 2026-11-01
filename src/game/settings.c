/*
 * settings.c - this title's side of the runtime's settings overlay, which
 * Back + Start and F1 open (Double-S.T.E.A.L 2's settings.c, the same
 * shape).
 *
 * The overlay keeps what the player changes in settings.toml in the save
 * folder. enhance.toml beside the executable stays this build's defaults:
 * its values are the overlay's defaults (Reset goes back to them), and
 * whatever settings.toml sets is put in the environment first, which every
 * reader of a setting, the enhancements layer included, ranks over the file.
 *
 * One row of the title's own: the TV picture (picture.c). The game's OPTIONS
 * screen already has vibration and the three volumes, and it has no video
 * modes to offer (NOTES.md, "Video modes"). Hidden: the aspect ratio (4:3 is all this game
 * draws), the 2D and texture upscalers (no AI upscaling for this one), the
 * overscan crop and the button prompt sets (the game's prompts are its own
 * pictures). The game's OPTIONS screen has a row that opens the overlay,
 * PC SETTINGS (options_row.c), unless interface.menu_entry is off.
 */
#include "settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(XBOXRECOMP_HAVE_OVERLAY)

#include "xbox_settings.h"
#include "platform/recomp_env.h"
#include "enhance_cfg.h"
#include "recomp_cfg.h"
#include "picture.h"
#include "widescreen.h"

static void tv_apply(int v) { wr_picture_set_tv(v); }
static void menu_apply(int v) { wr_picture_set_menu(v); }
static void wide_apply(int v) { wr_widescreen_set(v); }

static const xbox_title_setting k_title[] = {
    { "game.menu_brightness", RENV_MENU_BRIGHTNESS, NULL, NULL, "Brighter menus",
      "The menus a little brighter than the console draws them (their pictures are drawn at"
      " half intensity). Missions and cutscenes are not changed. Changes at once.",
      XS_PAGE_DISPLAY, 0, menu_apply },
    { "game.tv_picture", RENV_TV_PICTURE, NULL, NULL, "TV picture",
      "Brighter midtones, as a capture of the Xbox game on a TV measures. Off (the default)"
      " is the console's own output, as dark as the menus are on an Xbox. Changes at once.",
      XS_PAGE_DISPLAY, 0, tv_apply },
    { "game.widescreen", RENV_WIDESCREEN, NULL, NULL, "Widescreen",
      "Missions, replays and cutscenes in 16:9: the same height of view and more at the sides,"
      " as the game's own split-screen chases are drawn. The menus stay 4:3. Changes at once.",
      XS_PAGE_DISPLAY, 0, wide_apply },
};

/* enhance.toml's keys the overlay has a row for, by the variable it sets. */
static const struct { const char *key; recomp_env_id env; } k_file_keys[] = {
    { "render.scale", RENV_RENDER_SCALE },
    { "graphics.aniso", RENV_GFX_ANISO },
    { "graphics.aa", RENV_GFX_AA },
    { "present.filter", RENV_PRESENT_FILTER },
    { "present.fullscreen", RENV_PRESENT_FULLSCREEN },
    { "game.tv_picture", RENV_TV_PICTURE },
    { "game.menu_brightness", RENV_MENU_BRIGHTNESS },
    { "game.widescreen", RENV_WIDESCREEN },
};

void wr_settings_init(const char *save_dir)
{
    xbox_SettingsAddTitle(k_title, (int)(sizeof k_title / sizeof k_title[0]));
    xbox_SettingsHide(XS_DISPLAY_ASPECT);
    xbox_SettingsHide(XS_GRAPHICS_UI_SCALE);
    xbox_SettingsHide(XS_GRAPHICS_TEXUP);
    xbox_SettingsHide(XS_GRAPHICS_TEXUP_SCALE);
    xbox_SettingsHide(XS_GRAPHICS_TEXUP_ROADS);
    xbox_SettingsHide(XS_GRAPHICS_TEXUP_PRELOAD);
    xbox_SettingsHide(XS_DISPLAY_OVERSCAN);
    xbox_SettingsHide(XS_INPUT_GLYPHS);
    /* settings.toml beside the saves (--save-dir), where recomp_save_dir
     * looks first. */
    setenv("RECOMP_SAVE_DIR", save_dir, 0);
    xbox_SettingsInit("wreckless");
}

void wr_settings_default(const char *name, const char *value)
{
    xbox_SettingsGameDefault(name, value);
}

void wr_settings_finish(void)
{
    size_t i;

    for (i = 0; i < sizeof k_file_keys / sizeof k_file_keys[0]; i++) {
        const char *key = k_file_keys[i].key;
        const recomp_cfg *c = enhance_cfg_lookup(key);
        char v[64];
        if (!c)
            continue;
        switch (recomp_cfg_type_of(c, key)) {
        case RCFG_STRING: snprintf(v, sizeof v, "%s", recomp_cfg_string(c, key, "")); break;
        case RCFG_INT:    snprintf(v, sizeof v, "%lld", recomp_cfg_int(c, key, 0)); break;
        case RCFG_FLOAT:  snprintf(v, sizeof v, "%g", recomp_cfg_float(c, key, 0)); break;
        case RCFG_BOOL:   snprintf(v, sizeof v, "%d", recomp_cfg_bool(c, key, 0)); break;
        default:          continue;
        }
        /* As an env_default: the variable, under what the environment and
         * settings.toml set, and the overlay told. */
        setenv(recomp_env_name(k_file_keys[i].env), v, 0);
        xbox_SettingsGameDefault(recomp_env_name(k_file_keys[i].env), v);
    }
    xbox_SettingsFinish();
    xbox_OverlaySetAbout("Wreckless: The Yakuza Missions (Xbox, 2002), recompiled."
                         " Settings: OPTIONS > PC SETTINGS, Back + Start or F1.");
}

/* RECOMP_MENU_ENTRY=force: the row without an overlay (a headless check of
 * the screen; A on it then only says so). */
int wr_pc_setting_row(void)
{
    const char *e = recomp_env(RENV_MENU_ENTRY);
    if (e && !strcmp(e, "force"))
        return 1;
    return xbox_OverlayAvailable() && !(e && e[0] == '0');
}

void wr_pc_setting_open(void)
{
    if (!xbox_OverlayRequestOpen())
        fprintf(stderr, "[WR] PC SETTINGS: no overlay to open (headless or RECOMP_OVERLAY=0)\n");
}

#else

void wr_settings_init(const char *save_dir) { (void)save_dir; }
void wr_settings_default(const char *name, const char *value) { (void)name; (void)value; }
void wr_settings_finish(void) {}
int wr_pc_setting_row(void) { return 0; }
void wr_pc_setting_open(void) {}

#endif
