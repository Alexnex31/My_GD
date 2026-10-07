/*
** ALEXNEX PROJECT, 2026
** tests/test_doc.c
** File description:
** a level as a document: parsed, written back, and played from memory (FEATURES 11.1)
*/

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/level.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

static int warnings;

static void count_warning(const char *msg)
{
    (void)msg;
    warnings += 1;
}

static int parse(const char *text, level_doc_t *doc, level_header_t *hdr)
{
    warnings = 0;
    level_header_defaults(hdr, "7");
    return level_parse_doc(text, strlen(text), "7", doc, hdr, count_warning);
}

/* Start positions: a place, a gamemode, maybe a gravity and a speed. */
static void test_starts(void)
{
    level_doc_t doc;
    level_header_t hdr;
    sim_t s;
    const char *text = "start 2000 300 ship\nblock 1000 750 2\n"
        "start 5000 800 ball up 2\nstart 6000 100 wave 0.5\n";

    CHECK(parse(text, &doc, &hdr) == 0 && warnings == 0);
    CHECK(doc.count == 1 && doc.nb_starts == 3);
    CHECK(doc.starts[0].pos.x == 2000.0 && doc.starts[0].pos.y == 300.0);
    CHECK(doc.starts[0].mode == MODE_SHIP && doc.starts[0].gravity_dir == 1);
    CHECK(doc.starts[0].speed_mult == 1.0);
    CHECK(doc.starts[1].mode == MODE_BALL && doc.starts[1].gravity_dir == -1);
    CHECK(doc.starts[1].speed_mult == 2.0);
    CHECK(doc.starts[2].mode == MODE_WAVE && doc.starts[2].speed_mult == 0.5);
    CHECK(hdr.start.pos.x == PLAYER_SPAWN_X && hdr.start.mode == MODE_CUBE);
    level_doc_free(&doc);
    CHECK(parse("start 2000 300\nstart 2000 300 spider\nstart x 300 cube\n"
        "start 2000 300 cube sideways\nstart 2000 300 cube up 7\n", &doc,
        &hdr) == 5);
    CHECK(warnings == 5 && doc.nb_starts == 0 && doc.count == 0);
    level_doc_free(&doc);
    sim_load_mem(&s, text, strlen(text), "7", count_warning);
    CHECK(s.lvl.nb_objects == 1 && s.lvl.nb_starts == 3);   /* no object */
    CHECK(s.lvl.starts[1].pos.x == 5000.0);
    CHECK(s.st.player.pos.x == PLAYER_SPAWN_X);    /* the real start is its own */
    sim_free(&s);
}

/* What nothing here edits is kept as written, per object. */
static void test_extras(void)
{
    level_doc_t doc;
    level_header_t hdr;
    char *text;

    parse("block 1000 750 2 group=3 rot=90 colour=red\nspike 2000 750 2\n",
        &doc, &hdr);
    CHECK(doc.count == 2 && warnings == 2);
    CHECK(doc.extras[0] != NULL
        && strcmp(doc.extras[0], " group=3 colour=red") == 0);
    CHECK(doc.extras[1] == NULL && doc.objs[0].rotation == 90.0);
    text = level_write_mem(&doc, &hdr);
    CHECK(strstr(text, "block 1000 750 2 rot=90 group=3 colour=red\n") != NULL);
    free(text);
    level_doc_free(&doc);
}

/* Each object comes back from its own line as the same object. */
static void test_lines(void)
{
    static const char *const lines[] = {
        "block 1000 750 2", "block 1000 650 2 w=8 h=1",
        "slope 6000 750 2 rot=180", "spike 3400 0 2 rot=180",
        "block 4500 -50 2 rot=30", "portal 2100 750 2 ship",
        "portal 2100 750 2 ball w=3", "portal 2100 750 2 wave h=4",
        "gravity 4000 750 2 up", "gravity 4000.5 -120.25 1 down",
        "pad 1400 750 2 yellow", "pad 6300 350 2 blue rot=180",
        "orb 1750 550 2 green", "orb 7700 320 3 black"};
    level_doc_t doc = {0};
    level_header_t hdr;
    object_t o;
    char *text;
    char expect[128];

    level_header_defaults(&hdr, "7");
    hdr.name[0] = '\0';
    doc.objs = &o;
    doc.count = 1;
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        CHECK(level_object_from_line(lines[i], &o) == 0);
        text = level_write_mem(&doc, &hdr);
        snprintf(expect, sizeof(expect), "%s\n", lines[i]);
        CHECK(strcmp(text, expect) == 0);
        free(text);
    }
    CHECK(level_object_from_line("name hello", &o) != 0);
    CHECK(level_object_from_line("pad 1000 750 2 black", &o) != 0);
}

