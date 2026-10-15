/*
** ALEXNEX PROJECT, 2026
** tests/test_speed.c
** File description:
** speed portals, GD's five speeds, and the time an x is reached at (FEATURES 10.3)
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor/ed_music.h"
#include "editor/ed_props.h"
#include "music/library.h"
#include "sim/bot.h"
#include "sim/internal.h"
#include "sim/level.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "sim/speed.h"
#include "test.h"

#define NONE ((input_t){false, false})
#define PRESS ((input_t){true, true})
#define HELD ((input_t){true, false})
#define FAR "block 60000 750 2\n"             /* a level long enough  */

static int warnings;

static void count_warning(const char *msg)
{
    (void)msg;
    warnings += 1;
}

static void load(sim_t *s, const char *text)
{
    warnings = 0;
    sim_load_mem(s, text, strlen(text), "1", count_warning);
}

/* The names a file writes, and GD's speeds behind them: not ratios. */
static void test_table(void)
{
    static const double PX_S[SPEED_COUNT] = {837.2, 1038.6, 1291.4, 1560.0,
        1920.0};
    static const char *const WORDS[SPEED_COUNT] = {"0.5", "1", "2", "3", "4"};

    for (int i = 0; i < SPEED_COUNT; i++) {
        CHECK(speed_is_known(SPEED_NAMES[i]));
        CHECK(fabs(speed_px_per_s(SPEED_NAMES[i]) - PX_S[i]) < 1e-9);
        CHECK(speed_step(SPEED_NAMES[i]) == PER_TICK(speed_px_per_s(
            SPEED_NAMES[i])));
        CHECK(strcmp(speed_word(SPEED_NAMES[i]), WORDS[i]) == 0);
    }
    CHECK(speed_step(1.0) == 4.3275 && speed_step(4.0) == 8.0);
    CHECK(!speed_is_known(0.0) && !speed_is_known(1.5) && !speed_is_known(5.0));
    CHECK(speed_px_per_s(7.0) == SPEED_NORMAL);       /* unknown: the normal */
    CHECK(strcmp(speed_word(7.0), "1") == 0);
}

/* The line, its box (a portal's, around the cell's center), what is skipped. */
static void test_grammar(void)
{
    sim_t s;
    const object_t *o;
    level_doc_t doc;
    level_header_t hdr;
    char *text;

    load(&s, "speed 1500 650 2 3\nspeed 2500 650 2 0.5\nspeed 3500 650 2\n"
        "speed 4500 650 2 7\nspeed 5500 650 2 fast\nspeed 6500 650 2 1.0\n");
    CHECK(s.lvl.nb_objects == 3 && s.lvl.skipped_lines == 3 && warnings == 3);
    o = &s.lvl.objects[0];
    CHECK(o->type == OBJ_SPEED && o->speed == 3.0);
    CHECK(OBJ_CATEGORY[OBJ_SPEED] == CAT_INTERACTIVE);
    CHECK(o->rect.w == 120.0 && o->rect.h == 280.0);
    CHECK(o->rect.x == 1490.0 && o->rect.y == 560.0);  /* centered on its cell */
    CHECK(o->hitbox.kind == SHAPE_POLY && o->hitbox.aabb.x == 1490.0);
    CHECK(s.lvl.objects[1].speed == 0.5 && s.lvl.objects[2].speed == 1.0);
    CHECK(s.lvl.speeds.n == 3 && s.lvl.speeds.at[0].x == 1490.0 - PLAYER_HALF);
    sim_free(&s);
    text = "speed 1500 650 2 3\nspeed 2500 650 2 0.5 rot=90\n";
    level_header_defaults(&hdr, "1");
    level_parse_doc(text, strlen(text), "1", &doc, &hdr, NULL);
    text = level_write_mem(&doc, &hdr);
    CHECK(strstr(text, "speed 1500 650 2 3\n") != NULL);
    CHECK(strstr(text, "speed 2500 650 2 0.5 rot=90\n") != NULL);
    free(text);
    level_doc_free(&doc);
}

/* Ticks until object `i` is spent; -1 if the run ends first. */
static long run_until_spent(sim_t *s, size_t i, input_t in)
{
    for (long t = 0; t < 20000 && s->st.player.alive && !s->st.complete; t++) {
        sim_tick(s, in);
        if (is_spent(&s->st, i))
            return t;
    }
    return -1;
}

