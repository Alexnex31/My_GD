/*
** ALEXNEX PROJECT, 2026
** tests/test_ufo.c
** File description:
** the UFO: one hop per press, the numbers of FEATURES 7.3
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

#define UFO "start_gamemode ufo\n"
#define FAR "block 30000 750 2\n"                     /* a level long enough  */
#define HOP (PER_TICK(UFO_JUMP_V))

static void load(sim_t *s, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

/* One hop from the floor, the button as given after the press. */
static void hop(sim_t *s, input_t after, double *rise, int *apex, int *back)
{
    double floor_y = s->st.player.pos.y;

    *rise = 0.0;
    *apex = 0;
    *back = 0;
    sim_tick(s, PRESS);
    for (int i = 1; i < 400 && !s->st.player.grounded; i++) {
        if (floor_y - s->st.player.pos.y > *rise) {
            *rise = floor_y - s->st.player.pos.y;
            *apex = i;
        }
        sim_tick(s, after);
        *back = i + 1;
    }
}

/* 7.3: 153.6 px up, the apex after 0.213 s, down again after 102 ticks. */
static void test_hop_numbers(void)
{
    sim_t s;
    double rise;
    int apex;
    int back;

    load(&s, UFO FAR);
    CHECK(s.st.player.mode == MODE_UFO && s.st.player.grounded);
    CHECK(s.st.bounds.active && s.st.bounds.bottom == GROUND_Y);
    CHECK(s.st.bounds.top == GROUND_Y - 1000.0);
    hop(&s, NONE, &rise, &apex, &back);
    CHECK(fabs(rise - 153.6) < 0.05);
    CHECK(apex == 51);                         /* 0.2125 s */
    CHECK(back == 102);                        /* 0.425 s, a cube jump's time */
    CHECK(s.st.player.pos.y == PLAYER_SPAWN_Y && s.st.player.vy == 0.0);
    CHECK(rise < 213.3 - 50.0);                /* well under a cube's jump */
    sim_free(&s);
}

/* Holding after the press changes nothing, and neither does landing held. */
static void test_holding_does_nothing(void)
{
    sim_t s;
    double rise;
    int apex;
    int back;

    load(&s, UFO FAR);
    hop(&s, HELD, &rise, &apex, &back);
    CHECK(fabs(rise - 153.6) < 0.05 && apex == 51 && back == 102);
    for (int i = 0; i < 50; i++) {
        sim_tick(&s, HELD);                    /* no auto-hop like the cube's */
        CHECK(s.st.player.grounded && s.st.player.pos.y == PLAYER_SPAWN_Y);
    }
    sim_free(&s);
    load(&s, UFO FAR);                         /* a hold carried into the attempt */
    for (int i = 0; i < 50; i++)
        sim_tick(&s, HELD);
    CHECK(s.st.player.grounded && s.st.player.pos.y == PLAYER_SPAWN_Y);
    CHECK(s.st.player.hold == HOLD_FRESH);     /* nothing used it */
    sim_free(&s);
}

/* A hop sets the speed: the same one from a fast fall, a fast rise, the floor. */
static void test_assign_not_add(void)
{
    static const double before[] = {-1800.0, -300.0, 0.0, 900.0, 2876.9};
    double g = MODES[MODE_UFO].gravity;
    sim_t s;

    load(&s, UFO FAR);
    for (size_t i = 0; i < sizeof(before) / sizeof(before[0]); i++) {
        sim_reset(&s);
        s.st.player.pos.y = 400.0;             /* in the air, mid corridor */
        s.st.player.vy = PER_TICK(before[i]);
        s.st.player.hold = HOLD_NONE;
        sim_tick(&s, PRESS);
        CHECK(s.st.player.vy == HOP - g);      /* then one tick of gravity */
        CHECK(s.st.player.pos.y == 400.0 - (HOP - g));
        CHECK(s.st.player.hold == HOLD_USED);  /* it activates no orb now */
        CHECK(!s.st.player.grounded && !s.st.player.can_jump);
    }
    sim_reset(&s);
    ufo_input(&s.st.player, PRESS);            /* from the floor: it leaves it */
    CHECK(s.st.player.vy == HOP);
    CHECK(!s.st.player.grounded && !s.st.player.can_jump);
    sim_free(&s);
}

/* Height after `presses` hops, one every `every` ticks, from mid air. */
static double tapped_height(sim_t *s, int every, int presses)
{
    sim_reset(s);
    s->st.player.pos.y = 500.0;
    for (int i = 0; i < every * presses; i++)
        sim_tick(s, i % every == 0 ? PRESS : NONE);
    CHECK(s->st.player.alive);
    return 500.0 - s->st.player.pos.y;
}

/*
** 7.3: the rhythm that holds an altitude is the hop's own duration, between
** 101 and 102 ticks. Faster climbs, slower sinks.
*/
static void test_tap_rhythm(void)
{
    sim_t s;

    load(&s, UFO FAR);
    CHECK(tapped_height(&s, 101, 4) > 0.0 && tapped_height(&s, 101, 4) < 10.0);
    CHECK(tapped_height(&s, 102, 4) < 0.0 && tapped_height(&s, 102, 4) > -20.0);
    CHECK(tapped_height(&s, 80, 4) > 100.0);
    CHECK(tapped_height(&s, 115, 3) < -100.0);
    sim_free(&s);
}

/* Released, it falls with its own gravity up to its own cap. */
static void test_fall_cap(void)
{
    double g = MODES[MODE_UFO].gravity;
    sim_t s;

    load(&s, UFO FAR);
    s.st.player.pos.y = -90.0;                 /* under the corridor's ceiling */
    sim_tick(&s, NONE);
    CHECK(s.st.player.vy == -g);
    CHECK(g < MODES[MODE_CUBE].gravity);       /* lighter than the cube */
    for (int i = 0; i < 70 && !s.st.player.grounded; i++)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.grounded);
    CHECK(s.st.player.vy == -PER_TICK(UFO_MAX_FALL));
    CHECK(fabs(UFO_MAX_FALL - 1796.8) < 0.05);     /* FEATURES 7.2 */
    CHECK(fabs(UFO_GRAVITY - 6903.6) < 0.05);
    CHECK(fabs(UFO_JUMP_V - 1470.7) < 0.05);
    sim_free(&s);
}

