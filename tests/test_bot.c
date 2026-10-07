/*
** ALEXNEX PROJECT, 2026
** tests/test_bot.c
** File description:
** the bot: its verdicts, and that a path it finds replays from tick 0 (8.3)
*/

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/bot.h"
#include "sim/level.h"
#include "sim/sim.h"
#include "test.h"

static void load(sim_t *s, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

/*
** The found inputs, from a fresh reset with no snapshot: the run must end
** complete on exactly its last tick, so restoring snapshots changed nothing.
*/
static bool replays(sim_t *s, const bot_result_t *r)
{
    bool prev = false;

    sim_reset(s);
    for (long t = 0; t < r->ticks; t++) {
        sim_tick(s, (input_t){r->inputs[t], r->inputs[t] && !prev});
        prev = r->inputs[t];
        if (!s->st.player.alive || (s->st.complete && t + 1 < r->ticks))
            return false;
    }
    return s->st.complete;
}

static void test_empty_level(void)
{
    sim_t s;
    bot_result_t r;

    load(&s, "");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && r.attempts == 1);
    CHECK(r.furthest == 100.0f);
    CHECK(replays(&s, &r));
    bot_result_free(&r);
    sim_free(&s);
}

static void test_spike_needs_a_jump(void)
{
    sim_t s;
    bot_result_t r;

    load(&s, "spike 1500 750 2\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && r.attempts > 1);
    CHECK(s.st.tick == 0 && s.st.player.alive);   /* the sim is left reset */
    CHECK(replays(&s, &r));
    bot_result_free(&r);
    r = bot_solve(&s, 1);                      /* the cap: one run, it dies */
    CHECK(r.verdict == BOT_GAVE_UP && r.attempts == 1 && r.inputs == NULL);
    CHECK(r.furthest > 0.0f && r.furthest < 100.0f);
    sim_free(&s);
}

/* 300 px of wall: above the cube's 213 px jump, too tall to step onto. */
static void test_wall_too_tall(void)
{
    sim_t s;
    bot_result_t r;

    load(&s, "block 1500 550 2 w=2 h=6\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_NO_PATH && r.inputs == NULL);
    CHECK(r.furthest > 0.0f && r.furthest < 100.0f);
    sim_free(&s);
}

/* The same wall, for a ship: it decides every 12 ticks and flies over. */
static void test_ship_flies_over(void)
{
    sim_t s;
    bot_result_t r;

    load(&s, "start_gamemode ship\nblock 1500 550 2 w=2 h=6\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND);
    CHECK(replays(&s, &r));
    bot_result_free(&r);
    sim_free(&s);
}

/*
** Up through a gravity portal, a jump off the ceiling over a hanging spike,
** back down, a jump on the ground (FEATURES 9.4). Without the ceiling there
** is nothing to do: every run ends at the kill ceiling.
*/
static void test_gravity_portals(void)
{
    sim_t s;
    bot_result_t r;

    load(&s, "gravity 1000 700 2 up\nblock 1100 300 2 w=60 h=2\n"
        "spike 2500 400 2 rot=180\ngravity 3800 400 2 down\n"
        "spike 5000 750 2\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && r.attempts > 2);
    CHECK(replays(&s, &r));
    bot_result_free(&r);
    sim_free(&s);
    load(&s, "gravity 1000 700 2 up\nspike 5000 750 2\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_NO_PATH && r.furthest < 100.0f);
    bot_result_free(&r);
    sim_free(&s);
}

/*
** What the found run did as `mode`: the ticks it spent in it and the times
** its gravity turned there. A level meant for a mode proves nothing when the
** path walks around the portal.
*/
static void run_as(sim_t *s, const bot_result_t *r, gamemode_t mode,
    long *ticks, int *flips)
{
    bool prev = false;
    int dir;

    *ticks = 0;
    *flips = 0;
    sim_reset(s);
    for (long t = 0; t < r->ticks; t++) {
        dir = s->st.player.gravity_dir;
        sim_tick(s, (input_t){r->inputs[t], r->inputs[t] && !prev});
        prev = r->inputs[t];
        if (s->st.player.mode != mode)
            continue;
        *ticks += 1;
        *flips += dir != s->st.player.gravity_dir;
    }
}

/*
** FEATURES 7.5: a 300 px wall takes two hops, then a spike, then a cube
** again. The same level without the UFO portal is the cube's too tall wall.
*/
static void test_ufo_level(void)
{
    sim_t s;
    bot_result_t r;
    long ticks;
    int flips;

    load(&s, "portal 1500 650 2 ufo\nblock 2300 550 2\nblock 2300 650 2\n"
        "block 2300 750 2\nspike 2900 750 2\nportal 3600 650 2 cube\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && r.attempts > 2);
    CHECK(replays(&s, &r));
    run_as(&s, &r, MODE_UFO, &ticks, &flips);
    CHECK(ticks > 400);
    bot_result_free(&r);
    sim_free(&s);
    load(&s, "block 2300 550 2\nblock 2300 650 2\n"
        "block 2300 750 2\nspike 2900 750 2\nportal 3600 650 2 cube\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_NO_PATH);
    bot_result_free(&r);
    sim_free(&s);
}

/*
** The wave: over a floor block, under a ceiling block, over a taller one.
** The portal stands on the ground, so the path has to take it. FEATURES 8.6
** as written is no test of the wave: its portal is high enough for a cube
** to pass under, and a cube then jumps the spike and walks to the end.
*/
static void test_wave_level(void)
{
    sim_t s;
    bot_result_t r;
    long ticks;
    int flips;

    load(&s, "portal 1500 650 2 wave\nblock 2300 650 2 w=4 h=4\n"
        "block 3100 -150 2 w=4 h=14\nblock 3900 450 2 w=4 h=8\n"
        "portal 4700 -200 2 cube\nportal 4700 100 2 cube\n"
        "portal 4700 400 2 cube\nportal 4700 700 2 cube\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && r.attempts > 2);
    CHECK(replays(&s, &r));
    run_as(&s, &r, MODE_WAVE, &ticks, &flips);
    CHECK(ticks > 600);                        /* from one portal to the other */
    bot_result_free(&r);
    sim_free(&s);
}

/*
** FEATURES 9.5, its portals on the ground so the path has to take them: a
** floor spike, a ceiling spike, a floor spike, one flip for each.
*/
static void test_ball_level(void)
{
    sim_t s;
    bot_result_t r;
    long ticks;
    int flips;

    load(&s, "portal 1500 650 2 ball\nspike 2200 750 2\n"
        "spike 2800 0 2 rot=180\nspike 3400 750 2\nportal 4200 650 2 cube\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && r.attempts > 2);
    CHECK(replays(&s, &r));
    run_as(&s, &r, MODE_BALL, &ticks, &flips);
    CHECK(ticks > 500 && flips >= 3);
    bot_result_free(&r);
    sim_free(&s);
}

/* After a replay: whether the run used the level's first object of a type. */
static bool used(const sim_t *s, obj_type_t type)
{
    for (size_t i = 0; i < s->lvl.nb_objects; i++)
        if (s->lvl.objects[i].type == type)
            return is_spent(&s->st, i);
    return false;
}

/*
** A wall 400 px tall behind a yellow pad: the pad's 4.38 blocks clear it,
** and without the pad nothing does (FEATURES 10.1).
*/
static void test_pad_level(void)
{
    sim_t s;
    bot_result_t r;

    load(&s, "pad 1400 750 2 yellow\nblock 1700 450 2 h=8\n"
        "spike 2600 750 2\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND);
    CHECK(replays(&s, &r) && used(&s, OBJ_PAD));
    bot_result_free(&r);
    sim_free(&s);
    load(&s, "block 1700 450 2 h=8\nspike 2600 750 2\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_NO_PATH);
    bot_result_free(&r);
    sim_free(&s);
}

/*
** Six spikes in a row, too wide for a jump, and an orb above them: the cube
** has to click it in the air, where it has no other choice to make. The
** search only finds that because every tick near a live orb is a decision,
** releases included (FEATURES 10.2).
*/
static void test_orb_level(void)
{
    sim_t s;
    bot_result_t r;

    load(&s, "spike 1500 750 2\nspike 1600 750 2\nspike 1700 750 2\n"
        "spike 1800 750 2\nspike 1900 750 2\nspike 2000 750 2\n"
        "orb 1750 550 2 yellow\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && r.attempts > 2);
    CHECK(replays(&s, &r) && used(&s, OBJ_ORB));
    bot_result_free(&r);
    sim_free(&s);
    load(&s, "spike 1500 750 2\nspike 1600 750 2\nspike 1700 750 2\n"
        "spike 1800 750 2\nspike 1900 750 2\nspike 2000 750 2\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_NO_PATH);
    bot_result_free(&r);
    sim_free(&s);
}

static int numeric_name(const struct dirent **a, const struct dirent **b)
{
    long x = strtol((*a)->d_name, NULL, 10);
    long y = strtol((*b)->d_name, NULL, 10);

    return (x > y) - (x < y);
}

/* What the level list shows: the loader's own rule (7.2). */
static int is_level(const struct dirent *e)
{
    char id[LEVEL_ID_MAX + 1];

    return level_id_from_path(e->d_name, id, sizeof(id));
}

/*
** Every level of levels/: the verdict is printed, never checked (8.3). What
** is checked is that a path it found replays.
*/
static void test_levels(void)
{
    struct dirent **names = NULL;
    int n = scandir("levels", &names, is_level, numeric_name);
    char path[300];
    char line[128];
    sim_t s;
    bot_result_t r;

    CHECK(n > 0);                /* levels/ not found: run from the repository */
    printf("bot, information only:\n");
    for (int i = 0; i < n; i++) {
        snprintf(path, sizeof(path), "levels/%s", names[i]->d_name);
        free(names[i]);
        if (sim_load(&s, path, NULL) != 0) {
            CHECK(!"a level the list shows doesn't load");
            continue;
        }
        r = bot_solve(&s, BOT_MAX_ATTEMPTS);
        bot_describe(&r, line, sizeof(line));
        printf("  %-16s %s\n", path, line);
        if (r.verdict == BOT_FOUND)
            CHECK(replays(&s, &r));
        bot_result_free(&r);
        sim_free(&s);
    }
    free(names);
}

void test_bot(void)
{
    test_empty_level();
    test_spike_needs_a_jump();
    test_wall_too_tall();
    test_ship_flies_over();
    test_gravity_portals();
    test_ufo_level();
    test_wave_level();
    test_ball_level();
    test_pad_level();
    test_orb_level();
    test_levels();
}
