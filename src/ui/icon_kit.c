/*
** ALEXNEX PROJECT, 2026
** ui/icon_kit.c
** File description:
** the player's icons, per gamemode: which files, in what order, which one is chosen (FEATURES 3.8)
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/alloc.h"
#include "ui/icon_kit.h"

const gamemode_t ICON_KIT_ORDER[MODE_COUNT] = {MODE_CUBE, MODE_SHIP,
    MODE_BALL, MODE_UFO, MODE_WAVE, MODE_ROBOT, MODE_SPIDER, MODE_SWING};

static bool is_png(const char *file)
{
    size_t n = strlen(file);

    if (n < 5 || n >= SETTING_FILE_MAX || file[0] == '.')
        return false;
    for (size_t i = 0; i < n; i++)
        if (file[i] == '/' || file[i] == '\\'
            || !isprint((unsigned char)file[i]))
            return false;
    for (size_t i = 0; i < 4; i++)
        if (tolower((unsigned char)file[n - 4 + i]) != ".png"[i])
            return false;
    return true;
}

/* Runs of digits by their value, the rest byte by byte: "2" < "10". */
static int natural_cmp(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
            size_t na = 0;
            size_t nb = 0;
            int c = 0;

            while (*a == '0' && isdigit((unsigned char)a[1]))
                a++;
            while (*b == '0' && isdigit((unsigned char)b[1]))
                b++;
            while (isdigit((unsigned char)a[na]))
                na++;
            while (isdigit((unsigned char)b[nb]))
                nb++;
            if (na != nb)
                return na < nb ? -1 : 1;
            c = strncmp(a, b, na);
            if (c != 0)
                return c;
            a += na;
            b += nb;
            continue;
        }
        if (*a != *b)
            return (unsigned char)*a < (unsigned char)*b ? -1 : 1;
        a++;
        b++;
    }
    if (*a == *b)
        return 0;
    return *a == '\0' ? -1 : 1;
}

/* "a01" and "a1" are alike: their bytes decide, whatever the folder's order. */
static int icon_order(const char *a, const char *b)
{
    int c = natural_cmp(a, b);

    return c != 0 ? c : strcmp(a, b);
}

bool icon_kit_add(icon_kit_t *kit, gamemode_t mode, const char *file)
{
    int n = kit->count[mode];
    int at = 0;
    char **grown = NULL;

    if (!is_png(file) || icon_kit_find(kit, mode, file) >= 0)
        return false;
    while (at < n && icon_order(kit->names[mode][at], file) <= 0)
        at++;
    grown = sim_xcalloc((size_t)n + 1, sizeof(char *));
    if (n > 0) {
        memcpy(grown, kit->names[mode], (size_t)at * sizeof(char *));
        memcpy(&grown[at + 1], &kit->names[mode][at],
            (size_t)(n - at) * sizeof(char *));
    }
    grown[at] = sim_xstrdup(file);
    free(kit->names[mode]);
    kit->names[mode] = grown;
    kit->count[mode] = n + 1;
    return true;
}

int icon_kit_find(const icon_kit_t *kit, gamemode_t mode, const char *name)
{
    for (int i = 0; i < kit->count[mode]; i++)
        if (strcmp(kit->names[mode][i], name) == 0)
            return i;
    return -1;
}

int icon_kit_chosen(const icon_kit_t *kit, const settings_t *s,
    gamemode_t mode)
{
    int i = icon_kit_find(kit, mode, s->icons[mode]);

    if (i >= 0)
        return i;
    return kit->count[mode] > 0 ? 0 : -1;
}

void icon_kit_choose(const icon_kit_t *kit, settings_t *s, gamemode_t mode,
    int i)
{
    if (i < 0 || i >= kit->count[mode]
        || strcmp(s->icons[mode], kit->names[mode][i]) == 0)
        return;
    snprintf(s->icons[mode], SETTING_FILE_MAX, "%s", kit->names[mode][i]);
    s->dirty = true;
}

int icon_kit_pages(const icon_kit_t *kit, gamemode_t mode)
{
    int n = kit->count[mode];

    return n <= ICONS_PER_PAGE ? 1 : (n + ICONS_PER_PAGE - 1) / ICONS_PER_PAGE;
}

void icon_kit_free(icon_kit_t *kit)
{
    for (int m = 0; m < MODE_COUNT; m++) {
        for (int i = 0; i < kit->count[m]; i++)
            free(kit->names[m][i]);
        free(kit->names[m]);
    }
    *kit = (icon_kit_t){0};
}
