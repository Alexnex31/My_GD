/*
** ALEXNEX PROJECT, 2026
** tests/test_parser.c
** File description:
** the level loader: grammar, rejections, sort and derived values (7.2, 7.3)
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sim/level.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

static int warnings;
static char last[256];

static void count_warning(const char *msg)
{
    snprintf(last, sizeof(last), "%s", msg);
    warnings += 1;
}

static int load(sim_t *s, const char *text);

/* The one warning a text gives: its message, without the file and line. */
static bool said(const char *text, const char *what)
{
    sim_t s;
    const char *colon;

    last[0] = '\0';
    load(&s, text);
    sim_free(&s);
    colon = strchr(last, ':');
    colon = colon != NULL ? strchr(colon + 1, ':') : NULL;
    return warnings == 1 && colon != NULL && strcmp(colon + 2, what) == 0;
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
        "\n"
        "block 1000 200 1\n"
        "spike 3000 750 2\n"
        "portal 2100 750 2 ship\n");
    CHECK(warnings == 0);
    CHECK(s.lvl.nb_objects == 3);
    CHECK(strcmp(s.lvl.hdr.name, "STEREO MADNESS") == 0);
    CHECK(strcmp(s.lvl.hdr.author, "ALEXNEX") == 0);
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

    load(&s, "music stereo_madness.ogg\nmusic_offset 2.5\nbpm 140\n"
        "first_beat 0.25\nblock 1000 700 2\n");
    CHECK(warnings == 0);
    CHECK(strcmp(s.lvl.hdr.music, "stereo_madness.ogg") == 0);
    CHECK(s.lvl.hdr.music_offset == 2.5 && s.lvl.hdr.bpm == 140.0);
    CHECK(s.lvl.hdr.first_beat == 0.25);
    sim_free(&s);
    /* unknown keys and bad numbers warn, the level still loads */
    load(&s, "difficulty hard\nbpm abc\nblock 1000 700 2\n");
    CHECK(warnings == 2);
    CHECK(s.lvl.nb_objects == 1);
    CHECK(s.lvl.hdr.bpm == 0.0);
    sim_free(&s);
    /* version is not a field any more: an old file loads with one warning */
    load(&s, "name LEVEL 1\nversion 2\nblock 1000 700 2\n");
    CHECK(warnings == 1);
    CHECK(s.lvl.nb_objects == 1);
    CHECK(strcmp(s.lvl.hdr.name, "LEVEL 1") == 0);
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
    CHECK(warnings == 0);
    sim_free(&s);
    /* a '#' ends a header line too, and the value is trimmed */
    load(&s, "name  STEREO MADNESS   # the first level\nblock 1000 700 2\n");
    CHECK(warnings == 0);
    CHECK(strcmp(s.lvl.hdr.name, "STEREO MADNESS") == 0);
    sim_free(&s);
    /* the last line of a repeated key wins */
    load(&s, "name FIRST\nname SECOND\nblock 1000 700 2\n");
    CHECK(warnings == 0);
    CHECK(strcmp(s.lvl.hdr.name, "SECOND") == 0);
    sim_free(&s);
    /* an empty value keeps the default instead of blanking it */
    load(&s, "name\nname   \nblock 1000 700 2\n");
    CHECK(warnings == 2);
    CHECK(strcmp(s.lvl.hdr.name, "10280") == 0);
    sim_free(&s);
    /* keys are lowercase: anything else is an unknown field */
    load(&s, "Name STEREO MADNESS\nblock 1000 700 2\n");
    CHECK(warnings == 1);
    CHECK(strcmp(s.lvl.hdr.name, "10280") == 0);
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
    CHECK(s.lvl.skipped_lines == 1);           /* what --check fails on (7.4) */
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
    check_rejected("block 1e12 700 2");          /* finite, but out of range */
    check_rejected("block 1000 -2e7 2");
    check_rejected("block 1000 700 2 w=300000");
    check_rejected("block 1000 700 300000");
    check_rejected("block 1000 700 2 rot=1e308");  /* finite, no angle (7.2) */
    check_rejected("block 1000 700 2 rot=-2e6");
    check_rejected("block 1000 700 2 w=0.01");     /* half a pixel */
    check_rejected("start 2e7 300 cube");
}