/* A block's underside sends it back with 30% of its speed; nothing dies. */
static void test_ceiling_bounce(void)
{
    sim_t s;
    double before = 0.0;

    load(&s, UFO "block 300 460 2 w=20\n" FAR);
    for (int i = 0; i < 20; i++)
        sim_tick(&s, i == 0 ? PRESS : NONE);   /* 240 px of room: no touch */
    s.st.player.vy = PER_TICK(2000);
    for (int i = 0; i < 60 && s.st.player.vy > 0.0; i++) {
        before = s.st.player.vy;
        sim_tick(&s, NONE);
    }
    CHECK(s.st.player.alive && s.st.player.vy < 0.0);
    CHECK(fabs(s.st.player.vy + before * BOUNCE_RESTITUTION_SHIP) < 0.2);
    sim_free(&s);
}

/* The corridor's ceiling too: hopping into it over and over is safe. */
static void test_corridor_ceiling(void)
{
    sim_t s;
    double top = 0.0;

    load(&s, UFO FAR);
    for (int i = 0; i < 1500; i++) {
        sim_tick(&s, i % 30 == 0 ? PRESS : NONE);
        top = fmin(top, s.st.player.pos.y - PLAYER_HALF);
        CHECK(s.st.player.pos.y - PLAYER_HALF >= s.st.bounds.top);
    }
    CHECK(s.st.player.alive);
    CHECK(top < s.st.bounds.top + 1.0);        /* it did reach it */
    sim_free(&s);
}