/*
** Taken once, the tick the player's side reaches it; the speed is then its
** own until the next one, and a second touch of the first does nothing.
*/
static void test_taken(void)
{
    sim_t s;
    double x;

    load(&s, "speed 1500 650 2 3\nspeed 3000 650 2 0.5\nspeed 4500 650 2 4\n"
        FAR);
    CHECK(s.st.player.vx == 4.3275 && s.st.speed_mult == 1.0);
    CHECK(run_until_spent(&s, 0, NONE) > 0);
    x = s.st.player.pos.x;
    CHECK(x + PLAYER_HALF > 1490.0 && x + PLAYER_HALF <= 1490.0 + 4.3275);
    CHECK(s.st.player.vx == 6.5);                      /* 1560 px/s */
    CHECK(s.st.speed_mult == SPEED_VFAST / SPEED_NORMAL);
    CHECK(!is_spent(&s.st, 1));
    sim_tick(&s, NONE);
    CHECK(s.st.player.pos.x == x + 6.5);               /* from the next tick */
    CHECK(s.st.distance == s.st.player.pos.x - PLAYER_SPAWN_X);
    for (int i = 0; i < 20; i++)
        sim_tick(&s, NONE);                            /* still inside it */
    CHECK(s.st.player.vx == 6.5 && s.st.player.pos.x < 1700.0);
    CHECK(run_until_spent(&s, 1, NONE) > 0);
    CHECK(s.st.player.vx == PER_TICK(SPEED_SLOW));
    CHECK(run_until_spent(&s, 2, NONE) > 0);
    CHECK(s.st.player.vx == 8.0 && s.st.player.alive);
    sim_reset(&s);                                     /* a new attempt: normal */
    CHECK(s.st.player.vx == 4.3275 && s.st.speed_mult == 1.0);
    CHECK(!is_spent(&s.st, 0));
    sim_free(&s);
}

/* How high a cube's jump goes, and how far it lands, from this start. */
static void jump_of(const char *text, double *rise, double *length)
{
    sim_t s;
    double from;

    load(&s, text);
    for (int i = 0; i < 40; i++)
        sim_tick(&s, NONE);
    from = s.st.player.pos.x;
    *rise = 0.0;
    sim_tick(&s, PRESS);
    while (!s.st.player.grounded && s.st.player.alive) {
        *rise = fmax(*rise, PLAYER_SPAWN_Y - s.st.player.pos.y);
        sim_tick(&s, NONE);
    }
    *length = s.st.player.pos.x - from;
    sim_free(&s);
}

/*
** Only the ground covered changes: a jump is as high at any speed, since the
** jump's numbers are tied to the normal speed (PLAN 3.2), and as long in
** time, so its length follows the speed.
*/
static void test_jump_height(void)
{
    double rise[2];
    double length[2];

    jump_of(FAR, &rise[0], &length[0]);
    jump_of("start_speed 4\n" FAR, &rise[1], &length[1]);
    CHECK(rise[0] > 200.0 && rise[0] == rise[1]);
    CHECK(fabs(length[1] / length[0] - SPEED_XFAST / SPEED_NORMAL) < 1e-9);
    jump_of("speed 350 750 2 4\n" FAR, &rise[1], &length[1]);   /* by a portal */
    CHECK(rise[0] == rise[1]);
    CHECK(fabs(length[1] / length[0] - SPEED_XFAST / SPEED_NORMAL) < 1e-9);
}

/* The wave's slope stays 1 after a portal; a spike isn't flown through. */
static void test_other_modes(void)
{
    sim_t s;
    double x;
    double y;

    load(&s, "start_gamemode wave\nstart_y 400\nspeed 500 400 2 4\n" FAR);
    CHECK(run_until_spent(&s, 0, NONE) >= 0);          /* sinking into it */
    sim_tick(&s, HELD);
    x = s.st.player.pos.x;
    y = s.st.player.pos.y;
    sim_tick(&s, HELD);
    CHECK(s.st.player.vx == 8.0 && s.st.player.vy == 8.0);
    CHECK(s.st.player.pos.x - x == 8.0 && y - s.st.player.pos.y == 8.0);
    sim_free(&s);
    load(&s, "speed 600 650 2 4\nspike 3003 750 2\n" FAR);
    for (int i = 0; i < 2000 && s.st.player.alive; i++)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.alive);                         /* 8 px a tick, 26 wide */
    CHECK(s.st.player.pos.x < 3000.0);
    sim_free(&s);
}