/*
** Each message says what is wrong with the line, and a skipped line says
** nothing else first (7.3).
*/
static void test_messages(void)
{
    CHECK(said("start 2e7 300 cube\n",
        "start out of range (10000000 px), line skipped"));
    CHECK(said("start 100 300\n",
        "start needs x y and a gamemode, line skipped"));
    CHECK(said("portal 100 750 2 ship extra\n",
        "unexpected word 'extra', line skipped"));
    CHECK(said("block 1 2 3 foo=1 w=0\n",
        "invalid value for w= ('0'), line skipped"));
    CHECK(said("block 1 2 3 rot=1e308\n",
        "invalid value for rot= ('1e308'), line skipped"));
    CHECK(said("block 1 2 3 foo=1\n", "unknown field foo=, ignored"));
    CHECK(said("difficulty hard\n",
        "unknown header field 'difficulty', ignored"));
    CHECK(said("start_x -1e300\n", "start_x out of range, ignored"));
    CHECK(said("bpm 1001\n", "bpm out of range, ignored"));
    CHECK(said("music_offset 1e6\n", "music_offset out of range, ignored"));
    CHECK(said("bpm 12000000000000000000000000000000000000000000000000000000000"
        "0000000000000000\n", "header field value too long, ignored"));
}

/* A huge turn is no angle, a big one is still its own (7.2, 4.2). */
static void test_rotation_range(void)
{
    sim_t a;
    sim_t b;

    load(&a, "block 1000 700 2 rot=100045\n");
    load(&b, "block 1000 700 2 rot=325\n");
    CHECK(warnings == 0 && a.lvl.nb_objects == 1);
    for (int i = 0; i < a.lvl.objects[0].hitbox.nverts; i++) {
        CHECK(a.lvl.objects[0].hitbox.verts[i].x
            == b.lvl.objects[0].hitbox.verts[i].x);
        CHECK(a.lvl.objects[0].hitbox.verts[i].y
            == b.lvl.objects[0].hitbox.verts[i].y);
    }
    sim_free(&a);
    sim_free(&b);
    load(&a, "block 3000 700 2\nblock 100 700 2 rot=1e308\nblock 1000 700 2\n");
    CHECK(a.lvl.nb_objects == 2 && a.lvl.skipped_lines == 1);
    CHECK(a.lvl.objects[0].rect.x == 1000.0 && a.lvl.objects[1].rect.x == 3000.0);
    sim_free(&a);
}

/* w and h take decimals: a portal is 1.2 blocks, widened by whole ones. */
static void test_decimal_sizes(void)
{
    sim_t s;

    load(&s, "portal 990 750 2 ship w=3.4\nblock 100 700 2 h=0.5\n");
    CHECK(warnings == 0 && s.lvl.nb_objects == 2);
    CHECK(s.lvl.objects[0].rect.h == 25.0);
    CHECK(s.lvl.objects[1].rect.w == 170.0);
    sim_free(&s);
}

/*
** A line is read whole or not at all: too many words or too many characters
** skip it, they never drop its end (7.3). Below that, every field counts.
*/
static void test_long_lines(void)
{
    char text[3000] = "block 100 750 2";
    sim_t s;

    for (int i = 0; i < 20; i++)
        snprintf(text + strlen(text), sizeof(text) - strlen(text), " k%d=1", i);
    strcat(text, " rot=90 w=4\n");
    load(&s, text);
    CHECK(s.lvl.nb_objects == 1 && s.lvl.skipped_lines == 0);
    CHECK(s.lvl.objects[0].rotation == 90.0 && s.lvl.objects[0].rect.w == 200.0);
    sim_free(&s);
    strcpy(text, "block 100 750 2");
    for (int i = 0; i < 70; i++)
        snprintf(text + strlen(text), sizeof(text) - strlen(text), " k%d=1", i);
    strcat(text, "\nblock 500 750 2\n");
    load(&s, text);
    CHECK(s.lvl.nb_objects == 1 && s.lvl.skipped_lines == 1);
    sim_free(&s);
    strcpy(text, "block 100 750 2 group=");
    for (int i = 0; i < 1100; i++)
        strcat(text, "7");
    strcat(text, " rot=90\nblock 500 750 2\n");
    load(&s, text);
    CHECK(s.lvl.nb_objects == 1 && s.lvl.skipped_lines == 1);
    CHECK(s.lvl.objects[0].rect.x == 500.0);
    sim_free(&s);
}

