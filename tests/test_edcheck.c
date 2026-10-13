/*
** ALEXNEX PROJECT, 2026
** tests/test_edcheck.c
** File description:
** what the editor says about a level: its warnings, the bot, the autosave (FEATURES 11.10, 11.12, 11.13)
*/

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "editor/ed_autosave.h"
#include "editor/ed_check.h"
#include "editor/ed_music.h"
#include "editor/ed_verify.h"
#include "sim/constants.h"
#include "test.h"

static int add(ed_level_t *lv, const char *line)
{
    object_t o;

    level_object_from_line(line, &o);
    return ed_add(lv, &o, NULL);
}

static size_t count_of(const ed_report_t *r, ed_issue_kind_t kind)
{
    size_t n = 0;

    for (size_t i = 0; i < r->count; i++)
        n += r->issues[i].kind == kind;
    return n;
}

static bool flagged(const ed_report_t *r, ed_issue_kind_t kind, int id)
{
    for (size_t i = 0; i < r->count; i++)
        if (r->issues[i].kind == kind && r->issues[i].id == id)
            return true;
    return false;
}

/* A level with nothing wrong says nothing, start positions included. */
static void test_clean(void)
{
    ed_level_t lv;
    ed_report_t r = {0};
    ed_object_t start;

    ed_level_new(&lv, "1");
    ed_check(&lv, &r);
    CHECK(r.count == 0 && r.warnings == 0 && r.hints == 0);
    add(&lv, "block 1500 750 2");
    add(&lv, "block 1600 750 2");                /* side by side */
    add(&lv, "spike 1500 650 2");                /* standing on the block */
    add(&lv, "spike 1700 750 2");                /* beside it */
    add(&lv, "portal 2500 650 2 ship");
    add(&lv, "portal 4000 650 2 cube");
    add(&lv, "block 5000 600 2 rot=45");         /* 45 degrees: a slope */
    add(&lv, "block 5400 600 2 rot=90");
    ed_start_object(&start, &(level_start_t){.pos = {100.0, 2000.0},
        .speed = 1.0, .gravity_dir = 1});
    ed_add_object(&lv, &start);                  /* underground, behind */
    ed_add_object(&lv, &start);                  /* and twice */
    ed_check(&lv, &r);
    CHECK(r.count == 0 && r.warnings == 0 && r.hints == 0);
    ed_report_free(&r);
    CHECK(r.issues == NULL && r.count == 0);
    ed_level_free(&lv);
}

static void test_twice(void)
{
    ed_level_t lv;
    ed_report_t r = {0};
    int second;

    ed_level_new(&lv, "1");
    add(&lv, "block 1500 750 2");
    add(&lv, "spike 1500 750 2 rot=90");         /* another type */
    add(&lv, "block 1500 750 2 rot=90");         /* another turn */
    add(&lv, "block 1500 750 2 w=3");            /* another size */
    add(&lv, "block 1525 750 2");                /* another place */
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_ISSUE_TWICE) == 0);
    second = add(&lv, "block 1500 750 2");
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_ISSUE_TWICE) == 1);
    CHECK(flagged(&r, ED_ISSUE_TWICE, second));  /* the one on top */
    add(&lv, "block 1500 750 2");
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_ISSUE_TWICE) == 2);    /* three: two too many */
    CHECK(!ed_issue_is_hint(ED_ISSUE_TWICE));
    ed_report_free(&r);
    ed_level_free(&lv);
}

/* Wholly under the ground, wholly behind the start: by the hitbox. */
static void test_out_of_reach(void)
{
    ed_level_t lv;
    ed_report_t r = {0};
    int under;
    int behind;

    ed_level_new(&lv, "1");
    add(&lv, "block 1500 800 2");                /* half in the ground */
    under = add(&lv, "block 1700 850 2");
    add(&lv, "block 250 750 2");                 /* its edge at the player's */
    add(&lv, "block 200 750 2");                 /* touches the player's back */
    behind = add(&lv, "block 199 750 2");
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_ISSUE_UNDERGROUND) == 1);
    CHECK(flagged(&r, ED_ISSUE_UNDERGROUND, under));
    CHECK(count_of(&r, ED_ISSUE_BEHIND_START) == 1);
    CHECK(flagged(&r, ED_ISSUE_BEHIND_START, behind));
    lv.hdr.start.pos.x = 2000.0;                 /* the start moved on */
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_ISSUE_BEHIND_START) == 5);
    CHECK(r.warnings == 6 && r.hints == 0);
    ed_report_free(&r);
    ed_level_free(&lv);
}

