/*
** ALEXNEX PROJECT, 2026
** editor/ed_autosave.h
** File description:
** header file for my_gd project
*/

#ifndef EDITOR_ED_AUTOSAVE_H
    #define EDITOR_ED_AUTOSAVE_H

    #include <time.h>

    #include "editor/ed_level.h"

    #define ED_AUTOSAVE_DIR "save/editor_autosave"
    #define ED_AUTOSAVE_EVERY_MS 60000

/*
** The level being edited, written beside the real file and never over it
** (FEATURES 11.12): <dir>/<id>.gd. A crash loses a minute at most. No
** window in it; the tests give it a folder of their own.
*/
void ed_autosave_path(const char *dir, const char *id, char *out, size_t size);

/* Makes the folder if it has to. The level isn't any less changed for it. */
int ed_autosave_write(const ed_level_t *lv, const char *dir);

/*
** There is one, and it is newer than the level's file, or the level has no
** file at all (a new level that was never saved). `when`: its date.
*/
bool ed_autosave_pending(const char *dir, const char *id,
    const char *level_path, time_t *when);

/* The level becomes what the autosave holds, as unsaved changes. */
int ed_autosave_restore(ed_level_t *lv, const char *dir);

void ed_autosave_remove(const char *dir, const char *id);

#endif