/*
** The time an x is reached at, against a run: on flat ground, at every tick,
** the map's answer for where the player is is that tick, to within the one
** tick a portal's touch takes to act. And back: the x of that many ticks.
*/
static void test_map_against_a_run(void)
{
    sim_t s;
    const speed_map_t *m;
    const level_start_t *st;
    double worst = 0.0;

    load(&s, "speed 1500 650 2 3\nspeed 3000 650 2 0.5\nspeed 4500 650 2 4\n"
        "speed 9000 650 2 2\nspeed 12000 650 2 1\nblock 20000 900 2\n");
    m = &s.lvl.speeds;
    st = &s.lvl.hdr.start;
    CHECK(speed_map_ticks(m, st, PLAYER_SPAWN_X) == 0.0);
    CHECK(speed_map_x(m, st, 0.0) == PLAYER_SPAWN_X);
    while (!s.st.complete && s.st.player.alive) {
        double ticks;

        sim_tick(&s, NONE);
        ticks = speed_map_ticks(m, st, s.st.player.pos.x);
        worst = fmax(worst, fabs(ticks - (double)s.st.tick));
        CHECK(fabs(speed_map_x(m, st, ticks) - s.st.player.pos.x) < 1e-6);
    }
    CHECK(s.st.complete && worst < 1.0);
    CHECK(worst > 0.0);                                /* it did change speed */
    CHECK(fabs(level_duration(&s.lvl) * TICK_RATE - (double)s.st.tick) < 2.0);
    sim_free(&s);
}

/* What a run never meets doesn't count; what it starts in counts at once. */
static void test_map_edges(void)
{
    speed_map_t m = {0};
    level_start_t st = {.pos = {1000.0, 800.0}, .speed = 1.0, .gravity_dir = 1};
    object_t o;
    sim_t s;

    CHECK(fabs(speed_map_ticks(&m, &st, 1000.0 + 10.0 * 4.3275) - 10.0)
        < 1e-9);
    CHECK(speed_map_x(&m, &st, 10.0) == 1000.0 + 10.0 * 4.3275);
    CHECK(fabs(speed_map_ticks(&m, &st, 1000.0 - 4.3275) + 1.0) < 1e-9);
    level_object_from_line("block 2000 650 2", &o);
    speed_map_add(&m, &o);
    CHECK(m.n == 0);                                   /* not a speed portal */
    level_object_from_line("speed 100 650 2 3", &o);   /* wholly behind */
    speed_map_add(&m, &o);
    CHECK(m.n == 1);
    CHECK(fabs(speed_map_ticks(&m, &st, 1000.0 + 10.0 * 4.3275) - 10.0)
        < 1e-9);                                       /* never met */
    CHECK(fabs(speed_map_x(&m, &st, 10.0) - 1000.0 - 10.0 * 4.3275) < 1e-9);
    level_object_from_line("speed 5000 650 2 4", &o);  /* added out of order */
    speed_map_add(&m, &o);
    level_object_from_line("speed 950 650 2 0.5", &o); /* the start is in it */
    speed_map_add(&m, &o);
    speed_map_sort(&m);
    CHECK(m.n == 3 && m.at[0].x < m.at[1].x && m.at[1].x < m.at[2].x);
    CHECK(fabs(speed_map_ticks(&m, &st, 1000.0 + 100.0 * PER_TICK(SPEED_SLOW))
        - 100.0) < 1e-9);
    CHECK(fabs(speed_map_ticks(&m, &st, 4940.0)
        - 3940.0 / PER_TICK(SPEED_SLOW)) < 1e-9);      /* up to the next */
    CHECK(fabs(speed_map_ticks(&m, &st, 4940.0 + 80.0)
        - 3940.0 / PER_TICK(SPEED_SLOW) - 10.0) < 1e-9);    /* 8 px a tick */
    CHECK(fabs(speed_map_x(&m, &st, 3940.0 / PER_TICK(SPEED_SLOW) + 10.0)
        - 5020.0) < 1e-6);
    st.speed = 4.0;
    st.pos.x = 6000.0;                                 /* past them all */
    CHECK(speed_map_ticks(&m, &st, 6080.0) == 10.0);
    speed_map_free(&m);
    CHECK(m.at == NULL && m.n == 0);
    load(&s, "start_x 1000\nspeed 950 650 2 0.5\nspeed 100 650 2 3\n" FAR);
    sim_tick(&s, NONE);                                /* the run agrees */
    CHECK(s.st.player.vx == PER_TICK(SPEED_SLOW) && !is_spent(&s.st, 0));
    sim_free(&s);
}

