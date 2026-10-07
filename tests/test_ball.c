/*
** ALEXNEX PROJECT, 2026
** tests/test_ball.c
** File description:
** the ball: a flip where the cube has its jump (FEATURES 9)
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

#define BALL "start_gamemode ball\n"
#define FAR "block 30000 750 2\n"             /* a level long enough  */
#define FLOOR_Y (GROUND_Y - PLAYER_HALF)      /* resting on the floor: 800 */
#define CROSSING 122                          /* ticks from one side to the other */
#define CEIL_Y (GROUND_Y - 800.0 + PLAYER_HALF)   /* and on the ceiling: 100 */

static void load(sim_t *s, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

static void run(sim_t *s, int ticks, input_t in)
{
    for (int i = 0; i < ticks && s->st.player.alive; i++)
        sim_tick(s, in);
}

/* Ticks until it rests on something again, the button as given. */
static int cross(sim_t *s, input_t in)
{
    int ticks = 0;

    while (s->st.player.alive && !s->st.player.grounded && ticks < 1000) {
        sim_tick(s, in);
        ticks += 1;
    }
    return ticks;
}

/* Its row: the cube's size, its own gravity, an 800 px corridor (6.9). */
static void test_row(void)
{
    const mode_ops_t *m = &MODES[MODE_BALL];
    sim_t s;

    CHECK(m->half == PLAYER_HALF && m->inner_half == PLAYER_INNER_HALF);
    CHECK(m->corridor_height == 800.0 && m->bot_decision_ticks == 0);
    CHECK(!m->neutral_kills && !m->keep_vy_on_surface);
    CHECK(fabs(BALL_GRAVITY - 5393.4) < 0.06);
    CHECK(fabs(BALL_MAX_FALL - 1661.8) < 0.05);
    CHECK(m->gravity == PER_TICK2(BALL_GRAVITY));
    CHECK(m->max_fall == PER_TICK(BALL_MAX_FALL));
    CHECK(modes_tallest_corridor() == 1000.0);     /* still the ship's */
    load(&s, BALL FAR);
    CHECK(s.st.player.mode == MODE_BALL && s.st.bounds.active);
    CHECK(s.st.bounds.bottom == GROUND_Y && s.st.bounds.top == GROUND_Y - 800.0);
    CHECK(s.st.player.grounded && s.st.player.can_jump);
    sim_free(&s);
}

/* A click flips the gravity and pushes the ball off its surface (9.2). */
static void test_flip_from_rest(void)
{
    double g = MODES[MODE_BALL].gravity;
    double push = PER_TICK(BALL_FLIP_V);
    sim_t s;
    int ticks;

    load(&s, BALL FAR);
    run(&s, 10, NONE);
    CHECK(s.st.player.pos.y == FLOOR_Y && s.st.player.vy == 0.0);
    sim_tick(&s, PRESS);
    CHECK(s.st.player.gravity_dir == -1 && !s.st.player.grounded);
    CHECK(s.st.player.vy == -push - g);        /* a small push, then gravity */
    CHECK(s.st.player.pos.y == FLOOR_Y - push - g);
    CHECK(fabs(BALL_FLIP_V - 415.4) < 0.05 && BALL_FLIP_V <= BALL_MAX_FALL / 4.0);
    CHECK(s.st.player.hold == HOLD_USED);
    run(&s, 4, NONE);
    CHECK(fabs(FLOOR_Y - s.st.player.pos.y - 10.1) < 0.05);  /* after 5 ticks */
    ticks = 5 + cross(&s, NONE);
    CHECK(ticks == CROSSING);                  /* 0.51 s across 700 px (9.3) */
    CHECK(s.st.player.alive && s.st.player.pos.y == CEIL_Y);
    CHECK(s.st.player.vy == 0.0 && s.st.player.can_jump);
    run(&s, 50, NONE);
    CHECK(s.st.player.pos.y == CEIL_Y && s.st.player.gravity_dir == -1);
    sim_tick(&s, PRESS);                       /* and back */
    ticks = 1 + cross(&s, NONE);
    CHECK(ticks == CROSSING && s.st.player.pos.y == FLOOR_Y);
    CHECK(s.st.player.gravity_dir == 1);
    sim_free(&s);
}

/* The flip uses the hold up: kept down, the ball flips once and no more. */
static void test_a_hold_flips_once(void)
{
    sim_t s;
    int flips = 0;
    int dir;

    load(&s, BALL FAR);
    run(&s, 10, NONE);
    for (int i = 0; i < 500; i++) {
        dir = s.st.player.gravity_dir;
        sim_tick(&s, (input_t){true, i == 0});
        flips += dir != s.st.player.gravity_dir;
    }
    CHECK(s.st.player.alive && flips == 1);
    CHECK(s.st.player.pos.y == CEIL_Y && s.st.player.grounded);
    CHECK(s.st.player.hold == HOLD_USED);
    sim_tick(&s, NONE);                        /* released, pressed again */
    sim_tick(&s, PRESS);
    CHECK(s.st.player.gravity_dir == 1);
    sim_free(&s);
}

/*
** No press memory: a click released in the air is lost. A hold that is
** still down when the ball lands is fresh, and flips there, once.
*/
static void test_clicks_in_the_air(void)
{
    sim_t s;

    load(&s, BALL FAR);
    sim_tick(&s, TAP);                         /* inside one tick: it counts */
    CHECK(s.st.player.gravity_dir == -1);
    run(&s, 30, NONE);
    sim_tick(&s, PRESS);                       /* in the air */
    run(&s, 5, HELD);
    CHECK(s.st.player.gravity_dir == -1);
    run(&s, 5, NONE);                          /* released before it lands */
    cross(&s, NONE);
    run(&s, 50, NONE);
    CHECK(s.st.player.pos.y == CEIL_Y && s.st.player.gravity_dir == -1);
    sim_tick(&s, PRESS);
    run(&s, 30, NONE);
    sim_tick(&s, PRESS);                       /* in the air again, and kept */
    CHECK(s.st.player.gravity_dir == 1 && s.st.player.hold == HOLD_FRESH);
    cross(&s, HELD);
    CHECK(s.st.player.pos.y == FLOOR_Y && s.st.player.gravity_dir == 1);
    sim_tick(&s, HELD);                        /* the landing: it flips */
    CHECK(s.st.player.gravity_dir == -1 && s.st.player.hold == HOLD_USED);
    cross(&s, HELD);
    run(&s, 50, HELD);                         /* and stays up there */
    CHECK(s.st.player.pos.y == CEIL_Y && s.st.player.gravity_dir == -1);
    sim_free(&s);
}

/* 9.3: flipped, it lands under a block whose underside is at 400. */
static void test_lands_under_a_block(void)
{
    sim_t s;

    load(&s, BALL "block 800 300 2 w=20\n" FAR);
    while (s.st.player.pos.x < 1000.0)
        sim_tick(&s, NONE);
    sim_tick(&s, PRESS);
    cross(&s, NONE);
    CHECK(s.st.player.alive && s.st.player.grounded);
    CHECK(s.st.player.pos.y == 450.0);
    while (s.st.player.pos.x < 1900.0 && s.st.player.alive)
        sim_tick(&s, NONE);                    /* off its end: up to the ceiling */
    cross(&s, NONE);
    CHECK(s.st.player.alive && s.st.player.pos.y == CEIL_Y);
    sim_free(&s);
}

/* Thrown at a ceiling it bounces with 30%; spikes and walls kill it. */
static void test_contacts(void)
{
    sim_t s;
    double before = 0.0;

    load(&s, BALL "block 300 460 2 w=20\n" FAR);
    s.st.player.pos.y = 700.0;
    s.st.player.vy = PER_TICK(2000);
    for (int i = 0; i < 60 && s.st.player.vy > 0.0; i++) {
        before = s.st.player.vy;
        sim_tick(&s, NONE);
    }
    CHECK(s.st.player.alive && s.st.player.vy < 0.0);
    CHECK(fabs(s.st.player.vy + before * BOUNCE_RESTITUTION_SHIP) < 0.2);
    sim_free(&s);
    load(&s, BALL "spike 1000 750 2\n" FAR);
    run(&s, 400, NONE);
    CHECK(!s.st.player.alive);
    sim_free(&s);
    load(&s, BALL "block 1000 650 2 h=4\n" FAR);
    run(&s, 400, NONE);
    CHECK(!s.st.player.alive && s.st.player.pos.x < 1000.0);
    sim_free(&s);
    load(&s, BALL "spike 1000 50 2 rot=180\n" FAR);   /* on the ceiling */
    sim_tick(&s, PRESS);
    run(&s, 400, NONE);
    CHECK(!s.st.player.alive);
    sim_free(&s);
}

/*
** The 800 px corridor is the one that isn't 1000. A portal is 280 px tall,
** so whatever touches it is well inside the corridor it opens: a ship
** against the top of its own is not moved (5.2), and falls to the new floor.
*/
static void test_corridor_holds_the_player(void)
{
    sim_t s;
    vec2_t at;

    load(&s, "start_gamemode ship\nportal 2000 0 2 ball\n" FAR);
    while (s.st.player.alive && s.st.player.mode == MODE_SHIP
        && !s.st.complete) {
        at = s.st.player.pos;
        sim_tick(&s, HELD);                    /* pressed against -150 */
    }
    CHECK(s.st.player.mode == MODE_BALL && s.st.player.alive);
    CHECK(at.y < -99.0 && s.st.player.pos.y - at.y < 0.01);
    CHECK(s.st.bounds.bottom - s.st.bounds.top == 800.0);
    CHECK(s.st.bounds.top <= s.st.player.pos.y - PLAYER_HALF);
    CHECK(s.st.bounds.top == -350.0);          /* centered on the portal: 50 */
    run(&s, 300, NONE);
    CHECK(s.st.player.alive && s.st.player.pos.y == 450.0 - PLAYER_HALF);
    sim_free(&s);
    load(&s, "portal 1500 350 2 ball\n" FAR);  /* a cube walking under it */
    run(&s, 400, NONE);
    CHECK(s.st.player.mode == MODE_CUBE);      /* too high to touch */
    sim_free(&s);
}

/* A wave turning into a ball on the ground stands on it (FEATURES 8.7). */
static void test_from_a_wave(void)
{
    sim_t s;

    load(&s, "start_gamemode wave\nportal 1500 750 2 ball\n" FAR);
    while (s.st.player.alive && s.st.player.mode == MODE_WAVE
        && !s.st.complete)
        sim_tick(&s, NONE);
    CHECK(s.st.player.mode == MODE_BALL && s.st.player.pos.y == FLOOR_Y);
    CHECK(s.st.bounds.bottom == GROUND_Y && s.st.player.can_jump);
    sim_tick(&s, PRESS);
    CHECK(s.st.player.gravity_dir == -1);
    sim_free(&s);
}

/* It rolls: one turn a second at 1x, backwards along a ceiling. */
static void test_rolling(void)
{
    double per_tick = 360.0 / TICK_RATE;
    sim_t s;
    float before;

    load(&s, BALL FAR);
    run(&s, 20, NONE);
    CHECK(fabs(s.st.player.rotation - 20.0 * per_tick) < 1e-3);
    run(&s, 200, NONE);
    CHECK(s.st.player.rotation >= 0.0f && s.st.player.rotation < 360.0f);
    sim_tick(&s, PRESS);
    cross(&s, NONE);
    before = s.st.player.rotation;
    sim_tick(&s, NONE);
    CHECK(fabs(s.st.player.rotation - (before - per_tick)) < 1e-3);
    sim_free(&s);
}

void test_ball(void)
{
    test_row();
    test_flip_from_rest();
    test_a_hold_flips_once();
    test_clicks_in_the_air();
    test_lands_under_a_block();
    test_contacts();
    test_corridor_holds_the_player();
    test_from_a_wave();
    test_rolling();
}
