/*
** ALEXNEX PROJECT, 2026
** atlas
** File description:
** every object image in one texture, so they share a draw call (9.2)
*/

#include "mygd.h"

#define PAD 2                     /* border pixels, so a rounded sample can't
                                     reach into the neighbouring image (9.1) */

#define DRAWN_PLATE "#plate"      /* not files: shapes made in drawn_image */
#define DRAWN_ORB "#orb"
#define DRAWN_SAW "#saw"
#define SAW_TEETH 10
#define DRAWN_SIZE 64

static const char *const ATLAS_FILES[OBJ_TYPE_COUNT] = {
    [OBJ_BLOCK] = "res/block.png",
    [OBJ_SLOPE] = "res/block.png",
    [OBJ_SPIKE] = "res/spike.png",
    [OBJ_PORTAL] = "res/cube_portal.png",
    [OBJ_GRAVITY] = "res/cube_portal.png",
    [OBJ_PAD] = DRAWN_PLATE,                 /* no image yet: drawn here in */
    [OBJ_ORB] = DRAWN_ORB,                   /* white, tinted by its colour */
    [OBJ_SAW] = DRAWN_SAW,
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

/*
** A saw's stand-in: a grey disc as wide as its hitbox (SAW_RADIUS of the
** image's width), teeth from there to the image's edge, a dark hub. The
** teeth lean, so its spin can be seen.
*/
static sfImage *drawn_saw(void)
{
    sfImage *im = sfImage_createFromColor(DRAWN_SIZE, DRAWN_SIZE,
        sfTransparent);
    float mid = (DRAWN_SIZE - 1) / 2.0f;
    float body = 2.0f * (float)SAW_RADIUS;

    for (unsigned int y = 0; y < DRAWN_SIZE; y++)
        for (unsigned int x = 0; x < DRAWN_SIZE; x++) {
            float r = hypotf((float)x - mid, (float)y - mid) / mid;
            float turn = atan2f((float)y - mid, (float)x - mid)
                * SAW_TEETH / (2.0f * (float)M_PI);
            float tooth = body + (1.0f - body) * (1.0f - (turn - floorf(turn)));

            if (r < 0.22f)
                sfImage_setPixel(im, x, y, (sfColor){40, 44, 60, 255});
            else if (r < body)
                sfImage_setPixel(im, x, y, (sfColor){200, 205, 215, 255});
            else if (r < tooth)
                sfImage_setPixel(im, x, y, (sfColor){150, 155, 170, 255});
        }
    return im;
}

/*
** The stand-ins for objects that have no image yet, white so the object's
** colour is exactly its tint: a plain plate, and a ring around a dot.
*/
static sfImage *drawn_image(const char *name)
{
    sfImage *im = NULL;
    float mid = (DRAWN_SIZE - 1) / 2.0f;

    if (strcmp(name, DRAWN_SAW) == 0)
        return drawn_saw();
    im = sfImage_createFromColor(DRAWN_SIZE, DRAWN_SIZE, sfWhite);
    if (strcmp(name, DRAWN_PLATE) == 0)
        return im;
    for (unsigned int y = 0; y < DRAWN_SIZE; y++)
        for (unsigned int x = 0; x < DRAWN_SIZE; x++) {
            float r = hypotf((float)x - mid, (float)y - mid) / mid;

            if (r > 1.0f || (r > 0.45f && r < 0.72f))
                sfImage_setPixel(im, x, y, sfTransparent);
        }
    return im;
}

static sfImage *load_image(const char *path)
{
    sfImage *im = path[0] == '#' ? drawn_image(path)
        : sfImage_createFromFile(path);

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
