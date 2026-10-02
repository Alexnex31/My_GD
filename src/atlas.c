/*
** ALEXNEX PROJECT, 2026
** atlas
** File description:
** every object image in one texture, so they share a draw call (9.2)
*/

#include "mygd.h"

#define PAD 2                     /* border pixels, so a rounded sample can't
                                     reach into the neighbouring image (9.1) */

static const char *const ATLAS_FILES[OBJ_TYPE_COUNT] = {
    [OBJ_BLOCK] = "res/block.png",
    [OBJ_SLOPE] = "res/block.png",
    [OBJ_SPIKE] = "res/spike.png",
    [OBJ_PORTAL] = "res/cube_portal.png",
};

/*
** Repeat the image's own edge rows and columns into the padding, so a texture
** coordinate rounded outward still lands on the right colour.
*/
static void copy_with_padding(sfImage *dst, const sfImage *src, unsigned int x,
    unsigned int y)
{
    sfVector2u s = sfImage_getSize((sfImage *)src);

    sfImage_copyImage(dst, (sfImage *)src, x + PAD, y + PAD,
        (sfIntRect){0, 0, (int)s.x, (int)s.y}, sfFalse);
    sfImage_copyImage(dst, (sfImage *)src, x, y + PAD,
        (sfIntRect){0, 0, PAD, (int)s.y}, sfFalse);
    sfImage_copyImage(dst, (sfImage *)src, x + PAD + s.x, y + PAD,
        (sfIntRect){(int)s.x - PAD, 0, PAD, (int)s.y}, sfFalse);
    sfImage_copyImage(dst, (sfImage *)src, x + PAD, y,
        (sfIntRect){0, 0, (int)s.x, PAD}, sfFalse);
    sfImage_copyImage(dst, (sfImage *)src, x + PAD, y + PAD + s.y,
        (sfIntRect){0, (int)s.y - PAD, (int)s.x, PAD}, sfFalse);
}

static sfImage *load_image(const char *path)
{
    sfImage *im = sfImage_createFromFile(path);

    if (im == NULL) {
        dprintf(2, "my_gd: missing asset %s\n", path);
        exit(84);
    }
    return im;
}

static void atlas_place(gd_t *gd, sfImage *atlas, sfImage **parts,
    unsigned int *widths)
{
    unsigned int x = 0;

    for (int t = 0; t < OBJ_TYPE_COUNT; t++) {
        sfVector2u s;

        if (parts[t] == NULL)
            continue;
        s = sfImage_getSize(parts[t]);
        copy_with_padding(atlas, parts[t], x, 0);
        gd->atlas_rect[t] = (sfFloatRect){(float)(x + PAD), (float)PAD,
            (float)s.x, (float)s.y};
        x += widths[t];
    }
}

/* The earlier type drawn from the same file, or -1: it is loaded once. */
static int same_image_as(int t)
{
    for (int u = 0; u < t; u++)
        if (ATLAS_FILES[u] != NULL
            && strcmp(ATLAS_FILES[u], ATLAS_FILES[t]) == 0)
            return u;
    return -1;
}

/*
** One texture for every object type, built once at startup. Types that share
** an image (a slope is drawn with the block texture) share its rectangle.
*/
void atlas_build(gd_t *gd)
{
    sfImage *parts[OBJ_TYPE_COUNT] = {NULL};
    unsigned int widths[OBJ_TYPE_COUNT] = {0};
    unsigned int total = 0;
    unsigned int height = 0;
    sfImage *atlas = NULL;

    for (int t = 0; t < OBJ_TYPE_COUNT; t++) {
        sfVector2u s;

        if (ATLAS_FILES[t] == NULL || same_image_as(t) >= 0)
            continue;
        parts[t] = load_image(ATLAS_FILES[t]);
        s = sfImage_getSize(parts[t]);
        widths[t] = s.x + 2 * PAD;
        total += widths[t];
        height = s.y + 2 * PAD > height ? s.y + 2 * PAD : height;
    }
    atlas = sfImage_createFromColor(total, height, sfTransparent);
    atlas_place(gd, atlas, parts, widths);
    for (int t = 0; t < OBJ_TYPE_COUNT; t++)
        if (ATLAS_FILES[t] != NULL && same_image_as(t) >= 0)
            gd->atlas_rect[t] = gd->atlas_rect[same_image_as(t)];
    gd->atlas = sfTexture_createFromImage(atlas, NULL);
    sfTexture_setSmooth(gd->atlas, sfFalse);
    for (int t = 0; t < OBJ_TYPE_COUNT; t++)
        if (parts[t] != NULL)
            sfImage_destroy(parts[t]);
    sfImage_destroy(atlas);
}