/* 50 degrees is the limit between a slope and a wall (PLAN 4.4). */
static void test_steep(void)
{
    ed_level_t lv;
    ed_report_t r = {0};
    int wall;

    ed_level_new(&lv, "1");
    add(&lv, "block 1500 600 2");
    add(&lv, "block 1900 600 2 rot=45");
    add(&lv, "block 2300 600 2 rot=90");
    add(&lv, "block 2700 600 2 rot=180");
    add(&lv, "spike 3100 600 2 rot=60");         /* a spike is no floor */
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_ISSUE_STEEP) == 0);
    wall = add(&lv, "block 3500 600 2 rot=60");  /* its 60 degree face */
    add(&lv, "block 3900 600 2 rot=30");         /* the same, turned over */
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_ISSUE_STEEP) == 2);    /* once each, not per face */
    CHECK(flagged(&r, ED_ISSUE_STEEP, wall));
    ed_report_free(&r);
    ed_level_free(&lv);
}

/* On a block is where spikes go; in one is a hint, and only a hint. */
static void test_spike_in_block(void)
{
    ed_level_t lv;
    ed_report_t r = {0};
    int in;
    int in_too;

    ed_level_new(&lv, "1");
    add(&lv, "block 1500 550 2");
    add(&lv, "spike 1500 450 2");                /* on it */
    add(&lv, "spike 1600 550 2");                /* beside it */
    add(&lv, "spike 1400 550 2");
    add(&lv, "spike 1500 650 2 rot=180");        /* under it */
    add(&lv, "portal 1500 550 2 cube");          /* no spike */
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_HINT_SPIKE_IN_BLOCK) == 0);
    in = add(&lv, "spike 1500 550 2");           /* the very cell */
    in_too = add(&lv, "spike 1450 500 2");       /* a corner of it, from the left */
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_HINT_SPIKE_IN_BLOCK) == 2);
    CHECK(flagged(&r, ED_HINT_SPIKE_IN_BLOCK, in));
    CHECK(flagged(&r, ED_HINT_SPIKE_IN_BLOCK, in_too));
    CHECK(r.hints == 2 && r.warnings == 0);
    CHECK(ed_issue_is_hint(ED_HINT_SPIKE_IN_BLOCK));
    ed_report_free(&r);
    ed_level_free(&lv);
}

static int add_block_at(ed_level_t *lv, double x, double y, int turn)
{
    char line[96];

    snprintf(line, sizeof(line), "block %.10g %.10g 2 rot=%d", x, y, turn);
    return add(lv, line);
}

/* The bottom right corner of a hitbox. */
static vec2_t low_right(const hitbox_t *h)
{
    vec2_t corner = {-1e9, -1e9};

    for (int v = 0; v < h->nverts; v++)
        if (h->verts[v].y > corner.y || (h->verts[v].y == corner.y
            && h->verts[v].x > corner.x))
            corner = h->verts[v];
    return corner;
}

