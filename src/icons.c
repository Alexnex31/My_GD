/*
** ALEXNEX PROJECT, 2026
** icons
** File description:
** the player's icons: each mode's folder in res/icons/, loaded once, and the one chosen (FEATURES 3.8)
*/

#include "mygd.h"
#include "sim/modes.h"
#include "ui/icon_kit.h"

/* The folder's names, in the kit's order: what the images load in. */
static void list_folder(icon_kit_t *found, gamemode_t mode, const char *dir)
{
    DIR *d = opendir(dir);

    if (d == NULL)
        return;
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d))
        icon_kit_add(found, mode, e->d_name);
    closedir(d);
}

/*
** A file that isn't an image is left out, with a warning; a mode with none
** at all is a missing asset, as a missing background is.
*/
static void load_mode(gd_t *gd, gamemode_t mode)
{
    icon_kit_t found = {0};
    char dir[64];
    char path[sizeof(dir) + SETTING_FILE_MAX];
    sfTexture *tex = NULL;

    snprintf(dir, sizeof(dir), "%s/%s", ASSET_ICONS, MODES[mode].name);
    list_folder(&found, mode, dir);
    gd->icon_tex[mode] = sim_xcalloc((size_t)found.count[mode] + 1,
        sizeof(sfTexture *));
    for (int i = 0; i < found.count[mode]; i++) {
        snprintf(path, sizeof(path), "%s/%s", dir, found.names[mode][i]);
        tex = sfTexture_createFromFile(path, NULL);
        if (tex == NULL) {
            dprintf(2, "my_gd: %s isn't an image, left out\n", path);
            continue;
        }
        gd->icon_tex[mode][gd->icons.count[mode]] = tex;
        icon_kit_add(&gd->icons, mode, found.names[mode][i]);
    }
    icon_kit_free(&found);
    if (gd->icons.count[mode] == 0) {
        dprintf(2, "my_gd: missing asset: no icon in %s/\n", dir);
        exit(84);
    }
}

void icons_load(gd_t *gd)
{
    for (int m = 0; m < MODE_COUNT; m++)
        load_mode(gd, (gamemode_t)m);
}

void icons_free(gd_t *gd)
{
    for (int m = 0; m < MODE_COUNT; m++) {
        for (int i = 0; i < gd->icons.count[m]; i++)
            sfTexture_destroy(gd->icon_tex[m][i]);
        free(gd->icon_tex[m]);
        gd->icon_tex[m] = NULL;
    }
    icon_kit_free(&gd->icons);
}

/* The settings' choice, else the mode's first (the file may have gone). */
sfTexture *icon_texture(gd_t *gd, gamemode_t mode)
{
    return gd->icon_tex[mode][icon_kit_chosen(&gd->icons, &gd->settings,
        mode)];
}