/* It lands on a block's top, and dies running into a wall or a spike. */
static void test_blocks_and_spikes(void)
{
    sim_t s;

    load(&s, UFO "block 1000 750 2 w=6\n" FAR);
    for (int i = 0; i < 400 && s.st.player.pos.x < 1250.0; i++)
        sim_tick(&s, i == 100 ? PRESS : NONE);   /* a hop onto it */
    CHECK(s.st.player.alive && s.st.player.grounded);
    CHECK(s.st.player.pos.y == 750.0 - PLAYER_HALF);
    sim_free(&s);
    load(&s, UFO "block 1000 650 2 h=4\n" FAR);
    for (int i = 0; i < 400 && s.st.player.alive; i++)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.alive && s.st.player.pos.x < 1000.0);
    sim_free(&s);
    load(&s, UFO "spike 1000 750 2\n" FAR);
    for (int i = 0; i < 400 && s.st.player.alive; i++)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.alive);
    sim_free(&s);
}

/* Flipped, the corridor's ceiling is its floor and a hop goes down the screen. */
static void test_flipped(void)
{
    sim_t s;
    double rest = GROUND_Y - 1000.0 + PLAYER_HALF;
    double lowest = rest;

    load(&s, UFO "start_gravity flipped\n" FAR);
    CHECK(s.st.player.gravity_dir == -1 && !s.st.player.grounded);
    for (int i = 0; i < 400 && !s.st.player.grounded; i++)
        sim_tick(&s, NONE);                    /* it falls up to the ceiling */
    CHECK(s.st.player.grounded && s.st.player.pos.y == rest);
    sim_tick(&s, PRESS);
    for (int i = 1; i < 102; i++) {
        lowest = fmax(lowest, s.st.player.pos.y);
        CHECK(!s.st.player.grounded);
        sim_tick(&s, NONE);
    }
    CHECK(fabs(lowest - rest - 153.6) < 0.05);
    CHECK(s.st.player.grounded && s.st.player.pos.y == rest);
    sim_free(&s);
}

/* The portal changes the mode and keeps the speed; the next press is a hop. */
static void test_portals(void)
{
    sim_t s;
    double vy;

    load(&s, "portal 1500 650 2 ufo\nportal 3600 650 2 cube\n" FAR);
    for (int i = 0; i < 2000 && s.st.player.mode == MODE_CUBE; i++) {
        vy = s.st.player.vy;
        sim_tick(&s, i == 240 ? PRESS : NONE);   /* a cube jump into it */
    }
    CHECK(s.st.player.mode == MODE_UFO && s.st.bounds.active);
    CHECK(s.st.bounds.bottom == GROUND_Y && s.st.bounds.top == -150.0);
    CHECK(!s.st.player.grounded);
    CHECK(s.st.player.vy == vy - MODES[MODE_CUBE].gravity);   /* kept */
    sim_tick(&s, PRESS);                         /* in the air: a cube couldn't */
    CHECK(s.st.player.vy == HOP - MODES[MODE_UFO].gravity);
    while (s.st.player.alive && s.st.player.mode == MODE_UFO
        && !s.st.complete)
        sim_tick(&s, NONE);
    CHECK(s.st.player.mode == MODE_CUBE && !s.st.bounds.active);
    sim_free(&s);
}

/* The icon leans a third of what the ship does, mirrored with gravity. */
static void test_rotation(void)
{
    player_t ufo = {.vx = 4.0, .vy = 4.0, .gravity_dir = 1};
    player_t ship = ufo;

    ufo_rotation(&ufo);
    ship_rotation(&ship);
    CHECK(fabsf(ship.rotation + 45.0f) < 1e-4f);
    CHECK(fabsf(ufo.rotation + 15.0f) < 1e-4f);
    ufo.gravity_dir = -1;
    ufo_rotation(&ufo);
    CHECK(fabsf(ufo.rotation - 15.0f) < 1e-4f);
    ufo.vy = 0.0;
    ufo_rotation(&ufo);
    CHECK(ufo.rotation == 0.0f);
}

void test_ufo(void)
{
    test_hop_numbers();
    test_holding_does_nothing();
    test_assign_not_add();
    test_tap_rhythm();
    test_fall_cap();
    test_ceiling_bounce();
    test_corridor_ceiling();
    test_blocks_and_spikes();
    test_flipped();
    test_portals();
    test_rotation();
}