/*
** By the shapes, not their boxes. A block a hair into the spike's hitbox
** only touches it. And a block turned 45 degrees whose side passes just
** off the spike's corner isn't in it, though their boxes cross: only the
** block's own side tells them apart. A narrow spike begins after such a
** block, a wide one before it: either may be the one on the left.
*/
static void test_spike_by_shape(void)
{
    static const char *const SPIKES[] = {"spike 1500 400 2",
        "spike 1500 400 2 w=6"};
    ed_level_t lv;
    ed_report_t r = {0};

    for (int k = 0; k < 4; k++) {
        double side = k % 2 == 0 ? -3.0 : 3.0;       /* over it, or off it */
        hitbox_t h;
        vec2_t corner;
        int block;

        ed_level_new(&lv, "1");
        h = ed_find(&lv, add(&lv, SPIKES[k / 2]))->obj.hitbox;
        corner = low_right(&h);
        block = add_block_at(&lv, corner.x - 100.0, corner.y - 0.5, 0);
        ed_check(&lv, &r);
        CHECK(count_of(&r, ED_HINT_SPIKE_IN_BLOCK) == 0);    /* half a pixel */
        ed_remove(&lv, block);
        block = add_block_at(&lv, corner.x - 100.0, corner.y - 2.0, 0);
        ed_check(&lv, &r);
        CHECK(count_of(&r, ED_HINT_SPIKE_IN_BLOCK) == 1);
        ed_remove(&lv, block);
        block = add_block_at(&lv, corner.x + 25.0 * sqrt(2.0) + side - 50.0,
            corner.y + 25.0 * sqrt(2.0) + side - 50.0, 45);
        CHECK((ed_find(&lv, block)->obj.hitbox.aabb.x < h.aabb.x) == (k < 2));
        ed_check(&lv, &r);
        CHECK(count_of(&r, ED_HINT_SPIKE_IN_BLOCK) == (side < 0 ? 1u : 0u));
        ed_level_free(&lv);
    }
    ed_report_free(&r);
}

static void test_last_portal(void)
{
    ed_level_t lv;
    ed_report_t r = {0};
    int ship;
    int wave;
    int cube;

    ed_level_new(&lv, "1");
    add(&lv, "gravity 9000 650 2 up");           /* not a mode portal */
    ship = add(&lv, "portal 2000 650 2 ship");
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_HINT_ENDS_IN_MODE) == 1);
    CHECK(flagged(&r, ED_HINT_ENDS_IN_MODE, ship));
    wave = add(&lv, "portal 3000 650 2 wave");
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_HINT_ENDS_IN_MODE) == 1);   /* only the last */
    CHECK(flagged(&r, ED_HINT_ENDS_IN_MODE, wave));
    cube = add(&lv, "portal 4000 650 2 cube");
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_HINT_ENDS_IN_MODE) == 0);
    ed_remove(&lv, cube);
    add(&lv, "portal 1000 650 2 cube");          /* a cube portal before */
    ed_check(&lv, &r);
    CHECK(count_of(&r, ED_HINT_ENDS_IN_MODE) == 1);
    ed_report_free(&r);
    ed_level_free(&lv);
}

/* From left to right, whatever the order the objects were placed in. */
static void test_order(void)
{
    ed_level_t lv;
    ed_report_t r = {0};
    int far;
    int near;
    int mid;

    ed_level_new(&lv, "1");
    far = add(&lv, "block 9000 900 2");
    near = add(&lv, "block 1000 900 2");
    mid = add(&lv, "block 5000 900 2 rot=60");
    ed_check(&lv, &r);
    CHECK(r.count == 4 && r.warnings == 4);
    CHECK(r.issues[0].id == near && r.issues[1].id == mid);
    CHECK(r.issues[1].kind == ED_ISSUE_UNDERGROUND);
    CHECK(r.issues[2].id == mid && r.issues[2].kind == ED_ISSUE_STEEP);
    CHECK(r.issues[3].id == far);
    for (int k = 0; k < ED_ISSUE_KIND_COUNT; k++)
        CHECK(ed_issue_text((ed_issue_kind_t)k)[0] != '\0');
    ed_report_free(&r);
    ed_level_free(&lv);
}

