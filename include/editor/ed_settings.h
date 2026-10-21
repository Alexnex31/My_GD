/*
** ALEXNEX PROJECT, 2026
** editor/ed_settings.h
** File description:
** header file for my_gd project
*/

#ifndef EDITOR_ED_SETTINGS_H
    #define EDITOR_ED_SETTINGS_H

    #include "editor/ed_history.h"
    #include "editor/ed_music.h"

/*
** The level's settings window (FEATURES 11.16), as GD's: what the level's
** header says, a row each. Fewer than GD's: no colour channels, no
** backgrounds, nothing this game doesn't do. Pure, tested alone.
*/
typedef enum ed_setting {
    ED_SET_SONG,              /* the song rows: ed_song_prop_t's, in order    */
    ED_SET_OFFSET,
    ED_SET_BPM,
    ED_SET_FIRST_BEAT,
    ED_SET_MODE,              /* the start: start_gamemode, round the modes   */
    ED_SET_SPEED,             /* start_speed, x0.5 to x4, stopping at the ends */
    ED_SET_SIZE,              /* start_size: normal or mini                   */
    ED_SET_GRAVITY,           /* start_gravity: normal or flipped             */
    ED_SET_COUNT
} ed_setting_t;

    #define ED_SET_FIRST_START ED_SET_MODE   /* where the Start group begins */

const char *ed_setting_name(ed_setting_t s);
void ed_setting_text(const ed_level_t *lv, const library_t *lib,
    ed_setting_t s, char *out, size_t size);
/*
** The next (dir 1) or previous (-1) value; `big` is the song rows' bigger
** step. A change is an edit: dirty, and a step of the undo history. false
** when nothing changed.
*/
bool ed_setting_step(ed_level_t *lv, ed_history_t *h, const library_t *lib,
    ed_setting_t s, int dir, bool big);

#endif