/*
** A start position after speed portals: its tick is the one a run from the
** level's start reaches it at, the portals behind it are spent, and its own
** speed is what it goes on at.
*/
static void test_start_position(void)
{
    sim_t s;
    long reached = 0;
    double x;

    load(&s, "start 8000 800 cube 2\nspeed 1500 650 2 4\nspeed 5000 650 2 0.5\n"
        "speed 9000 650 2 3\nblock 20000 900 2\n");
    CHECK(s.lvl.nb_starts == 1);
    while (s.st.player.pos.x < 8000.0)
        sim_tick(&s, NONE);
    reached = s.st.tick;
    sim_reset_at(&s, &s.lvl.starts[0]);
    CHECK(labs(s.st.tick - reached) <= 2);
    CHECK(s.st.tick != (long)floor((8000.0 - PLAYER_SPAWN_X) / 4.3275));
    x = s.st.player.pos.x;
    CHECK(x <= 8000.0 && x > 8000.0 - PER_TICK(SPEED_SLOW) - 1e-6);
    CHECK(s.st.distance == x - PLAYER_SPAWN_X);
    CHECK(s.st.player.vx == PER_TICK(SPEED_FAST));     /* its own, not 0.5 */
    CHECK(is_spent(&s.st, 0) && is_spent(&s.st, 1) && !is_spent(&s.st, 2));
    CHECK(run_until_spent(&s, 2, NONE) > 0 && s.st.player.vx == 6.5);
    sim_free(&s);
}

/* Six spikes are too wide for a jump at the normal speed, and not at 4. */
static void test_speed_bot(void)
{
    static const char SPIKES[] = "spike 2500 750 2\nspike 2600 750 2\n"
        "spike 2700 750 2\nspike 2800 750 2\nspike 2900 750 2\n"
        "spike 3000 750 2\n";
    char text[512];
    sim_t s;
    bot_result_t r;

    load(&s, SPIKES);
    r = bot_solve(&s, BOT_MAX_TICKS);
    CHECK(r.verdict == BOT_NO_PATH);
    bot_result_free(&r);
    sim_free(&s);
    snprintf(text, sizeof(text), "speed 1200 650 2 4\n%s", SPIKES);
    load(&s, text);
    r = bot_solve(&s, BOT_MAX_TICKS);
    CHECK(r.verdict == BOT_FOUND && bot_replays(&s, &r) && is_spent(&s.st, 0));
    bot_result_free(&r);
    sim_free(&s);
}

static ed_entry_t *find_entry(ed_entry_t *e, int n, const char *label)
{
    for (int i = 0; i < n; i++)
        if (strcmp(e[i].label, label) == 0)
            return &e[i];
    return NULL;
}

static bool prop_is(const ed_level_t *lv, ed_prop_t p, const char *want)
{
    char text[64];

    ed_prop_text(lv, p, text, sizeof(text));
    return strcmp(text, want) == 0;
}

/* In the editor: its five entries, its Speed row, and what a save writes. */
static void test_speed_editor(void)
{
    ed_entry_t e[ED_MAX_ENTRIES];
    int n = ed_entries(e, ED_MAX_ENTRIES);
    ed_history_t h = {0};
    ed_level_t lv;
    char *text;

    ed_level_new(&lv, "1");
    for (int k = 0; k < SPEED_COUNT; k++) {
        char label[32];

        snprintf(label, sizeof(label), "speed %s", speed_word(SPEED_NAMES[k]));
        CHECK(find_entry(e, n, label) != NULL);
    }
    CHECK(ed_place(&lv, find_entry(e, n, "speed 3"), (vec2_t){1520.0, 660.0},
        50.0));
    CHECK(lv.objects[0].obj.type == OBJ_SPEED && lv.objects[0].obj.speed == 3.0);
    ed_select_all(&lv);
    CHECK(ed_prop_applies(&lv, ED_PROP_SPEED));
    CHECK(!ed_prop_applies(&lv, ED_PROP_KIND));
    CHECK(!ed_prop_applies(&lv, ED_PROP_GRAVITY));
    CHECK(prop_is(&lv, ED_PROP_SPEED, "x3"));
    ed_gesture(&h);
    CHECK(ed_prop_step(&lv, &h, ED_PROP_SPEED, 1));
    CHECK(lv.objects[0].obj.speed == 4.0 && prop_is(&lv, ED_PROP_SPEED, "x4"));
    ed_gesture(&h);
    CHECK(!ed_prop_step(&lv, &h, ED_PROP_SPEED, 1));   /* nothing is faster */
    for (int i = 0; i < 4; i++) {
        ed_gesture(&h);
        CHECK(ed_prop_step(&lv, &h, ED_PROP_SPEED, -1));
    }
    CHECK(lv.objects[0].obj.speed == 0.5 && prop_is(&lv, ED_PROP_SPEED, "x0.5"));
    CHECK(ed_undo(&lv, &h) && lv.objects[0].obj.speed == 1.0);
    text = ed_level_text(&lv);
    CHECK(strstr(text, "speed 1500 650 2 1\n") != NULL);
    free(text);
    ed_history_free(&h);
    ed_level_free(&lv);
}