/* Blanks before a key, and the mark some editors write first, are nothing. */
static void test_leading_blanks(void)
{
    sim_t s;

    load(&s, "\xEF\xBB\xBF" "block 100 750 2\n  name Foo\n\tauthor Me\n");
    CHECK(warnings == 0 && s.lvl.nb_objects == 1);
    CHECK(strcmp(s.lvl.hdr.name, "Foo") == 0);
    CHECK(strcmp(s.lvl.hdr.author, "Me") == 0);
    sim_free(&s);
}

/* Header numbers stay where nothing downstream overflows on them (7.2). */
static void test_header_ranges(void)
{
    sim_t s;

    load(&s, "start_x -1e300\nbpm 1e300\nfirst_beat 1e9\nstart 100 800 cube\n");
    CHECK(warnings == 3);
    CHECK(s.lvl.hdr.start.pos.x == PLAYER_SPAWN_X && s.lvl.hdr.bpm == 0.0);
    sim_reset_at(&s, &s.lvl.starts[0]);
    CHECK(s.st.tick == 0);
    sim_free(&s);
    load(&s, "start_x 9999999\nbpm 1000\nmusic_offset -3\n");
    CHECK(warnings == 0 && s.lvl.hdr.bpm == 1000.0);
    CHECK(s.lvl.hdr.start.pos.x == 9999999.0 && s.lvl.hdr.music_offset == -3.0);
    sim_free(&s);
}

/* The range is wide: two hours of level still load (7.2). */
static void test_far_object_kept(void)
{
    sim_t s;

    load(&s, "block 9000000 700 2\n");
    CHECK(warnings == 0 && s.lvl.nb_objects == 1);
    sim_free(&s);
}

static void test_kept_with_warning(void)
{
    sim_t s;

    load(&s, "block 2600 750 2 speed=3\n");
    CHECK(warnings == 1);
    CHECK(s.lvl.nb_objects == 1);
    CHECK(s.lvl.skipped_lines == 0);           /* a warning, not invalid */
    sim_free(&s);
    load(&s, "block 2600 750 2 group=4,5\nblock 2700 750 2 group=6\n");
    CHECK(warnings == 1);                      /* one warning per level */
    CHECK(s.lvl.nb_objects == 2);
    sim_free(&s);
    load(&s, "start_size tiny\nfoo bar\nstart_gamemode rocket\n");
    CHECK(warnings == 3 && s.lvl.skipped_lines == 0);
    sim_free(&s);
}

/* Z: a layer by its name, an order and an editor layer in range (11.15). */
static void test_z_fields(void)
{
    sim_t s;

    load(&s, "block 1000 750 2 zl=b5 z=-10000 el=10000\n"
        "spike 2000 750 2 zl=t4 z=10000\n");
    CHECK(warnings == 0 && s.lvl.nb_objects == 2);
    CHECK(s.lvl.objects[0].z_layer == Z_B5);
    CHECK(s.lvl.objects[0].z_order == -10000);
    CHECK(s.lvl.objects[0].editor_layer == 10000);
    CHECK(s.lvl.objects[1].z_layer == Z_T4);
    CHECK(s.lvl.objects[1].z_order == 10000);
    sim_free(&s);
    CHECK(said("block 1000 750 2 zl=t5\n",
        "invalid value for zl= ('t5'), line skipped"));
    CHECK(said("block 1000 750 2 z=10001\n",
        "invalid value for z= ('10001'), line skipped"));
    CHECK(said("block 1000 750 2 z=1.5\n",
        "invalid value for z= ('1.5'), line skipped"));
    CHECK(said("block 1000 750 2 el=-1\n",
        "invalid value for el= ('-1'), line skipped"));
}

