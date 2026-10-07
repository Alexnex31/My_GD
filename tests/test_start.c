/*
** ALEXNEX PROJECT, 2026
** tests/test_start.c
** File description:
** the header's start_ fields: where and how an attempt begins (7.2)
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

static int warnings;

static void count_warning(const char *msg)
{
    (void)msg;
    warnings += 1;
}

static void load(sim_t *s, const char *text)
{
    warnings = 0;
    sim_load_mem(s, text, strlen(text), "10280", count_warning);
}

/* Without a single start_ field, an attempt starts exactly as it always did. */
static void test_defaults(void)
{
    sim_t s;

    load(&s, "block 1000 700 2\n");
    CHECK(warnings == 0);
    CHECK(s.st.player.pos.x == PLAYER_SPAWN_X);
    CHECK(s.st.player.pos.y == PLAYER_SPAWN_Y);
    CHECK(s.st.player.mode == MODE_CUBE);
    CHECK(s.st.player.gravity_dir == 1);
    CHECK(s.st.player.vx == PER_TICK(SCROLL_SPEED));
    CHECK(s.st.speed_mult == 1.0);
    CHECK(s.st.player.grounded && s.st.player.can_jump);
    CHECK(!s.st.bounds.active);
    CHECK(s.st.cam.pos.y == 0.0);
    sim_free(&s);
}

/* start_x and start_y place the player's center, and 0% is the spawn (7.2). */
static void test_start_position(void)
{
    sim_t s;

    load(&s, "start_x 2000\nstart_y 300\nblock 5000 700 2\n");
    CHECK(warnings == 0);
    CHECK(s.st.player.pos.x == 2000.0 && s.st.player.pos.y == 300.0);
    CHECK(s.st.distance == 0.0 && sim_percent(&s) == 0.0);
    CHECK(!s.st.player.grounded && !s.st.player.can_jump);   /* in the air */
    CHECK(s.lvl.end_shift == 5100.0 + LEVEL_END_PADDING - 2000.0);
    CHECK(s.st.cam.pos.y == 300.0 - CAM_ZONE_TOP);   /* above the zone: pulled up */
    sim_tick(&s, (input_t){false, false});
    CHECK(s.st.player.pos.x == 2000.0 + PER_TICK(SCROLL_SPEED));
    sim_free(&s);
    /* high above the ground the camera starts where it would have settled */
    load(&s, "start_y -500\nblock 5000 700 2\n");
    CHECK(s.st.cam.pos.y == -500.0 - CAM_ZONE_TOP);
    sim_free(&s);
    /* the level still completes, 500 px past the last object */
    load(&s, "start_x 2000\nblock 3000 200 2\n");   /* out of the cube's way */
    for (int i = 0; i < 1000 && s.st.player.alive && !s.st.complete; i++)
        sim_tick(&s, (input_t){false, false});
    CHECK(s.st.player.alive);
    CHECK(s.st.complete);
    CHECK(s.st.player.pos.x >= 3100.0 + LEVEL_END_PADDING);
    CHECK(sim_percent(&s) == 100.0);
    sim_free(&s);
}

/* A corridor mode opens its corridor at the spawn, as a portal would (5.2). */
static void test_start_gamemode(void)
{
    sim_t s;

    load(&s, "start_gamemode ship\nstart_y 100\nblock 5000 700 2\n");
    CHECK(warnings == 0);
    CHECK(s.st.player.mode == MODE_SHIP);
    CHECK(s.st.bounds.active);
    CHECK(s.st.bounds.top == -400.0 && s.st.bounds.bottom == 600.0);
    CHECK(s.st.cam.pos.y == -400.0 - (VIEW_HEIGHT - 1000.0) / 2.0);
    sim_free(&s);
    /* a ship at the default height rests on the corridor's floor: the ground */
    load(&s, "start_gamemode ship\nblock 5000 700 2\n");
    CHECK(s.st.bounds.bottom == GROUND_Y);
    CHECK(s.st.player.grounded && s.st.player.can_jump);
    sim_free(&s);
    /* an unknown mode warns and the level starts as a cube */
    load(&s, "start_gamemode spider\nblock 5000 700 2\n");
    CHECK(warnings == 1);
    CHECK(s.st.player.mode == MODE_CUBE);
    sim_free(&s);
}

/* start_gravity flips the whole attempt: the player falls upward (FEATURES 9). */
static void test_start_gravity(void)
{
    sim_t s;
    double y;

    load(&s, "start_gravity flipped\nstart_y 400\nblock 5000 700 2\n");
    CHECK(warnings == 0);
    CHECK(s.st.player.gravity_dir == -1);
    CHECK(s.st.player.support_normal.y == 1.0);
    y = s.st.player.pos.y;
    for (int i = 0; i < 10; i++)
        sim_tick(&s, (input_t){false, false});
    CHECK(s.st.player.pos.y < y);                            /* falling up */
    sim_free(&s);
    load(&s, "start_gravity sideways\nblock 5000 700 2\n");
    CHECK(warnings == 1);
    CHECK(s.st.player.gravity_dir == 1);
    sim_free(&s);
    load(&s, "start_gravity flipped\nstart_gravity normal\n");  /* last wins */
    CHECK(warnings == 0 && s.st.player.gravity_dir == 1);
    sim_free(&s);
    load(&s, "start_gravity flipped\nstart_gravity sideways\n");
    CHECK(warnings == 1 && s.st.player.gravity_dir == -1);    /* kept */
    sim_free(&s);
}