/* The level against its song: what is left of it when the song is over. */
static void test_song_length(void)
{
    song_t songs[2] = {{.file = "a.ogg", .title = "A", .duration = 10.0f},
        {.file = "b.ogg", .title = "B", .duration = 60.0f,
            .default_offset = 55.0f}};
    library_t lib = {songs, 2};
    ed_level_t lv;
    double end = PLAYER_SPAWN_X + 12.0 * SCROLL_SPEED;
    double fast = 12.0 * SPEED_NORMAL / SPEED_XFAST;         /* 6.49 s */

    ed_level_new(&lv, "1");
    CHECK(fabs(ed_level_seconds(&lv, end) - 12.0) < 1e-9);
    CHECK(ed_song_short_by(&lv, &lib, end) == 0.0);          /* no song */
    snprintf(lv.hdr.music, sizeof(lv.hdr.music), "a.ogg");
    CHECK(fabs(ed_song_short_by(&lv, &lib, end) - 2.0) < 1e-6);
    lv.hdr.music_offset = 3.0;                               /* less song left */
    CHECK(fabs(ed_song_short_by(&lv, &lib, end) - 5.0) < 1e-6);
    lv.hdr.start.speed = 4.0;                                /* a shorter level */
    CHECK(fabs(ed_level_seconds(&lv, end) - fast) < 1e-9);
    CHECK(ed_song_short_by(&lv, &lib, end) == 0.0);
    lv.hdr.start.speed = 1.0;
    lv.hdr.music_offset = 0.0;
    snprintf(lv.hdr.music, sizeof(lv.hdr.music), "b.ogg");   /* its own offset */
    CHECK(fabs(ed_song_short_by(&lv, &lib, end) - 7.0) < 1e-6);
    ed_level_free(&lv);
}

static bool text_starts(const ed_verify_t *v, const char *want)
{
    char text[160];

    ed_verify_text(v, text, sizeof(text));
    return strncmp(text, want, strlen(want)) == 0;
}

/* Left to right from the start, never more than a few ticks apart. */
static bool is_a_run(const ed_verify_t *v, const ed_level_t *lv)
{
    if (v->path_len < 2 || v->path[0].x != lv->hdr.start.pos.x
        || v->path[0].y != lv->hdr.start.pos.y)
        return false;
    for (size_t i = 1; i < v->path_len; i++)
        if (v->path[i].x <= v->path[i - 1].x || v->path[i].x
            > v->path[i - 1].x + ED_VERIFY_EVERY * PER_TICK(SPEED_NORMAL) + 1e-6)
            return false;
    return v->path[v->path_len - 1].x == v->end.x
        && v->path[v->path_len - 1].y == v->end.y;
}

/* A path found: the line goes over the spike and on to the level's end. */
static void test_verify_found(void)
{
    ed_level_t lv;
    ed_verify_t v = {0};
    double end_x;
    double kill_y;
    bool jumped = false;

    ed_level_new(&lv, "1");
    add(&lv, "spike 1500 750 2");
    CHECK(!ed_verify_running(&v) && text_starts(&v, ""));
    ed_verify_begin(&v, &lv);
    CHECK(ed_verify_running(&v) && !v.done);
    CHECK(text_starts(&v, "Verifying: 0 runs"));
    CHECK(!ed_verify_step(&v, 1, ED_VERIFY_MAX));        /* the run that dies */
    CHECK(ed_verify_running(&v) && text_starts(&v, "Verifying: 1 runs"));
    while (!ed_verify_step(&v, 1, ED_VERIFY_MAX));
    CHECK(!ed_verify_running(&v) && v.done && v.sim == NULL);
    CHECK(ed_verify_step(&v, 1, ED_VERIFY_MAX));         /* over stays over */
    CHECK(v.verdict == BOT_FOUND && v.attempts > 1 && v.furthest == 100.0f);
    CHECK(text_starts(&v, "Bot found a path"));
    ed_level_measure(&lv, &end_x, &kill_y);
    CHECK(is_a_run(&v, &lv) && v.end.x >= end_x);
    CHECK(v.end.x < end_x + PER_TICK(SPEED_NORMAL) + 1e-6);
    for (size_t i = 0; i < v.path_len; i++)
        if (v.path[i].x > 1500.0 && v.path[i].x < 1600.0)
            jumped = jumped || v.path[i].y < 750.0;
    CHECK(jumped);
    CHECK(lv.count == 1 && !lv.objects[0].selected);     /* a copy was run */
    ed_verify_free(&v);
    CHECK(v.path == NULL && !v.done);
    ed_level_free(&lv);
}

