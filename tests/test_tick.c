/*
** ALEXNEX PROJECT, 2026
** tests/test_tick.c
** File description:
** one tick at a time: the jump, contacts, deaths, the zone, determinism (3.4)
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

#define HELD ((input_t){true, false})
#define TAP ((input_t){true, true})
#define NONE ((input_t){false, false})

static void load(sim_t *s, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

static void run(sim_t *s, int ticks, input_t in)
{
    for (int i = 0; i < ticks; i++)
        sim_tick(s, in);
}

/* The cube's jump, the numbers PLAN 11.1 pins. */
static void test_jump_arc(void)
{
    sim_t s;
    double start_x;
    double apex = 0.0;
    int airtime = 0;

    load(&s, "block 100000 700 2\n");         /* far away: flat ground only */
    run(&s, 10, NONE);
    CHECK(s.st.player.pos.y == PLAYER_SPAWN_Y);
    CHECK(s.st.player.can_jump && s.st.player.grounded);
    start_x = s.st.player.pos.x;
    sim_tick(&s, TAP);                        /* the jump's tick */
    while (!s.st.player.grounded && airtime < 400) {
        apex = fmax(apex, PLAYER_SPAWN_Y - s.st.player.pos.y);
        sim_tick(&s, NONE);
        airtime += 1;
    }
    CHECK(fabs(apex - 213.32) < 0.05);
    CHECK(airtime + 1 == 102);
    CHECK(fabs(s.st.player.pos.x - start_x - 441.4) < 0.1);
    CHECK(s.st.player.pos.y == PLAYER_SPAWN_Y);
    sim_free(&s);
}

/*
** A tick always covers exactly vx, whatever it met on the way: the legs of
** one tick are fractions of the same move, never separate additions (3.2).
*/
static void test_scroll_is_exact(void)
{
    sim_t s;
    double step = PER_TICK(SCROLL_SPEED);
    double previous;

    load(&s, "block 100000 700 2\n");
    for (int i = 1; i <= 100; i++) {
        previous = s.st.distance;
        sim_tick(&s, NONE);
        CHECK(s.st.distance == previous + step);
        CHECK(s.st.player.pos.x == PLAYER_SPAWN_X + s.st.distance);
    }
    CHECK(fabs(s.st.distance - step * 100.0) < 1e-9);
    CHECK(s.st.player.vy == 0.0);             /* resting on the ground */
    sim_free(&s);
    /* the same over a landing, where the tick is split in two legs */
    load(&s, "block 600 700 2\nblock 700 700 2\n");
    s.st.player.pos.y = 400.0;
    s.st.player.grounded = false;
    for (int i = 0; i < 200; i++) {
        previous = s.st.distance;
        sim_tick(&s, i == 100 ? TAP : NONE);
        CHECK(s.st.distance == previous + step);
    }
    sim_free(&s);
}

/* Landing puts the square exactly on the surface, whatever the fall. */
static void test_land_on_ground_and_block(void)
{
    sim_t s;

    load(&s, "block 100000 700 2\n");
    s.st.player.pos.y = 400.0;
    s.st.player.grounded = false;
    run(&s, 120, NONE);
    CHECK(s.st.player.pos.y == PLAYER_SPAWN_Y);
    CHECK(s.st.player.vy == 0.0 && s.st.player.grounded);
    sim_free(&s);
    load(&s, "block 600 700 2\nblock 700 700 2\n");
    s.st.player.pos.y = 400.0;
    s.st.player.grounded = false;
    run(&s, 60, NONE);
    CHECK(s.st.player.pos.y == 650.0);        /* block top 700, half 50 */
    CHECK(s.st.player.grounded && s.st.player.alive);
    sim_free(&s);
}

/* Twenty blocks in a row: no seam kills the player or stops it. */
static void test_seams(void)
{
    sim_t s;
    char text[1024];
    size_t pos = 0;
    int grounded_ticks = 0;

    for (int i = 0; i < 20; i++)
        pos += snprintf(text + pos, sizeof(text) - pos, "block %d 700 2\n",
            600 + i * 100);
    load(&s, text);
    s.st.distance = 250.0;                    /* start on the platform's first block */
    s.st.player.pos.x = PLAYER_SPAWN_X + s.st.distance;
    s.st.player.pos.y = 650.0;
    for (int i = 0; i < 400; i++) {
        sim_tick(&s, NONE);
        if (s.st.player.pos.x > 700.0 && s.st.player.pos.x < 2450.0) {
            grounded_ticks += s.st.player.grounded;
            CHECK(s.st.player.pos.y == 650.0);   /* level across every seam */
            CHECK(s.st.player.can_jump);
        }
    }
    CHECK(s.st.player.alive);
    CHECK(grounded_ticks > 350);              /* 400 ticks cover 1731 px */
    sim_free(&s);
}

