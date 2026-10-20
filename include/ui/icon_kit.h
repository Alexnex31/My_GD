/*
** ALEXNEX PROJECT, 2026
** ui/icon_kit.h
** File description:
** header file for my_gd project
*/

#ifndef UI_ICON_KIT_H
    #define UI_ICON_KIT_H

    #include <stdbool.h>

    #include "sim/settings.h"

    #define ICONS_PER_PAGE 30         /* the Icons screen's grid, 10 x 3       */

/*
** The player's icons (FEATURES 3.8): every .png in res/icons/<mode>/, any
** name, in natural order ("2" before "10"). The settings keep the chosen
** file's name per mode, so adding files never changes a choice. Pure: the
** game layer lists the folders and loads the images in this order.
*/
typedef struct icon_kit {
    char **names[MODE_COUNT];
    int count[MODE_COUNT];
} icon_kit_t;

/* The modes as GD's icon kit lists them: its tabs, left to right. */
extern const gamemode_t ICON_KIT_ORDER[MODE_COUNT];

/*
** In its place among the mode's. Not added: a name that isn't a plain .png
** file (hidden, a folder in it, too long for the settings), or one already
** there.
*/
bool icon_kit_add(icon_kit_t *kit, gamemode_t mode, const char *file);

/* The name's index, -1 if the mode has no such file. */
int icon_kit_find(const icon_kit_t *kit, gamemode_t mode, const char *name);

/* The one the settings chose, else the mode's first; -1 if it has none. */
int icon_kit_chosen(const icon_kit_t *kit, const settings_t *s,
    gamemode_t mode);

/* The settings take the mode's i-th, dirty only if that's a change. */
void icon_kit_choose(const icon_kit_t *kit, settings_t *s, gamemode_t mode,
    int i);

/* How many pages of the grid a mode's icons take: at least one. */
int icon_kit_pages(const icon_kit_t *kit, gamemode_t mode);

void icon_kit_free(icon_kit_t *kit);

#endif
