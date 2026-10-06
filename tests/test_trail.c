/*
** ALEXNEX PROJECT, 2026
** tests/test_trail.c
** File description:
** the wave's trail: which corners it keeps (FEATURES 8.5)
*/

#include <math.h>
#include <string.h>

#include "fx/trail.h"
#include "sim/sim.h"
#include "test.h"

#define NONE ((input_t){false, false})
#define HELD ((input_t){true, false})

#define WAVE "start_gamemode wave\nstart_y 400\n"
#define FAR "block 30000 750 2\n"
#define VX (PER_TICK(SCROLL_SPEED))

static void load(sim_t *s, trail_t *t, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
    trail_reset(t, &s->st.player);
}

static void run(sim_t *s, trail_t *t, int ticks, input_t in)
{
    for (int i = 0; i < ticks; i++) {
        sim_tick(s, in);
        trail_after_tick(t, &s->st.player);
    }
}

/* Only the turns are kept, each where the tick that turned started. */
static void test_corners(void)
{
    sim_t s;
    trail_t t;
    vec2_t at;

    load(&s, &t, WAVE FAR);
    CHECK(t.n == 1 && t.pts[0].x == PLAYER_SPAWN_X && t.pts[0].y == 400.0);
    run(&s, &t, 30, HELD);
    CHECK(t.n == 1);                           /* a straight line: no corner */
    at = s.st.player.pos;
    run(&s, &t, 1, NONE);
    CHECK(t.n == 2 && t.pts[1].x == at.x && t.pts[1].y == at.y);
    run(&s, &t, 20, NONE);
    CHECK(t.n == 2);
    at = s.st.player.pos;
    run(&s, &t, 5, HELD);
    CHECK(t.n == 3 && t.pts[2].x == at.x && t.pts[2].y == at.y);
    sim_free(&s);
}

/* Reaching a surface and leaving it are corners; sliding along it isn't. */
static void test_surfaces(void)
{
    sim_t s;
    trail_t t;

    load(&s, &t, WAVE FAR);
    run(&s, &t, 300, NONE);                    /* down to the ground, then along */
    CHECK(s.st.player.grounded && t.n == 2);
    CHECK(t.pts[1].y == GROUND_Y - WAVE_HALF);
    CHECK(fabs(t.pts[1].x - t.pts[0].x - (t.pts[1].y - 400.0)) < VX);
    run(&s, &t, 300, HELD);                    /* up to the ceiling, then along */
    CHECK(s.st.player.alive && t.n == 4);
    CHECK(t.pts[2].y == GROUND_Y - WAVE_HALF);
    CHECK(fabs(t.pts[3].y - (GROUND_Y - 1000.0 + WAVE_HALF)) < 0.01);
    run(&s, &t, 50, HELD);
    CHECK(t.n == 4);                           /* the ceiling's skin isn't a turn */
    sim_free(&s);
}

/* It begins where the portal acted and ends with the mode. */
static void test_portals(void)
{
    sim_t s;
    trail_t t;

    load(&s, &t, "portal 1000 700 2 wave\nportal 2000 700 2 cube\n" FAR);
    CHECK(t.n == 0);
    while (s.st.player.mode == MODE_CUBE && s.st.tick < 2000)
        run(&s, &t, 1, NONE);
    CHECK(s.st.player.mode == MODE_WAVE);
    CHECK(t.n == 1 && t.pts[0].x == s.st.player.pos.x);
    CHECK(t.pts[0].y == s.st.player.pos.y);
    run(&s, &t, 30, HELD);
    CHECK(t.n == 1);                           /* its first line has no corner */
    run(&s, &t, 10, NONE);
    CHECK(t.n == 2);
    while (s.st.player.mode == MODE_WAVE && s.st.tick < 2000)
        run(&s, &t, 1, NONE);
    CHECK(s.st.player.mode == MODE_CUBE && t.n == 0);
    run(&s, &t, 50, NONE);
    CHECK(t.n == 0);
    sim_free(&s);
}

/* A new attempt starts a new trail; a death leaves the old one to be drawn. */
static void test_attempts(void)
{
    sim_t s;
    trail_t t;

    load(&s, &t, WAVE "block 600 0 2 h=18\n" FAR);
    run(&s, &t, 10, HELD);
    run(&s, &t, 200, NONE);
    CHECK(!s.st.player.alive && t.n == 2);
    sim_reset(&s);
    trail_reset(&t, &s.st.player);
    CHECK(t.n == 1 && t.pts[0].x == PLAYER_SPAWN_X);
    sim_free(&s);
}

/* Full, it forgets its oldest corner and keeps the order. */
static void test_capacity(void)
{
    sim_t s;
    trail_t t;

    load(&s, &t, WAVE FAR);
    for (int i = 0; i < 2 * TRAIL_CAP + 40; i++)
        run(&s, &t, 3, i % 2 == 0 ? HELD : NONE);
    CHECK(s.st.player.alive && t.n == TRAIL_CAP);
    for (size_t i = 1; i < t.n; i++)
        CHECK(t.pts[i].x > t.pts[i - 1].x);
    CHECK(fabs(t.pts[t.n - 1].x - (s.st.player.pos.x - 3.0 * VX)) < 1e-9);
    sim_free(&s);
}

/* A rounding error's worth of height along a surface is still level. */
static void test_level_is_level(void)
{
    player_t p = {.mode = MODE_WAVE, .pos = {0.0, 100.0}};
    trail_t t;

    trail_reset(&t, &p);
    for (int i = 1; i <= 20; i++) {
        p.prev_pos = p.pos;
        p.pos = (vec2_t){i * VX, 100.0 + (i % 2 == 0 ? 1e-9 : -1e-9)};
        trail_after_tick(&t, &p);
    }
    CHECK(t.n == 1);
    p.prev_pos = p.pos;
    p.pos.y -= VX;                             /* a real turn */
    trail_after_tick(&t, &p);
    CHECK(t.n == 2 && t.pts[1].x == p.prev_pos.x);
}

/* Left of the view only one corner stays: the line into the screen needs it. */
static void test_drop(void)
{
    trail_t t = {.n = 5, .pts = {{100.0, 1.0}, {200.0, 2.0}, {300.0, 3.0},
        {400.0, 4.0}, {500.0, 5.0}}};

    trail_drop_left_of(&t, 50.0);
    CHECK(t.n == 5);
    trail_drop_left_of(&t, 150.0);             /* 100 is the line's start */
    CHECK(t.n == 5 && t.pts[0].x == 100.0);
    trail_drop_left_of(&t, 350.0);
    CHECK(t.n == 3 && t.pts[0].x == 300.0 && t.pts[0].y == 3.0);
    CHECK(t.pts[2].x == 500.0);
    trail_drop_left_of(&t, 9000.0);            /* all behind: the last stays */
    CHECK(t.n == 1 && t.pts[0].x == 500.0);
    trail_drop_left_of(&t, 9000.0);
    CHECK(t.n == 1);
    t.n = 0;
    trail_drop_left_of(&t, 9000.0);
    CHECK(t.n == 0);
}

void test_trail(void)
{
    test_corners();
    test_surfaces();
    test_portals();
    test_attempts();
    test_capacity();
    test_level_is_level();
    test_drop();
}