static void test_fields(void)
{
    sim_t s;

    load(&s, "block 5000 650 2 w=8 h=1\n"
        "spike 1000 0 2 rot=180\n"
        "slope 2000 750 2\n");
    CHECK(warnings == 0);
    /* sorted by hitbox left edge: the spike (1037), the slope, the block */
    CHECK(s.lvl.objects[2].rect.w == 400.0 && s.lvl.objects[2].rect.h == 50.0);
    CHECK(s.lvl.objects[0].rotation == 180.0);
    CHECK(s.lvl.objects[0].hitbox.aabb.x == 1037.0);
    CHECK(s.lvl.objects[0].hitbox.aabb.y == 32.0);
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
    /* the run is measured from the spawn: 500 px past the last object (7.2) */
    CHECK(s.lvl.end_shift == 5400.0 + LEVEL_END_PADDING - PLAYER_SPAWN_X);
    CHECK(s.lvl.kill_y == GROUND_Y - modes_tallest_corridor()
        - KILL_CEILING_MARGIN);
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

/* A directory named like a level is not a file to read, not a huge one. */
static void test_directory_level(void)
{
    char dir[] = "/tmp/my_gd_testXXXXXX";
    char path[64];
    level_header_t hdr;
    sim_t s;

    if (mkdtemp(dir) == NULL)
        return;
    snprintf(path, sizeof(path), "%s/5.gd", dir);
    if (mkdir(path, 0755) == 0) {
        CHECK(sim_load(&s, path, count_warning) != 0);
        CHECK(level_read_header(path, &hdr, NULL) != 0);
        sim_free(&s);
        rmdir(path);
    }
    rmdir(dir);
}

static void test_real_levels(void)
{
    sim_t s;
    char path[64];

    for (int i = 1; i <= 7; i++) {
        snprintf(path, sizeof(path), "levels/%d.gd", i);
        warnings = 0;
        if (sim_load(&s, path, count_warning) != 0) {
            CHECK(!"levels/ not found: make test runs from the repository");
            continue;
        }
        CHECK(warnings == 0);
        CHECK(s.lvl.nb_objects > 0);
        CHECK(s.lvl.end_shift > 1000.0);
        CHECK(strncmp(s.lvl.hdr.name, "LEVEL", 5) == 0);
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

/* The level list reads a name and a version marker without building shapes. */
static void test_read_header_only(void)
{
    level_header_t a = {0};
    level_header_t b = {0};
    sim_t s;
    uint64_t ha = 0;
    uint64_t hb = 0;

    if (level_read_header("levels/7.gd", &a, &ha) != 0) {
        CHECK(!"levels/ not found: make test runs from the repository");
        return;
    }
    CHECK(strncmp(a.name, "LEVEL", 5) == 0);
    CHECK(ha != 0);
    CHECK(level_read_header("levels/7.gd", &b, &hb) == 0);
    CHECK(ha == hb);                           /* the same file, the same hash */
    CHECK(level_read_header("levels/6.gd", &b, &hb) == 0);
    CHECK(ha != hb);                           /* a different one, another hash */
    CHECK(level_read_header("levels/nope.gd", &b, &hb) == -1);
    CHECK(sim_load(&s, "levels/7.gd", NULL) == 0);
    CHECK(s.lvl.file_hash == ha);              /* the loader's own hash, the same */
    CHECK(strcmp(s.lvl.hdr.name, a.name) == 0);
    CHECK(s.lvl.hdr.start.pos.x == a.start.pos.x);
    CHECK(s.lvl.hdr.start.pos.y == a.start.pos.y);
    CHECK(s.lvl.hdr.start.mode == a.start.mode);
    sim_free(&s);
    /* FNV-1a, against its published value for "a" */
    CHECK(fnv1a("a", 1) == 0xaf63dc4c8601ec8cULL);
    CHECK(fnv1a("", 0) == 0xcbf29ce484222325ULL);
}

void test_parser(void)
{
    test_z_fields();
    test_read_header_only();
    test_basic_level();
    test_whitespace_and_comments();
    test_header_fields();
    test_name_rules();
    test_rejections();
    test_messages();
    test_rotation_range();
    test_decimal_sizes();
    test_long_lines();
    test_leading_blanks();
    test_header_ranges();
    test_far_object_kept();
    test_kept_with_warning();
    test_fields();
    test_sort();
    test_derived_values();
    test_empty_level();
    test_reset_after_load();
    test_level_ids();
    test_directory_level();
    test_real_levels();
    test_big_level();
}
