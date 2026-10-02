/*
** ALEXNEX PROJECT, 2026
** tests/test_slopes.c
** File description:
** the circle's contacts, slopes, head hits and the step-up (4.4, G.7, G.9)
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

#define TAP ((input_t){true, true})
#define NONE ((input_t){false, false})

static void load(sim_t *s, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

static void place(sim_t *s, double x, double y)
{
    s->st.distance = x - PLAYER_SPAWN_X;
    s->st.player.pos.x = x;
    s->st.player.pos.y = y;
}

static void run(sim_t *s, int ticks, input_t in)
{
    for (int i = 0; i < ticks; i++)
        sim_tick(s, in);
}

/* On a 45 degree slope the circle climbs at exactly the scroll speed. */
static void test_climb_a_slope(void)
{
    sim_t s;
    double vx = PER_TICK(SCROLL_SPEED);
    int grounded = 0;

    load(&s, "slope 1000 750 2\nblock 1100 750 2\n");
    while (s.st.player.pos.x < 1000.0 && s.st.player.alive)
        sim_tick(&s, NONE);
    for (int i = 0; i < 20 && s.st.player.pos.x < 1064.0; i++) {
        sim_tick(&s, NONE);
        grounded += s.st.player.grounded;
        CHECK(s.st.player.grounded);                       /* it stays on it */
        CHECK(fabs(s.st.player.vy - vx) < 1e-9);           /* 45 deg: vy = vx */
        CHECK(fabs(s.st.player.surface_rise - vx) < 1e-9);
        CHECK(s.st.player.support_normal.y < -0.7);
    }
    CHECK(s.st.player.alive);
    CHECK(grounded >= 14);                                 /* ~65 px of slope */
    sim_free(&s);
}

/* The circle rests 50 px from the face, so the player rises along it. */
static void test_slope_height(void)
{
    sim_t s;
    double before;

    load(&s, "slope 1000 750 2\nblock 1100 750 2\n");
    while (s.st.player.pos.x < 1005.0 && s.st.player.alive)
        sim_tick(&s, NONE);
    before = s.st.player.pos.y;
    while (s.st.player.pos.x < 1085.0 && s.st.player.alive)
        sim_tick(&s, NONE);
    CHECK(s.st.player.alive);
    CHECK(before - s.st.player.pos.y > 70.0);            /* it climbed */
    CHECK(s.st.player.pos.y < 760.0);
    sim_free(&s);
}

/* Off the crest of an uphill slope the player keeps the slope's rise speed. */
static void test_slope_launch(void)
{
    sim_t s;
    double vx = PER_TICK(SCROLL_SPEED);

    load(&s, "slope 1000 750 2\n");
    while (s.st.player.pos.x < 1100.0 && s.st.player.alive)
        sim_tick(&s, NONE);
    CHECK(s.st.player.alive);
    CHECK(s.st.player.vy > vx * 0.5);                    /* still going up */
    CHECK(!s.st.player.grounded);
    sim_free(&s);
}

/* Steeper than 50 degrees is a wall: the player enters it and dies. */
static void test_steep_slope_is_a_wall(void)
{
    sim_t s;

    load(&s, "slope 1000 650 2 w=1 h=4\n");              /* about 76 degrees */
    while (s.st.player.alive && s.st.player.pos.x < 1200.0)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.alive);
    sim_free(&s);
}

/* A cube that jumps into a block's underside dies: the circle takes it. */
static void test_head_hit_kills_the_cube(void)
{
    sim_t s;

    load(&s, "block 300 460 2 w=20\n");                  /* right above the spawn */
    run(&s, 5, NONE);
    sim_tick(&s, TAP);
    run(&s, 60, NONE);
    CHECK(!s.st.player.alive);
    CHECK(s.st.player.pos.y > 500.0);                    /* died under it */
    sim_free(&s);
}

