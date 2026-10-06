/*
** ALEXNEX PROJECT, 2026
** tests/test_wave.c
** File description:
** the wave: 45 degrees, no inertia, objects kill it and surfaces don't (FEATURES 8)
*/

#include <math.h>
#include <string.h>

#include "sim/internal.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

#define NONE ((input_t){false, false})
#define PRESS ((input_t){true, true})
#define HELD ((input_t){true, false})
#define TAP ((input_t){false, true})          /* down and up inside one tick */

#define WAVE "start_gamemode wave\n"
#define MID "start_y 400\n"                   /* corridor -100..900, cut at the ground */
#define FAR "block 30000 750 2\n"             /* a level long enough  */
#define VX (PER_TICK(SCROLL_SPEED))
#define ON_GROUND (GROUND_Y - WAVE_HALF)
#define CEILING (GROUND_Y - 1000.0)

static void load(sim_t *s, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

static void run(sim_t *s, int ticks, input_t in)
{
    for (int i = 0; i < ticks && s->st.player.alive; i++)
        sim_tick(s, in);
}

static void run_to_x(sim_t *s, double x, input_t in)
{
    while (s->st.player.alive && !s->st.complete && s->st.player.pos.x < x)
        sim_tick(s, in);
}

static void run_while_wave(sim_t *s, input_t in)
{
    while (s->st.player.alive && !s->st.complete
        && s->st.player.mode == MODE_WAVE)
        sim_tick(s, in);
}

/* Its row: a 30 px square that is its own inner box, a corridor, no gravity. */
static void test_row(void)
{
    const mode_ops_t *m = &MODES[MODE_WAVE];
    sim_t s;

    CHECK(m->half == 15.0 && m->inner_half == 15.0);
    CHECK(m->neutral_kills && m->keep_vy_on_surface);
    CHECK(mode_neutral_kill_half(m) == 15.0);
    CHECK(m->gravity == 0.0 && m->corridor_height == 1000.0);
    load(&s, WAVE MID FAR);
    CHECK(s.st.player.mode == MODE_WAVE && s.st.bounds.active);
    CHECK(s.st.bounds.bottom == GROUND_Y && s.st.bounds.top == CEILING);
    sim_free(&s);
}

/* 8.2: 40 held ticks are 173.1 px right and 173.1 px up, then the same down. */
static void test_45_degrees(void)
{
    sim_t s;

    load(&s, WAVE MID FAR);
    sim_tick(&s, PRESS);
    for (int i = 1; i < 40; i++) {
        sim_tick(&s, HELD);
        CHECK(s.st.player.vy == VX);
        CHECK(fabs((400.0 - s.st.player.pos.y)
            - (s.st.player.pos.x - PLAYER_SPAWN_X)) < 1e-9);
    }
    CHECK(fabs(s.st.player.pos.x - PLAYER_SPAWN_X - 173.1) < 1e-9);
    CHECK(fabs(400.0 - s.st.player.pos.y - 173.1) < 1e-9);
    CHECK(s.st.player.hold == HOLD_FRESH);     /* flying uses no hold (3.4) */
    run(&s, 40, NONE);
    CHECK(s.st.player.vy == -VX);
    CHECK(fabs(s.st.player.pos.y - 400.0) < 1e-9);
    CHECK(!s.st.player.grounded && s.st.player.alive);
    sim_free(&s);
}

/* No inertia: every tick goes the way the button is on that tick. */
static void test_no_inertia(void)
{
    sim_t s;
    double y;

    load(&s, WAVE MID FAR);
    for (int i = 0; i < 300; i++) {
        bool down = (i / 7) % 2 == 0;

        y = s.st.player.pos.y;
        sim_tick(&s, (input_t){down, down && i % 7 == 0});
        CHECK(s.st.player.pos.y == y + (down ? -VX : VX));
    }
    y = s.st.player.pos.y;
    sim_tick(&s, TAP);                         /* a tap gives one tick up */
    CHECK(s.st.player.pos.y == y - VX);
    sim_tick(&s, NONE);
    CHECK(s.st.player.pos.y == y);
    sim_free(&s);
}

/* The slope stays 1 at any horizontal speed. */
static void test_any_speed(void)
{
    sim_t s;

    load(&s, WAVE MID "start_speed 2\n" FAR);
    run(&s, 20, HELD);
    CHECK(s.st.player.vx == 2.0 * VX);
    CHECK(s.st.player.vy == s.st.player.vx);
    CHECK(fabs((400.0 - s.st.player.pos.y)
        - (s.st.player.pos.x - PLAYER_SPAWN_X)) < 1e-9);
    sim_free(&s);
}

/* Released into a block's top: a cube lands there, the wave dies touching it. */
static void test_block_top_kills(void)
{
    sim_t s;

    load(&s, WAVE MID "block 300 600 2 w=40\n" FAR);
    run(&s, 200, NONE);
    CHECK(!s.st.player.alive);
    CHECK(fabs(s.st.player.pos.y + WAVE_HALF - 600.0) < 1e-6);   /* at the touch */
    sim_free(&s);
    load(&s, MID "block 300 600 2 w=40\n" FAR);                  /* a cube */
    run(&s, 200, NONE);
    CHECK(s.st.player.alive && s.st.player.grounded);
    sim_free(&s);
}

/* Held into a block's underside, and straight into a wall. */
static void test_block_underside_and_wall(void)
{
    sim_t s;

    load(&s, WAVE MID "block 300 100 2 w=40\n" FAR);
    run(&s, 200, HELD);
    CHECK(!s.st.player.alive);
    CHECK(fabs(s.st.player.pos.y - WAVE_HALF - 200.0) < 1e-6);
    sim_free(&s);
    load(&s, WAVE "block 1000 750 2\n" FAR);
    run(&s, 400, NONE);                        /* sliding on the ground into it */
    CHECK(!s.st.player.alive);
    CHECK(fabs(s.st.player.pos.x + WAVE_HALF - 1000.0) < 1e-6);
    sim_free(&s);
}

/* A slope is an object too: no sliding up it. Spikes kill as they always do. */
static void test_slope_and_spike(void)
{
    sim_t s;

    load(&s, WAVE "slope 1000 750 2\n" FAR);
    run(&s, 400, NONE);
    CHECK(!s.st.player.alive && s.st.player.pos.x < 1100.0);
    sim_free(&s);
    load(&s, WAVE "spike 1000 760 2\n" FAR);   /* its box: 785..828 */
    run(&s, 400, NONE);
    CHECK(!s.st.player.alive && s.st.player.pos.x < 1100.0);
    sim_free(&s);
}

/*
** It is the 30 px square that decides: sliding on the ground, it passes
** under a block 31 px above the ground and dies under one 29 px above it.
*/
static void test_the_small_square(void)
{
    sim_t s;

    load(&s, WAVE "block 1000 719 2\n" FAR);   /* underside at 819 */
    run_to_x(&s, 1300.0, NONE);
    CHECK(s.st.player.alive && s.st.player.pos.y == ON_GROUND);
    sim_free(&s);
    load(&s, WAVE "block 1000 721 2\n" FAR);   /* underside at 821 */
    run_to_x(&s, 1300.0, NONE);
    CHECK(!s.st.player.alive);
    sim_free(&s);
}

/* The ground never kills: it slides, and a press leaves it on that very tick. */
static void test_slides_on_the_ground(void)
{
    sim_t s;

    load(&s, WAVE FAR);
    run(&s, 20, NONE);
    for (int i = 0; i < 200; i++) {
        sim_tick(&s, NONE);
        CHECK(s.st.player.alive && s.st.player.grounded);
        CHECK(s.st.player.pos.y == ON_GROUND);
        CHECK(s.st.player.vy == -VX);          /* kept: keep_vy_on_surface (8.3) */
    }
    sim_tick(&s, PRESS);
    CHECK(s.st.player.pos.y == ON_GROUND - VX && !s.st.player.grounded);
    sim_free(&s);
}

/* Held into the corridor's ceiling it stops and slides, and leaves at once. */
static void test_slides_on_the_ceiling(void)
{
    sim_t s;
    int ticks = 0;
    double y;

    load(&s, WAVE FAR);
    run(&s, 20, NONE);                         /* on the floor first */
    while (s.st.player.alive && ticks < 400
        && s.st.player.pos.y - WAVE_HALF > CEILING + 0.01) {
        sim_tick(&s, HELD);
        ticks += 1;
    }
    CHECK(ticks == 225);                       /* 970 px: 0.93 s (8.2) */
    for (int i = 0; i < 200; i++) {
        sim_tick(&s, HELD);
        CHECK(s.st.player.alive);
        CHECK(fabs(s.st.player.pos.y - WAVE_HALF - CEILING) < 0.01);
    }
    y = s.st.player.pos.y;
    sim_tick(&s, NONE);
    CHECK(s.st.player.pos.y == y + VX);
    sim_free(&s);
}

/* Flipped, held goes away from its floor: down the screen, as in GD. */
static void test_flipped(void)
{
    sim_t s;

    load(&s, WAVE MID "start_gravity flipped\n" FAR);
    run(&s, 40, HELD);
    CHECK(fabs(s.st.player.pos.y - 400.0 - 173.1) < 1e-9);
    run(&s, 400, NONE);                        /* up to the ceiling, its floor */
    CHECK(s.st.player.alive && s.st.player.grounded);
    CHECK(s.st.player.pos.y == CEILING + WAVE_HALF);
    run(&s, 400, HELD);                        /* down into the ground: a stop */
    CHECK(s.st.player.alive);
    CHECK(fabs(s.st.player.pos.y - ON_GROUND) < 0.01);
    sim_free(&s);
}

/* What the next mode gets from a wave going up, then from one going down. */
static void test_the_next_mode_inherits(void)
{
    sim_t s;

    load(&s, WAVE MID "portal 560 100 2 cube\n" FAR);
    run_while_wave(&s, HELD);
    CHECK(s.st.player.mode == MODE_CUBE && !s.st.bounds.active);
    CHECK(s.st.player.vy == VX);               /* a real velocity (8.1) */
    sim_tick(&s, NONE);
    CHECK(s.st.player.vy == VX - MODES[MODE_CUBE].gravity);
    sim_free(&s);
    load(&s, WAVE MID "portal 600 550 2 ufo\n" FAR);
    run_while_wave(&s, NONE);
    CHECK(s.st.player.mode == MODE_UFO && s.st.player.vy == -VX);
    sim_tick(&s, NONE);
    CHECK(s.st.player.vy == -VX - MODES[MODE_UFO].gravity);
    sim_free(&s);
}

/*
** A wave sliding on the ground is 35 px lower than a cube standing there.
** The portal puts the bigger square back on the surface, on the tick it
** acts, and never moves a player that is clear of it.
*/
static void test_growing_on_the_ground(void)
{
    sim_t s;
    double y;

    load(&s, WAVE "portal 1500 750 2 cube\n" FAR);
    run_while_wave(&s, NONE);
    CHECK(s.st.player.mode == MODE_CUBE && s.st.player.alive);
    CHECK(s.st.player.pos.y == PLAYER_SPAWN_Y);
    CHECK(s.st.player.can_jump);
    sim_tick(&s, PRESS);                       /* a cube like any other */
    CHECK(s.st.player.vy == PER_TICK(CUBE_JUMP_V) - MODES[MODE_CUBE].gravity);
    sim_free(&s);
    load(&s, WAVE "start_gravity flipped\nportal 1500 750 2 ufo\n" FAR);
    run_while_wave(&s, HELD);                  /* the ground is its ceiling */
    CHECK(s.st.player.mode == MODE_UFO && s.st.player.pos.y == PLAYER_SPAWN_Y);
    sim_free(&s);
    load(&s, WAVE MID "portal 560 100 2 ship\n" FAR);
    while (s.st.player.alive && s.st.player.mode == MODE_WAVE
        && !s.st.complete) {
        y = s.st.player.pos.y;
        sim_tick(&s, HELD);
    }
    CHECK(s.st.player.pos.y == y - VX);        /* in the air: not moved (5.2) */
    sim_free(&s);
}

/* The nose follows what it did: 45 degrees in the air, flat on a surface. */
static void test_rotation(void)
{
    sim_t s;

    load(&s, WAVE MID FAR);
    run(&s, 5, HELD);
    CHECK(fabsf(s.st.player.rotation + 45.0f) < 1e-3f);
    run(&s, 5, NONE);
    CHECK(fabsf(s.st.player.rotation - 45.0f) < 1e-3f);
    run(&s, 400, NONE);
    CHECK(s.st.player.grounded && s.st.player.rotation == 0.0f);
    sim_free(&s);
    load(&s, WAVE MID "start_gravity flipped\n" FAR);
    run(&s, 5, HELD);                          /* down the screen */
    CHECK(fabsf(s.st.player.rotation - 45.0f) < 1e-3f);
    sim_free(&s);
}

void test_wave(void)
{
    test_row();
    test_45_degrees();
    test_no_inertia();
    test_any_speed();
    test_block_top_kills();
    test_block_underside_and_wall();
    test_slope_and_spike();
    test_the_small_square();
    test_slides_on_the_ground();
    test_slides_on_the_ceiling();
    test_flipped();
    test_the_next_mode_inherits();
    test_growing_on_the_ground();
    test_rotation();
}