/* Off a ledge the player stays level, then falls with gravity alone. */
static void test_ledge(void)
{
    sim_t s;
    double gravity = MODES[MODE_CUBE].gravity;
    int ticks = 0;

    load(&s, "block 600 700 2\n");
    s.st.distance = 250.0;                    /* standing on the block */
    s.st.player.pos.x = PLAYER_SPAWN_X + s.st.distance;
    s.st.player.pos.y = 650.0;
    while (s.st.player.pos.y == 650.0 && ticks < 200) {
        sim_tick(&s, NONE);
        ticks += 1;
    }
    /* the square (half 50) leaves the block's right edge at x = 750 */
    CHECK(s.st.player.pos.x > 750.0 && s.st.player.pos.x < 750.0 + 2.0 * 4.3275);
    CHECK(fabs(s.st.player.vy + gravity) < 1e-12);   /* one tick of gravity */
    sim_free(&s);
}

static void test_wall_kills_at_the_inner_box(void)
{
    sim_t s;
    double penetration;

    load(&s, "block 1000 400 2 h=9\n");        /* a wall down to the ground */
    while (s.st.player.alive && s.st.player.pos.x < 1400.0)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.alive);
    penetration = s.st.player.pos.x + PLAYER_INNER_HALF - 1000.0;
    CHECK(penetration >= 0.0 && penetration < PER_TICK(SCROLL_SPEED));
    sim_free(&s);
}

static void test_spawn_inside_a_block(void)
{
    sim_t s;

    load(&s, "block 300 750 2\n");             /* over the spawn point */
    sim_tick(&s, NONE);
    CHECK(!s.st.player.alive);
    CHECK(s.st.player.pos.x == PLAYER_SPAWN_X);   /* it died where it stood */
    sim_free(&s);
}

static void test_spikes(void)
{
    sim_t s;

    load(&s, "spike 1000 750 2\n");
    while (s.st.player.alive && s.st.player.pos.x < 1200.0)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.alive);
    CHECK(s.st.player.pos.x + PLAYER_HALF >= 1030.0);   /* the spike's box */
    CHECK(s.st.player.pos.x + PLAYER_HALF < 1030.0 + PER_TICK(SCROLL_SPEED));
    sim_free(&s);
    /* the same spike 100 px higher: the square passes under it */
    load(&s, "spike 1000 600 2\n");
    run(&s, 300, NONE);
    CHECK(s.st.player.alive);
    sim_free(&s);
}

/* Holding jumps again on every landing; a press in the air is lost. */
static void test_jump_zone_rules(void)
{
    sim_t s;
    int jumps = 0;
    bool was_grounded = true;

    load(&s, "block 100000 700 2\n");
    run(&s, 5, NONE);
    for (int i = 0; i < 500; i++) {
        sim_tick(&s, HELD);
        jumps += !s.st.player.grounded && was_grounded;
        was_grounded = s.st.player.grounded;
    }
    CHECK(jumps >= 4);                         /* 500 ticks, 102 per jump */
    CHECK(s.st.player.hold == HOLD_USED);      /* the hold jumped */
    sim_free(&s);
    load(&s, "block 100000 700 2\n");
    run(&s, 5, NONE);
    sim_tick(&s, TAP);                         /* jump */
    run(&s, 20, NONE);
    CHECK(!s.st.player.can_jump);              /* no second jump in the air */
    sim_tick(&s, TAP);
    CHECK(s.st.player.vy < PER_TICK(CUBE_JUMP_V));
    run(&s, 30, TAP);                          /* pressing mid-air changes nothing */
    CHECK(!s.st.player.grounded);
    sim_free(&s);
}