static int add(ed_level_t *lv, const char *line)
{
    object_t o;

    level_object_from_line(line, &o);
    return ed_add(lv, &o, NULL);
}

/*
** The song along a level that changes speed: seconds are no longer one
** division, beats are further apart after a faster portal, and the level's
** length is what the game's own count gives.
*/
static void test_song_along(void)
{
    library_t lib = {NULL, 0};
    ed_level_t lv;
    double at = 1490.0 - PLAYER_HALF;                  /* where it is taken */
    double before = (at - PLAYER_SPAWN_X) / SPEED_NORMAL;
    double end_x;
    double kill_y;
    level_doc_t doc;
    sim_t s;

    ed_level_new(&lv, "1");
    add(&lv, "block 30000 750 2");
    CHECK(fabs(ed_time_at_x(&lv, &lib, 3000.0) - 2650.0 / SPEED_NORMAL) < 1e-9);
    add(&lv, "speed 1500 650 2 4");
    CHECK(fabs(ed_time_at_x(&lv, &lib, at) - before) < 1e-9);
    CHECK(fabs(ed_time_at_x(&lv, &lib, at + 1920.0) - before - 1.0) < 1e-9);
    CHECK(fabs(ed_x_at_time(&lv, &lib, before + 2.0) - at - 3840.0) < 1e-6);
    CHECK(fabs(ed_x_at_time(&lv, &lib, before / 2.0) - PLAYER_SPAWN_X
        - (at - PLAYER_SPAWN_X) / 2.0) < 1e-6);
    lv.hdr.bpm = 60.0;                                 /* a beat a second */
    CHECK(fabs(ed_beat_x(&lv, &lib, 1) - ed_beat_x(&lv, &lib, 0)
        - SPEED_NORMAL) < 1e-6);
    CHECK(fabs(ed_beat_x(&lv, &lib, 6) - ed_beat_x(&lv, &lib, 5) - 1920.0)
        < 1e-6);
    ed_level_measure(&lv, &end_x, &kill_y);
    ed_level_doc(&lv, &doc);
    sim_init(&s, &doc, &lv.hdr, lv.id);
    CHECK(fabs(ed_level_seconds(&lv, end_x) - level_duration(&s.lvl)) < 1e-9);
    CHECK(level_duration(&s.lvl) < (end_x - PLAYER_SPAWN_X) / SPEED_NORMAL);
    sim_free(&s);
    ed_doc_free(&doc);
    ed_level_free(&lv);
}

/*
** Portals stacked at one x: the later in the level acts last, in the run and
** in the time maths alike, whatever qsort does with ties (4.6).
*/
static void test_same_x(void)
{
    const double order[] = {2.0, 4.0, 0.5, 3.0, 2.0, 4.0, 1.0, 3.0};
    char text[512];
    size_t len = 0;
    sim_t s;
    sim_t one;

    for (int i = 0; i < 8; i++)
        len += snprintf(text + len, sizeof(text) - len, "speed 1500 650 2 %s\n",
            speed_word(order[i]));
    snprintf(text + len, sizeof(text) - len, FAR);
    load(&s, text);
    load(&one, "speed 1500 650 2 3\n" FAR);       /* the last of them alone */
    CHECK(s.lvl.speeds.n == 8);
    for (size_t i = 0; i + 1 < s.lvl.speeds.n; i++)
        CHECK(s.lvl.speeds.at[i].order < s.lvl.speeds.at[i + 1].order);
    for (int t = 0; t < 300 && s.st.player.alive; t++) {
        sim_tick(&s, NONE);
        sim_tick(&one, NONE);
        CHECK(s.st.player.pos.x == one.st.player.pos.x);
        CHECK(speed_map_x(&s.lvl.speeds, &s.lvl.hdr.start, (double)t)
            == speed_map_x(&one.lvl.speeds, &one.lvl.hdr.start, (double)t));
    }
    CHECK(s.st.player.vx == speed_step(3.0));
    sim_free(&s);
    sim_free(&one);
}

void test_speed(void)
{
    test_table();
    test_grammar();
    test_taken();
    test_jump_height();
    test_other_modes();
    test_map_against_a_run();
    test_map_edges();
    test_same_x();
    test_start_position();
    test_speed_bot();
    test_speed_editor();
    test_song_along();
}
