/*
** ALEXNEX PROJECT, 2026
** tests/test_parser.c
** File description:
** the level loader: grammar, rejections, sort and derived values (7.2, 7.3)
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sim/level.h"
#include "sim/sim.h"
#include "test.h"

static int warnings;

static void count_warning(const char *msg)
{
    (void)msg;
    warnings += 1;
}

static int load(sim_t *s, const char *text)
{
    warnings = 0;
    return sim_load_mem(s, text, strlen(text), "10280", count_warning);
}

static void test_basic_level(void)
{
    sim_t s;

    load(&s, "name STEREO MADNESS\n"
        "author ALEXNEX\n"
        "version 2\n"
        "\n"
        "block 1000 200 1\n"
        "spike 3000 750 2\n"
        "portal 2100 750 2 ship\n");
    CHECK(warnings == 0);
    CHECK(s.lvl.nb_objects == 3);
    CHECK(strcmp(s.lvl.hdr.name, "STEREO MADNESS") == 0);
    CHECK(strcmp(s.lvl.hdr.author, "ALEXNEX") == 0);
    CHECK(s.lvl.hdr.version == 2);
    CHECK(strcmp(s.lvl.id, "10280") == 0);
    CHECK(s.lvl.objects[0].type == OBJ_BLOCK);
    CHECK(s.lvl.objects[0].rect.x == 1000.0 && s.lvl.objects[0].rect.y == 200.0);
    CHECK(s.lvl.objects[0].rect.w == 50.0 && s.lvl.objects[0].size == 1);
    CHECK(s.lvl.objects[1].type == OBJ_PORTAL);
    CHECK(s.lvl.objects[1].portal_mode == MODE_SHIP);
    CHECK(s.lvl.objects[2].type == OBJ_SPIKE);
    sim_free(&s);
}

static void test_whitespace_and_comments(void)
{
    sim_t s;

    load(&s, "# a whole comment\n"
        "\n"
        "   \t\n"
        "\tblock 1000 700 2   # trailing comment\r\n"
        "spike 2000 750 2");            /* no trailing newline */
    CHECK(warnings == 0);
    CHECK(s.lvl.nb_objects == 2);
    sim_free(&s);
}

/* The music and editor fields of 7.2, parsed now, used by FEATURES later. */
static void test_header_fields(void)
{
    sim_t s;

    load(&s, "song stereo_madness.ogg\noffset 2.5\nbpm 140\n"
        "first_beat 0.25\nblock 1000 700 2\n");
    CHECK(warnings == 0);
    CHECK(strcmp(s.lvl.hdr.song, "stereo_madness.ogg") == 0);
    CHECK(s.lvl.hdr.offset == 2.5 && s.lvl.hdr.bpm == 140.0);
    CHECK(s.lvl.hdr.first_beat == 0.25);
    sim_free(&s);
    /* unknown keys and bad numbers warn, the level still loads */
    load(&s, "difficulty hard\nbpm abc\nblock 1000 700 2\n");
    CHECK(warnings == 2);
    CHECK(s.lvl.nb_objects == 1);
    CHECK(s.lvl.hdr.bpm == 0.0);
    sim_free(&s);
    /* the old format is not read any more: its header is just a bad line */
    load(&s, "7 35 100.000000\nblock 1000 700 2\n");
    CHECK(warnings == 1);
    CHECK(s.lvl.nb_objects == 1);
    sim_free(&s);
}

static void test_name_rules(void)
{
    sim_t s;
    char text[512];
    char long_name[300];

    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    snprintf(text, sizeof(text), "name %s\n", long_name);
    load(&s, text);
    CHECK(strlen(s.lvl.hdr.name) == sizeof(s.lvl.hdr.name) - 1);
    sim_free(&s);
    load(&s, "block 1000 700 2\n");
    CHECK(strcmp(s.lvl.hdr.name, "10280") == 0);   /* the id by default */
    sim_free(&s);
}