/*
** No path: the line is the run that went furthest, and it stops at the wall
** nothing gets over. Undecided: the same, when the runs ran out first.
*/
static void test_verify_no_path(void)
{
    ed_level_t lv;
    ed_verify_t v = {0};

    ed_level_new(&lv, "1");
    add(&lv, "block 1500 550 2 w=2 h=6");
    ed_verify_begin(&v, &lv);
    while (!ed_verify_step(&v, 7, ED_VERIFY_MAX));
    CHECK(v.verdict == BOT_NO_PATH && v.furthest > 0.0f && v.furthest < 100.0f);
    CHECK(text_starts(&v, "Bot found no path: furthest"));
    CHECK(is_a_run(&v, &lv));
    CHECK(v.end.x > 1400.0 && v.end.x < 1500.0);        /* its side meets the wall */
    ed_verify_begin(&v, &lv);                            /* again: from nothing */
    CHECK(ed_verify_running(&v) && !v.done && v.path == NULL);
    CHECK(ed_verify_step(&v, 100, 2));                   /* two runs allowed */
    CHECK(v.verdict == BOT_GAVE_UP && v.attempts == 2);
    CHECK(text_starts(&v, "Undecided"));
    CHECK(is_a_run(&v, &lv) && v.end.x > 1400.0);
    ed_verify_begin(&v, &lv);
    ed_verify_free(&v);                                  /* stopped half way */
    CHECK(!ed_verify_running(&v) && !v.done);
    ed_level_free(&lv);
}

static void set_age(const char *path, long seconds_ago)
{
    struct timespec t[2];

    clock_gettime(CLOCK_REALTIME, &t[0]);
    t[0].tv_sec -= seconds_ago;
    t[1] = t[0];
    utimensat(AT_FDCWD, path, t, 0);
}

/* Beside the level's file, never over it, and the level stays changed. */
static void test_autosave(void)
{
    static const char DIR[] = "/tmp/mygd_test_autosave/deeper";
    static const char FILE_[] = "/tmp/mygd_test_autosave/deeper/990091.gd";
    static const char LEVEL[] = "/tmp/990091.gd";
    ed_level_t lv;
    char path[128];
    char *before;
    char *after;
    time_t when = 0;

    remove(FILE_);
    remove(LEVEL);
    ed_autosave_path(DIR, "990091", path, sizeof(path));
    CHECK(strcmp(path, FILE_) == 0);
    ed_level_new(&lv, "990091");
    CHECK(!ed_autosave_pending(DIR, "990091", LEVEL, &when));
    CHECK(ed_autosave_restore(&lv, DIR) != 0 && lv.count == 0);
    CHECK(ed_level_save(&lv, LEVEL) == 0);       /* the empty level, saved */
    add(&lv, "block 1500 750 2");
    add(&lv, "spike 1700 750 2 rot=90");
    CHECK(ed_autosave_write(&lv, DIR) == 0 && lv.dirty);
    before = ed_level_text(&lv);
    set_age(LEVEL, 100);
    set_age(FILE_, 50);
    CHECK(ed_autosave_pending(DIR, "990091", LEVEL, &when));
    CHECK(when > 0 && time(NULL) - when >= 49 && time(NULL) - when <= 52);
    set_age(LEVEL, 10);                          /* saved since: the file wins */
    CHECK(!ed_autosave_pending(DIR, "990091", LEVEL, NULL));
    remove(LEVEL);                               /* a level never saved */
    CHECK(ed_autosave_pending(DIR, "990091", LEVEL, NULL));
    ed_level_free(&lv);
    ed_level_new(&lv, "990091");
    lv.dirty = false;
    CHECK(ed_autosave_restore(&lv, DIR) == 0);
    after = ed_level_text(&lv);
    CHECK(strcmp(before, after) == 0 && lv.count == 2 && lv.dirty);
    CHECK(strcmp(lv.id, "990091") == 0);
    ed_autosave_remove(DIR, "990091");
    CHECK(!ed_autosave_pending(DIR, "990091", LEVEL, NULL));
    ed_autosave_remove(DIR, "990091");           /* none: nothing happens */
    free(before);
    free(after);
    ed_level_free(&lv);
    rmdir(DIR);
    rmdir("/tmp/mygd_test_autosave");
}

void test_edcheck(void)
{
    test_clean();
    test_twice();
    test_out_of_reach();
    test_steep();
    test_spike_in_block();
    test_spike_by_shape();
    test_last_portal();
    test_order();
    test_song_length();
    test_verify_found();
    test_verify_no_path();
    test_autosave();
}
