/*
** ALEXNEX PROJECT, 2026
** tests/test_icons.c
** File description:
** the icon kit: which files, their order, the one chosen (FEATURES 3.8)
*/

#include "test.h"
#include <string.h>

#include "ui/icon_kit.h"

/* Natural order, whatever order the folder lists them in. */
static void test_order(void)
{
    static const char *const FILES[] = {"cube10.png", "b.png", "cube2.png",
        "cube02.png", "a.PNG", "cube1.png"};
    static const char *const SORTED[] = {"a.PNG", "b.png", "cube1.png",
        "cube02.png", "cube2.png", "cube10.png"};
    icon_kit_t kit = {0};
    int same = 0;

    for (size_t i = 0; i < 6; i++)
        CHECK(icon_kit_add(&kit, MODE_CUBE, FILES[i]));
    CHECK(kit.count[MODE_CUBE] == 6 && kit.count[MODE_SHIP] == 0);
    for (int i = 0; i < 6; i++)
        same += strcmp(kit.names[MODE_CUBE][i], SORTED[i]) == 0;
    CHECK(same == 6);
    icon_kit_free(&kit);
    CHECK(kit.count[MODE_CUBE] == 0 && kit.names[MODE_CUBE] == NULL);
}

/* Only plain .png names that fit the settings, each once. */
static void test_refused(void)
{
    static const char *const BAD[] = {"", ".png", ".hidden.png", "a.jpg",
        "a.png.txt", "dir/a.png", "a\\b.png", "tab\t.png", "png"};
    icon_kit_t kit = {0};
    char long_name[SETTING_FILE_MAX + 8];

    for (size_t i = 0; i < sizeof(BAD) / sizeof(BAD[0]); i++)
        CHECK(!icon_kit_add(&kit, MODE_WAVE, BAD[i]));
    memset(long_name, 'a', sizeof(long_name));
    memcpy(long_name + SETTING_FILE_MAX - 4, ".png", 5);   /* 128 with it */
    CHECK(!icon_kit_add(&kit, MODE_WAVE, long_name));
    memcpy(long_name + SETTING_FILE_MAX - 5, ".png", 5);   /* 127: fits */
    CHECK(icon_kit_add(&kit, MODE_WAVE, long_name));
    CHECK(icon_kit_add(&kit, MODE_WAVE, "w.png"));
    CHECK(!icon_kit_add(&kit, MODE_WAVE, "w.png"));         /* once */
    CHECK(kit.count[MODE_WAVE] == 2);
    icon_kit_free(&kit);
}

/* The settings' name, else the first; a choice is saved by name. */
static void test_choice(void)
{
    icon_kit_t kit = {0};
    settings_t s;

    settings_defaults(&s);
    CHECK(icon_kit_chosen(&kit, &s, MODE_BALL) == -1);     /* none at all */
    icon_kit_add(&kit, MODE_BALL, "ball_2.png");
    icon_kit_add(&kit, MODE_BALL, "ball_icon.png");
    CHECK(icon_kit_chosen(&kit, &s, MODE_BALL) == 1);      /* the default */
    snprintf(s.icons[MODE_BALL], SETTING_FILE_MAX, "%s", "gone.png");
    CHECK(icon_kit_chosen(&kit, &s, MODE_BALL) == 0);      /* missing: first */
    icon_kit_choose(&kit, &s, MODE_BALL, 1);
    CHECK(s.dirty && strcmp(s.icons[MODE_BALL], "ball_icon.png") == 0);
    s.dirty = false;
    icon_kit_choose(&kit, &s, MODE_BALL, 1);               /* the same: */
    icon_kit_choose(&kit, &s, MODE_BALL, 2);               /* out of range */
    icon_kit_choose(&kit, &s, MODE_BALL, -1);
    CHECK(!s.dirty && strcmp(s.icons[MODE_BALL], "ball_icon.png") == 0);
    icon_kit_add(&kit, MODE_BALL, "ball_1.png");           /* a new file */
    CHECK(icon_kit_chosen(&kit, &s, MODE_BALL) == 2);      /* same choice */
    icon_kit_free(&kit);
}

static void test_pages(void)
{
    icon_kit_t kit = {0};
    char name[32];
    int pages[4] = {0};

    pages[0] = icon_kit_pages(&kit, MODE_UFO);
    for (int i = 0; i < 2 * ICONS_PER_PAGE + 1; i++) {
        snprintf(name, sizeof(name), "u%d.png", i);
        icon_kit_add(&kit, MODE_UFO, name);
        if (i + 1 == ICONS_PER_PAGE)
            pages[1] = icon_kit_pages(&kit, MODE_UFO);
        if (i + 1 == ICONS_PER_PAGE + 1)
            pages[2] = icon_kit_pages(&kit, MODE_UFO);
    }
    pages[3] = icon_kit_pages(&kit, MODE_UFO);
    CHECK(pages[0] == 1 && pages[1] == 1 && pages[2] == 2 && pages[3] == 3);
    CHECK(strcmp(kit.names[MODE_UFO][ICONS_PER_PAGE], "u30.png") == 0);
    icon_kit_free(&kit);
}

/* GD's tabs: every mode once. */
static void test_tab_order(void)
{
    int seen[MODE_COUNT] = {0};
    int once = 0;

    for (int i = 0; i < MODE_COUNT; i++)
        seen[ICON_KIT_ORDER[i]] += 1;
    for (int m = 0; m < MODE_COUNT; m++)
        once += seen[m] == 1;
    CHECK(once == MODE_COUNT && ICON_KIT_ORDER[0] == MODE_CUBE);
}

void test_icons(void)
{
    test_order();
    test_refused();
    test_choice();
    test_pages();
    test_tab_order();
}