static void check_rejected(const char *line)
{
    sim_t s;
    char text[256];

    snprintf(text, sizeof(text), "block 500 700 2\n%s\nblock 5000 700 2\n",
        line);
    load(&s, text);
    CHECK(warnings == 1);
    CHECK(s.lvl.nb_objects == 2);              /* the good lines still load */
    sim_free(&s);
}

static void test_rejections(void)
{
    check_rejected("block 1000 700 0");
    check_rejected("block 1000 700 -1");
    check_rejected("block 1000 700 2.5");
    check_rejected("block nan 700 2");
    check_rejected("block 1000 inf 2");
    check_rejected("block 1e999 700 2");
    check_rejected("block 750x 700 2");
    check_rejected("spike 1200 750");

    check_rejected("portal 2100 750 2 rocket");
    check_rejected("portal 2100 750 2");
    check_rejected("block 1000 750 2 ship");
    check_rejected("block 1000 700 2 w=0");
    check_rejected("block 1000 700 2 h=-2");
    check_rejected("block 1000 700 2 rot=abc");
}

static void test_kept_with_warning(void)
{
    sim_t s;

    load(&s, "block 2600 750 2 speed=3\n");
    CHECK(warnings == 1);
    CHECK(s.lvl.nb_objects == 1);
    sim_free(&s);
    load(&s, "block 2600 750 2 group=4,5\nblock 2700 750 2 group=6\n");
    CHECK(warnings == 1);                      /* one warning per level */
    CHECK(s.lvl.nb_objects == 2);
    sim_free(&s);
}

static void test_fields(void)
{
    sim_t s;

    load(&s, "block 5000 650 2 w=8 h=1\n"
        "spike 1000 0 2 rot=180\n"
        "slope 2000 750 2\n");
    CHECK(warnings == 0);
    /* sorted by hitbox left edge: the spike (1030), the slope, the block */
    CHECK(s.lvl.objects[2].rect.w == 400.0 && s.lvl.objects[2].rect.h == 50.0);
    CHECK(s.lvl.objects[0].rotation == 180.0);
    CHECK(s.lvl.objects[0].hitbox.aabb.x == 1030.0);
    CHECK(s.lvl.objects[0].hitbox.aabb.y == 0.0);
    CHECK(s.lvl.objects[1].hitbox.nverts == 3);
    sim_free(&s);
}

/* Sorted by hitbox left edge, ties in file order (4.1). */
static void test_sort(void)
{
    sim_t s;

    load(&s, "block 3000 700 2\n"
        "block 1000 700 2 rot=45\n"            /* its aabb starts left of 1000 */
        "block 1000 700 2\n"
        "block 2000 700 2\n");
    CHECK(warnings == 0);
    CHECK(s.lvl.nb_objects == 4);
    for (size_t i = 1; i < s.lvl.nb_objects; i++)
        CHECK(s.lvl.objects[i - 1].hitbox.aabb.x
            <= s.lvl.objects[i].hitbox.aabb.x);
    CHECK(s.lvl.objects[0].rotation == 45.0);
    CHECK(s.lvl.objects[0].hitbox.aabb.x < 1000.0);
    CHECK(s.lvl.objects[1].rect.x == 1000.0 && s.lvl.objects[1].line == 3);
    sim_free(&s);
}

static void test_derived_values(void)
{
    sim_t s;

    load(&s, "block 1000 700 2\nblock 5000 650 2 w=8 h=1\n");
    CHECK(s.lvl.reach == 400.0);
    CHECK(s.lvl.end_shift == 5400.0 + LEVEL_END_PADDING);
    CHECK(s.lvl.kill_y == GROUND_Y - CORRIDOR_MAX_HEIGHT - KILL_CEILING_MARGIN);
    sim_free(&s);
    load(&s, "block 1000 -400 2\n");           /* higher than any corridor */
    CHECK(s.lvl.kill_y == -400.0 - KILL_CEILING_MARGIN);
    sim_free(&s);
}