/* A press released before landing never jumps: there is no buffer (3.4). */
static void test_no_jump_buffer(void)
{
    sim_t s;
    int ticks = 0;

    load(&s, "block 100000 700 2\n");
    run(&s, 5, NONE);
    sim_tick(&s, TAP);
    while (!s.st.player.grounded && ticks < 200) {
        sim_tick(&s, ticks < 60 ? HELD : NONE);   /* released well before landing */
        ticks += 1;
    }
    CHECK(s.st.player.grounded);
    sim_tick(&s, NONE);
    CHECK(s.st.player.grounded);               /* it stayed down */
    sim_free(&s);
}

static void test_kill_ceiling(void)
{
    sim_t s;

    load(&s, "block 1000 700 2\n");
    CHECK(s.lvl.kill_y == GROUND_Y - CORRIDOR_MAX_HEIGHT - KILL_CEILING_MARGIN);
    s.st.player.gravity_dir = -1;              /* flipped: it falls upward */
    s.st.player.grounded = false;
    run(&s, 600, NONE);
    CHECK(!s.st.player.alive);
    sim_free(&s);
    load(&s, "block 1000 700 2\n");
    s.st.player.vy = PER_TICK(6000);           /* launched, normal gravity */
    run(&s, 600, NONE);
    CHECK(s.st.player.alive);                  /* no ceiling with normal gravity */
    sim_free(&s);
}

static void test_determinism_and_snapshots(void)
{
    sim_t a;
    sim_t b;
    sim_snapshot_t snap;
    uint64_t hashes[300];

    load(&a, "block 600 700 2\nspike 5000 0 2\nblock 2000 650 2\n");
    load(&b, "block 600 700 2\nspike 5000 0 2\nblock 2000 650 2\n");
    for (int i = 0; i < 300; i++) {
        sim_tick(&a, i % 37 == 0 ? TAP : NONE);
        sim_tick(&b, i % 37 == 0 ? TAP : NONE);
        CHECK(sim_state_hash(&a) == sim_state_hash(&b));
    }
    sim_snapshot_init(&snap, &a);
    sim_snapshot_save(&snap, &a);
    for (int i = 0; i < 300; i++) {
        sim_tick(&a, i % 11 == 0 ? TAP : NONE);
        hashes[i] = sim_state_hash(&a);
    }
    sim_snapshot_restore(&a, &snap);
    for (int i = 0; i < 300; i++) {
        sim_tick(&a, i % 11 == 0 ? TAP : NONE);
        CHECK(sim_state_hash(&a) == hashes[i]);
    }
    sim_snapshot_free(&snap);
    sim_free(&a);
    sim_free(&b);
}

/* The camera decides nothing, so the bot's hash ignores it (3.3). */
static void test_physics_hash_ignores_camera(void)
{
    sim_t a;
    sim_t b;

    load(&a, "block 600 700 2\n");
    load(&b, "block 600 700 2\n");
    run(&a, 50, NONE);
    run(&b, 50, NONE);
    b.st.cam.pos.y = -123.0;
    CHECK(sim_state_hash(&a) != sim_state_hash(&b));
    CHECK(sim_physics_hash(&a) == sim_physics_hash(&b));
    sim_free(&a);
    sim_free(&b);
}

static void test_percent_and_completion(void)
{
    sim_t s;

    load(&s, "block 1000 400 2\n");            /* above the player: end_shift 1600 */
    CHECK(sim_percent(&s) == 0.0f);
    while (!s.st.complete && s.st.player.alive && s.st.tick < 2000)
        sim_tick(&s, NONE);
    CHECK(s.st.player.alive);
    CHECK(s.st.complete);
    CHECK(sim_percent(&s) <= 100.0f && sim_percent(&s) > 99.0f);
    run(&s, 10, NONE);                         /* a complete level stops ticking */
    CHECK(s.st.tick < 2000);
    sim_free(&s);
}

void test_tick(void)
{
    test_jump_arc();
    test_scroll_is_exact();
    test_land_on_ground_and_block();
    test_seams();
    test_ledge();
    test_wall_kills_at_the_inner_box();
    test_spawn_inside_a_block();
    test_spikes();
    test_jump_zone_rules();
    test_no_jump_buffer();
    test_kill_ceiling();
    test_determinism_and_snapshots();
    test_physics_hash_ignores_camera();
    test_percent_and_completion();
}
