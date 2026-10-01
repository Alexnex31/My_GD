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

static int numeric_name(const struct dirent **a, const struct dirent **b)
{
    long x = strtol((*a)->d_name, NULL, 10);
    long y = strtol((*b)->d_name, NULL, 10);

    return (x > y) - (x < y);
}

static int is_level(const struct dirent *e)
{
    size_t n = strlen(e->d_name);

    return n > 3 && strcmp(e->d_name + n - 3, ".gd") == 0;
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

    printf("bot, information only:\n");
    for (int i = 0; i < n; i++) {
        snprintf(path, sizeof(path), "levels/%s", names[i]->d_name);
        free(names[i]);
        if (sim_load(&s, path, NULL) != 0)
            continue;
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
    test_levels();
}