/*
** Flipped, the ceiling is the floor: seams hold, the jump is the same 213.32
** px and 102 ticks mirrored, a 25 px step is climbed and 35 px kills (4.4).
*/
static void test_flipped_walking(void)
{
    char text[1024] = "start_gravity flipped\nstart_y 450\nblock 9000 0 2\n";
    char line[64];
    sim_t s;
    double deepest = 0.0;
    int airtime = 1;

    for (int i = 0; i < 30; i++) {
        snprintf(line, sizeof(line), "block %d 300 2\n", 100 * i);
        strcat(text, line);
    }
    load(&s, text);
    while (s.st.player.alive && s.st.player.pos.x < 2900.0) {
        sim_tick(&s, (input_t){false, false});
        CHECK(s.st.player.grounded && s.st.player.pos.y == 450.0);
    }
    sim_free(&s);
    load(&s, "start_gravity flipped\nstart_y 450\nblock 0 300 2 w=60 h=2\n"
        "block 9000 0 2\n");
    for (int i = 0; i < 10; i++)              /* it starts in the air (4.3) */
        sim_tick(&s, (input_t){false, false});
    CHECK(s.st.player.grounded && s.st.player.can_jump);
    sim_tick(&s, (input_t){true, true});
    while (!s.st.player.grounded && s.st.player.alive && airtime < 400) {
        deepest = fmax(deepest, s.st.player.pos.y - 450.0);
        sim_tick(&s, (input_t){false, false});
        airtime += 1;
    }
    CHECK(fabs(deepest - 213.32) < 0.05 && airtime == 102);
    CHECK(s.st.player.pos.y == 450.0);
    sim_free(&s);
    load(&s, "start_gravity flipped\nstart_y 450\nblock 0 300 2 w=20 h=2\n"
        "block 1000 325 2 w=10 h=2\nblock 9000 0 2\n");
    while (s.st.player.alive && s.st.player.pos.x < 1300.0)
        sim_tick(&s, (input_t){false, false});
    CHECK(s.st.player.alive && s.st.player.pos.y == 475.0);
    sim_free(&s);
    load(&s, "start_gravity flipped\nstart_y 450\nblock 0 300 2 w=20 h=2\n"
        "block 1000 335 2 w=10 h=2\nblock 9000 0 2\n");
    while (s.st.player.alive && s.st.player.pos.x < 1300.0)
        sim_tick(&s, (input_t){false, false});
    CHECK(!s.st.player.alive);
    sim_free(&s);
}

/* start_speed takes a speed portal's own values (FEATURES 10.3). */
static void test_start_speed(void)
{
    sim_t s;

    load(&s, "start_speed 4\nblock 5000 700 2\n");
    CHECK(warnings == 0);
    CHECK(s.st.speed_mult == 4.0);
    CHECK(s.st.player.vx == PER_TICK(SCROLL_SPEED) * 4.0);
    sim_tick(&s, (input_t){false, false});
    CHECK(fabs(s.st.distance - PER_TICK(SCROLL_SPEED) * 4.0) < 1e-9);
    sim_free(&s);
    load(&s, "start_speed 0.5\nblock 5000 700 2\n");
    CHECK(s.st.speed_mult == 0.5);
    sim_free(&s);
    load(&s, "start_speed 7\nblock 5000 700 2\n");            /* not a GD speed */
    CHECK(warnings == 1);
    CHECK(s.st.speed_mult == 1.0);
    sim_free(&s);
}

/* start_size is parsed, and says out loud that the mini scale isn't there. */
static void test_start_size(void)
{
    sim_t s;

    load(&s, "start_size mini\nblock 5000 700 2\n");
    CHECK(warnings == 1);                                    /* not implemented */
    CHECK(s.lvl.hdr.start.mini);
    sim_free(&s);
    load(&s, "start_size normal\nblock 5000 700 2\n");
    CHECK(warnings == 0);
    CHECK(!s.lvl.hdr.start.mini);
    sim_free(&s);
    load(&s, "start_size mini\nstart_size normal\n");         /* last wins */
    CHECK(warnings == 0 && !s.lvl.hdr.start.mini);
    sim_free(&s);
}

/* Every retry starts from the same place, whatever the run did (3.4). */
static void test_reset_returns_to_the_start(void)
{
    sim_t s;

    load(&s, "start_x 2000\nstart_gamemode ship\nstart_y 100\n"
        "block 5000 700 2\n");
    for (int i = 0; i < 100 && s.st.player.alive; i++)
        sim_tick(&s, (input_t){true, false});
    sim_reset(&s);
    CHECK(s.st.player.pos.x == 2000.0 && s.st.player.pos.y == 100.0);
    CHECK(s.st.player.mode == MODE_SHIP);
    CHECK(s.st.bounds.top == -400.0);
    CHECK(s.st.distance == 0.0 && s.st.tick == 0);
    sim_free(&s);
}

void test_start(void)
{
    test_defaults();
    test_start_position();
    test_start_gamemode();
    test_start_gravity();
    test_flipped_walking();
    test_start_speed();
    test_start_size();
    test_reset_returns_to_the_start();
}