/* The ship bounces off the same ceiling with 30% of its rise speed. */
static void test_head_hit_bounces_the_ship(void)
{
    sim_t s;
    double before = 0.0;

    load(&s, "block 300 460 2 w=20\n");
    s.st.player.mode = MODE_SHIP;
    s.st.player.pos.y = 700.0;
    s.st.player.vy = PER_TICK(2000);
    for (int i = 0; i < 60 && s.st.player.vy > 0.0; i++) {
        before = s.st.player.vy;
        sim_tick(&s, NONE);
    }
    CHECK(s.st.player.alive);
    CHECK(s.st.player.vy < 0.0);                         /* thrown back down */
    CHECK(fabs(s.st.player.vy + before * BOUNCE_RESTITUTION_SHIP) < 0.2);
    sim_free(&s);
}

/* A low block is climbed the moment the inner box's leading side reaches it. */
static void test_step_up(void)
{
    sim_t s;

    load(&s, "block 1000 825 2\n");                      /* a 25 px step */
    while (s.st.player.pos.x < 1100.0 && s.st.player.alive)
        sim_tick(&s, NONE);
    CHECK(s.st.player.alive);
    CHECK(s.st.player.pos.y == 775.0);                   /* 825 - half */
    CHECK(s.st.player.grounded);
    sim_free(&s);
    /* the lift happens when the inner box (20 px) reaches the face, not before */
    load(&s, "block 1000 825 2\n");
    while (s.st.player.pos.x + PLAYER_INNER_HALF + PER_TICK(SCROLL_SPEED)
        < 1000.0)
        sim_tick(&s, NONE);
    CHECK(s.st.player.pos.y == PLAYER_SPAWN_Y);          /* still on the ground */
    CHECK(s.st.player.pos.x + PLAYER_HALF > 1000.0);     /* already inside it */
    sim_tick(&s, NONE);                                  /* the lead reaches the face */
    CHECK(s.st.player.pos.y == 775.0);                   /* lifted on that tick */
    sim_free(&s);
}

/* Too high for the inner box: the block's side kills instead. */
static void test_step_too_high(void)
{
    sim_t s;

    load(&s, "block 1000 815 2\n");                      /* 35 px above the feet */
    while (s.st.player.alive && s.st.player.pos.x < 1100.0)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.alive);
    sim_free(&s);
    /* exactly 30 px: the face is on the zone's line, so it is still a step */
    load(&s, "block 1000 820 2\n");
    while (s.st.player.alive && s.st.player.pos.x < 1100.0)
        sim_tick(&s, NONE);
    CHECK(s.st.player.alive);
    CHECK(s.st.player.pos.y == 770.0);
    sim_free(&s);
}

/*
** Two faces reached at the same time: one step, onto the highest (G.7). The
** lower one is listed first, so it sorts first and is tried first.
*/
static void test_simultaneous_steps(void)
{
    sim_t s;
    int steps = 0;

    load(&s, "block 1000 840 2\nblock 1000 835 2\n");
    while (s.st.player.alive && s.st.player.pos.x < 1100.0) {
        sim_tick(&s, NONE);
        for (int i = 0; i < s.nb_dbg; i++)
            steps += s.dbg[i].kind == DBG_STEP;
    }
    CHECK(s.st.player.alive);
    CHECK(steps == 1);
    CHECK(s.st.player.pos.y == 785.0);                   /* 835 - half */
    sim_free(&s);
}

/*
** A staircase of 4 px steps half a pixel apart: a landing and three steps use
** the whole budget of a tick (MAX_CONTACTS). The last leg tests nothing, yet
** the tick still covers exactly vx and the next tick climbs on (G.8).
*/
static void test_contact_budget(void)
{
    sim_t s;
    double x;
    int full = 0;

    load(&s, "block 1000 846 2\nblock 1000.5 842 2\nblock 1001 838 2\n"
        "block 1001.5 834 2\nblock 1002 830 2\nblock 1002.5 826 2\n"
        "block 1003 822 2\n");
    while (s.st.player.alive && s.st.player.pos.x < 1090.0) {
        x = s.st.player.pos.x;
        sim_tick(&s, NONE);
        full += s.nb_dbg >= MAX_CONTACTS;
        CHECK(fabs(s.st.player.pos.x - x - PER_TICK(SCROLL_SPEED)) < 1e-9);
    }
    CHECK(full > 0);                                     /* the budget ran out */
    CHECK(s.st.player.alive);
    CHECK(s.st.player.pos.y == 772.0);                   /* on the top step */
    sim_free(&s);
}

