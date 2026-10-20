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
#define DRAWN_SPEED "#speed"
#define SAW_TEETH 10
#define DRAWN_SIZE 64

static const char *const ATLAS_FILES[OBJ_TYPE_COUNT] = {
    [OBJ_BLOCK] = ASSET_BLOCK,
    [OBJ_SLOPE] = ASSET_BLOCK,
    [OBJ_SPIKE] = ASSET_SPIKE,
    [OBJ_PORTAL] = ASSET_PORTAL,
    [OBJ_GRAVITY] = ASSET_PORTAL,
    [OBJ_PAD] = DRAWN_PLATE,                 /* no image yet: drawn here in */
    [OBJ_ORB] = DRAWN_ORB,                   /* white, tinted by its colour */
    [OBJ_SAW] = DRAWN_SAW,
    [OBJ_SPEED] = DRAWN_SPEED,               /* white, tinted by its speed */
    [OBJ_MINI] = ASSET_PORTAL,
};

/* The pixel at (x, y) of the padded image: the nearest of the image's own. */
static sfColor padded_pixel(const sfImage *src, sfVector2u s, int x, int y)
{
    x = x < 0 ? 0 : (x >= (int)s.x ? (int)s.x - 1 : x);
    y = y < 0 ? 0 : (y >= (int)s.y ? (int)s.y - 1 : y);
    return sfImage_getPixel(src, (unsigned int)x, (unsigned int)y);
}

/*
** The image with its own edge pixels repeated into the padding, corners
** included, so a texture coordinate rounded outward still lands on the
** right colour. White: the same shape, every pixel white, for the flash.
*/
static void copy_with_padding(sfImage *dst, const sfImage *src, sfVector2u at,
    bool white)
{
    sfVector2u s = sfImage_getSize(src);

    for (int y = 0; y < (int)s.y + 2 * PAD; y++)
        for (int x = 0; x < (int)s.x + 2 * PAD; x++) {
            sfColor c = padded_pixel(src, s, x - PAD, y - PAD);

            if (white)
                c = (sfColor){255, 255, 255, c.a};
            sfImage_setPixel(dst, at.x + (unsigned int)x,
                at.y + (unsigned int)y, c);
        }
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
** A speed portal's stand-in: three arrowheads pointing right, one above the
** other, white so the portal's colour is its tint.
*/
static sfImage *drawn_speed(void)
{
    sfImage *im = sfImage_createFromColor(DRAWN_SIZE, DRAWN_SIZE,
        sfTransparent);

    for (unsigned int y = 0; y < DRAWN_SIZE; y++)
        for (unsigned int x = 0; x < DRAWN_SIZE; x++) {
            float u = (float)x / (DRAWN_SIZE - 1);
            float v = fmodf((float)y / DRAWN_SIZE * 3.0f, 1.0f);
            float edge = 0.75f - fabsf(v - 0.5f) * 1.2f;   /* the arrow's tip */

            if (u < edge && u > edge - 0.38f)
                sfImage_setPixel(im, x, y, sfWhite);
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
    if (strcmp(name, DRAWN_SPEED) == 0)
        return drawn_speed();
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

/* Side by side on the first row; the second row is the same, white. */
static void atlas_place(gd_t *gd, sfImage *atlas, sfImage **parts,
    unsigned int *widths)
{
    unsigned int x = 0;
    unsigned int row = sfImage_getSize(atlas).y / 2;

    for (int t = 0; t < OBJ_TYPE_COUNT; t++) {
        sfVector2u s;

        if (parts[t] == NULL)
            continue;
        s = sfImage_getSize(parts[t]);
        copy_with_padding(atlas, parts[t], (sfVector2u){x, 0}, false);
        copy_with_padding(atlas, parts[t], (sfVector2u){x, row}, true);
        gd->atlas_rect[t] = (sfFloatRect){(float)(x + PAD), (float)PAD,
            (float)s.x, (float)s.y};
        x += widths[t];
    }
    gd->atlas_white_dy = (float)row;
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
** Under them, their white silhouettes: what flashes when one fires (9.2).
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
    atlas = sfImage_createFromColor(total, 2 * height, sfTransparent);
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