/* The header: only what a file needs to say. */
static void test_header(void)
{
    level_doc_t doc;
    level_header_t hdr;
    char *text;

    parse("", &doc, &hdr);
    text = level_write_mem(&doc, &hdr);
    CHECK(strcmp(text, "name 7\n") == 0);
    free(text);
    level_doc_free(&doc);
    parse("name My level\nauthor Me\nmusic a.ogg\nmusic_offset 1.5\nbpm 128\n"
        "first_beat 0.35\nstart_x 2000\nstart_y 300\nstart_gamemode ship\n"
        "start_gravity flipped\nstart_speed 2\nstart 500 500 ufo up 3\n"
        "block 100 750 2\n", &doc, &hdr);
    text = level_write_mem(&doc, &hdr);
    CHECK(strcmp(text, "name My level\nauthor Me\nmusic a.ogg\n"
        "music_offset 1.5\nbpm 128\nfirst_beat 0.35\nstart_x 2000\n"
        "start_y 300\nstart_gamemode ship\nstart_gravity flipped\n"
        "start_speed 2\nstart 500 500 ufo up 3\n\nblock 100 750 2\n") == 0);
    free(text);
    level_doc_free(&doc);
}

static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    char *buf;

    if (f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    *len = (size_t)ftell(f);
    rewind(f);
    buf = calloc(*len + 1, 1);
    *len = fread(buf, 1, *len, f);
    fclose(f);
    return buf;
}

/* The same inputs on both: every tick's player, bit for bit. */
static bool same_play(sim_t *a, sim_t *b, bool same_order)
{
    for (int i = 0; i < 6000 && !a->st.complete && a->st.player.alive; i++) {
        input_t in = {(i / 23) % 3 == 0, (i / 23) % 3 == 0 && i % 23 == 0};

        sim_tick(a, in);
        sim_tick(b, in);
        if (a->st.player.pos.x != b->st.player.pos.x
            || a->st.player.pos.y != b->st.player.pos.y
            || a->st.player.vy != b->st.player.vy
            || a->st.player.mode != b->st.player.mode
            || a->st.player.alive != b->st.player.alive)
            return false;
        if (same_order && sim_state_hash(a) != sim_state_hash(b))
            return false;
    }
    return a->st.complete == b->st.complete;
}

/*
** One level of levels/: written and parsed again it is the same text, the
** document plays like the file, and so does the file it writes.
*/
static void check_round_trip(const char *path)
{
    size_t len = 0;
    char *file = read_file(path, &len);
    level_doc_t doc;
    level_doc_t again;
    level_header_t hdr;
    level_header_t hdr2;
    char *text;
    char *text2;
    sim_t from_file;
    sim_t from_doc;

    CHECK(file != NULL);
    level_header_defaults(&hdr, "1");
    level_header_defaults(&hdr2, "1");
    CHECK(level_parse_doc(file, len, "1", &doc, &hdr, NULL) == 0);
    text = level_write_mem(&doc, &hdr);
    CHECK(level_parse_doc(text, strlen(text), "1", &again, &hdr2, NULL) == 0);
    text2 = level_write_mem(&again, &hdr2);
    CHECK(doc.count == again.count && strcmp(text, text2) == 0);
    sim_load_mem(&from_file, file, len, "1", NULL);
    sim_init(&from_doc, &doc, &hdr, "1");
    CHECK(from_doc.lvl.end_shift == from_file.lvl.end_shift);
    CHECK(same_play(&from_file, &from_doc, true));
    sim_free(&from_doc);
    sim_reset(&from_file);
    sim_load_mem(&from_doc, text, strlen(text), "1", NULL);
    CHECK(same_play(&from_file, &from_doc, false));   /* ties in x may swap */
    sim_free(&from_doc);
    sim_free(&from_file);
    level_doc_free(&doc);
    level_doc_free(&again);
    free(text);
    free(text2);
    free(file);
}

static void test_every_level(void)
{
    DIR *dir = opendir("levels");
    struct dirent *e;
    char path[300];
    int seen = 0;

    CHECK(dir != NULL);
    while (dir != NULL && (e = readdir(dir)) != NULL) {
        if (strstr(e->d_name, ".gd") == NULL)
            continue;
        snprintf(path, sizeof(path), "levels/%s", e->d_name);
        check_round_trip(path);
        seen += 1;
    }
    if (dir != NULL)
        closedir(dir);
    CHECK(seen >= 12);
}

/* A file is written whole or not at all, and reads back. */
static void test_write_file(void)
{
    level_doc_t doc;
    level_header_t hdr;
    size_t len = 0;
    char *back;

    parse("name Saved\nblock 100 750 2\n", &doc, &hdr);
    CHECK(level_write("/tmp/mygd_test_doc.gd", &doc, &hdr) == 0);
    back = read_file("/tmp/mygd_test_doc.gd", &len);
    CHECK(back != NULL && strcmp(back, "name Saved\n\nblock 100 750 2\n") == 0);
    CHECK(read_file("/tmp/mygd_test_doc.gd.tmp", &len) == NULL);
    CHECK(level_write("/nonexistent_dir/x.gd", &doc, &hdr) != 0);
    free(back);
    remove("/tmp/mygd_test_doc.gd");
    level_doc_free(&doc);
}

void test_doc(void)
{
    test_starts();
    test_extras();
    test_lines();
    test_header();
    test_every_level();
    test_write_file();
}