static void test_empty_level(void)
{
    sim_t s;

    load(&s, "# nothing here\n");
    CHECK(warnings == 0);
    CHECK(s.lvl.nb_objects == 0);
    CHECK(s.lvl.reach == 0.0);
    CHECK(s.lvl.end_shift == 100.0);
    sim_free(&s);
}

static void test_reset_after_load(void)
{
    sim_t s;

    load(&s, "block 1000 700 2\nportal 2000 700 2 ship\n");
    CHECK(s.st.player.pos.x == PLAYER_SPAWN_X);
    CHECK(s.st.player.pos.y == 800.0);
    CHECK(s.st.player.vx == PER_TICK(SCROLL_SPEED));
    CHECK(s.st.player.mode == MODE_CUBE && s.st.player.gravity_dir == 1);
    CHECK(s.st.player.hold == HOLD_FRESH && s.st.player.alive);
    CHECK(s.st.player.grounded && s.st.player.can_jump);
    CHECK(s.st.tick == 0 && s.st.distance == 0.0 && !s.st.complete);
    CHECK(s.st.spent_words == 1);
    for (size_t i = 0; i < s.lvl.nb_objects; i++)
        CHECK(!is_spent(&s.st, i));
    sim_free(&s);
}

static void test_level_ids(void)
{
    char id[24];
    sim_t s;

    CHECK(level_id_from_path("levels/10280.gd", id, sizeof(id)));
    CHECK(strcmp(id, "10280") == 0);
    CHECK(level_id_from_path("7.gd", id, sizeof(id)) && strcmp(id, "7") == 0);
    CHECK(!level_id_from_path("levels/level7", id, sizeof(id)));
    CHECK(!level_id_from_path("levels/my_level.gd", id, sizeof(id)));
    CHECK(!level_id_from_path("levels/10280.gd.temp", id, sizeof(id)));
    CHECK(!level_id_from_path("levels/.gd", id, sizeof(id)));
    CHECK(!level_id_from_path("levels/12345678901234567890123.gd", id,
        sizeof(id)));                          /* longer than the id buffer */
    CHECK(sim_load(&s, "levels/level7", count_warning) != 0);   /* old name */
    sim_free(&s);
}

static void test_real_levels(void)
{
    sim_t s;
    char path[64];

    for (int i = 1; i <= 7; i++) {
        snprintf(path, sizeof(path), "levels/%d.gd", i);
        warnings = 0;
        if (sim_load(&s, path, count_warning) != 0)
            continue;                          /* run from another directory */
        CHECK(warnings == 0);
        CHECK(s.lvl.nb_objects > 0);
        CHECK(s.lvl.end_shift > 1000.0);
        CHECK(strncmp(s.lvl.hdr.name, "LEVEL", 5) == 0);
        CHECK(s.lvl.hdr.version == 2);
        CHECK(atoi(s.lvl.id) == i);
        sim_free(&s);
    }
}

static void test_big_level(void)
{
    sim_t s;
    size_t size = 10000 * 24 + 1;
    char *text = malloc(size);
    size_t pos = 0;

    for (int i = 0; i < 10000; i++)
        pos += snprintf(text + pos, size - pos, "block %d 700 2\n",
            10000 - i);
    load(&s, text);
    CHECK(warnings == 0);
    CHECK(s.lvl.nb_objects == 10000);
    for (size_t i = 1; i < s.lvl.nb_objects; i++)
        CHECK(s.lvl.objects[i - 1].hitbox.aabb.x
            <= s.lvl.objects[i].hitbox.aabb.x);
    sim_free(&s);
    free(text);
}

void test_parser(void)
{
    test_basic_level();
    test_whitespace_and_comments();
    test_header_fields();
    test_name_rules();
    test_rejections();
    test_kept_with_warning();
    test_fields();
    test_sort();
    test_derived_values();
    test_empty_level();
    test_reset_after_load();
    test_level_ids();
    test_real_levels();
    test_big_level();
}