/* A rotated slope's horizontal top is a step like any other (4.2). */
static void test_step_on_a_rotated_slope(void)
{
    sim_t s;

    load(&s, "slope 1000 800 1 rot=180\n");              /* top at 800: a 50 px... */
    while (s.st.player.alive && s.st.player.pos.x < 1100.0)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.alive);                           /* ...too high, it kills */
    sim_free(&s);
    load(&s, "slope 1000 825 1 rot=180\n");              /* its top is at 825 */
    while (s.st.player.alive && s.st.player.pos.x < 1100.0)
        sim_tick(&s, NONE);
    CHECK(s.st.player.alive);
    CHECK(s.st.player.pos.y == 775.0);
    sim_free(&s);
}

/* Rising through a corner phases past it; falling into it steps up (4.4). */
static void test_step_needs_downward_momentum(void)
{
    sim_t s;
    bool lifted = false;

    load(&s, "block 1000 825 2\n");
    place(&s, 900.0, PLAYER_SPAWN_Y);
    s.st.player.vy = PER_TICK(CUBE_JUMP_V);              /* rising through it */
    s.st.player.can_jump = false;
    for (int i = 0; i < 40 && s.st.player.alive; i++) {
        sim_tick(&s, NONE);
        if (s.st.player.pos.x > 1000.0 && s.st.player.vy > 0.0
            && s.st.player.grounded)
            lifted = true;
    }
    CHECK(!lifted);                                      /* never snapped on top */
    sim_free(&s);
}

/* The jump zone: a corner under the square's light part allows a jump. */
static void test_jump_off_a_corner(void)
{
    sim_t s;

    load(&s, "block 1000 400 2 h=4\n");                  /* a tall block */
    place(&s, 940.0, 300.0);                             /* falling beside it */
    s.st.player.grounded = false;
    s.st.player.vy = -PER_TICK(500);
    for (int i = 0; i < 30 && !s.st.player.can_jump && s.st.player.alive; i++)
        sim_tick(&s, NONE);
    CHECK(s.st.player.alive);
    CHECK(s.st.player.can_jump);                         /* GD's corner jump */
    sim_free(&s);
}

/* Climbing a slope the player may jump; the jump is the same as on flat ground. */
static void test_jump_from_a_slope(void)
{
    sim_t s;
    double vx = PER_TICK(SCROLL_SPEED);

    load(&s, "slope 1000 750 2\nblock 1100 750 2\n");
    while (s.st.player.pos.x < 1040.0 && s.st.player.alive)
        sim_tick(&s, NONE);
    CHECK(s.st.player.can_jump);                         /* climbing, so it can */
    CHECK(fabs(s.st.player.surface_rise - vx) < 1e-9);
    sim_tick(&s, TAP);
    CHECK(fabs(s.st.player.vy - (PER_TICK(CUBE_JUMP_V)
        - MODES[MODE_CUBE].gravity)) < 1e-9);            /* not additive */
    sim_free(&s);
}

void test_slopes(void)
{
    test_climb_a_slope();
    test_slope_height();
    test_slope_launch();
    test_steep_slope_is_a_wall();
    test_head_hit_kills_the_cube();
    test_head_hit_bounces_the_ship();
    test_step_up();
    test_step_too_high();
    test_simultaneous_steps();
    test_contact_budget();
    test_step_on_a_rotated_slope();
    test_step_needs_downward_momentum();
    test_jump_off_a_corner();
    test_jump_from_a_slope();
}
