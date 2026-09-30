/*
** ALEXNEX PROJECT, 2026
** tests/test_portal.c
** File description:
** portals act once, open corridors and never move the player (5.1, 5.2)
*/

#include <math.h>
#include <string.h>

#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

#define HELD ((input_t){true, false})
#define NONE ((input_t){false, false})

static void load(sim_t *s, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

static void run_until_mode(sim_t *s, gamemode_t mode, double stop_x)
{
    while (s->st.player.mode != mode && s->st.player.alive
        && s->st.player.pos.x < stop_x)
        sim_tick(s, NONE);
}

/* Drops the player at the portal's height just before it, then crosses it. */
static void cross_at(sim_t *s, double portal_x, double y)
{
    while (s->st.player.pos.x + PLAYER_HALF < portal_x - 20.0)
        sim_tick(s, NONE);
    s->st.player.pos.y = y;
    s->st.player.vy = 0.0;
    s->st.player.grounded = false;
    run_until_mode(s, MODE_SHIP, portal_x + 300.0);
}

/* One portal, one activation, and its hitbox is gone afterwards (5.1). */
static void test_portal_acts_once(void)
{
    sim_t s;
    int changes = 0;
    gamemode_t previous = MODE_CUBE;

    load(&s, "portal 1000 700 2 ship\n");   /* it overlaps the running player */
    for (int i = 0; i < 400 && s.st.player.alive; i++) {
        sim_tick(&s, NONE);
        changes += s.st.player.mode != previous;
        previous = s.st.player.mode;
    }
    CHECK(changes == 1);                      /* about 16 ticks of overlap */
    CHECK(s.st.player.mode == MODE_SHIP);
    CHECK(is_spent(&s.st, 0));
    sim_free(&s);
}

/* A portal is a ghost: it changes the mode and touches nothing else (5.2). */
static void test_portal_never_moves_the_player(void)
{
    sim_t s;
    double x;
    double y;
    double vy;
    int gravity_dir;

    load(&s, "portal 1000 700 2 ship\n");
    while (s.st.player.pos.x + PLAYER_HALF < 1000.0)
        sim_tick(&s, NONE);
    x = s.st.player.pos.x;
    y = s.st.player.pos.y;
    vy = s.st.player.vy;
    gravity_dir = s.st.player.gravity_dir;
    sim_tick(&s, NONE);                       /* the tick it enters the portal */
    CHECK(s.st.player.mode == MODE_SHIP);
    CHECK(s.st.player.pos.x == x + PER_TICK(SCROLL_SPEED));
    CHECK(fabs(s.st.player.pos.y - y) < 1.0); /* still on its own trajectory */
    CHECK(s.st.player.gravity_dir == gravity_dir);
    CHECK(fabs(s.st.player.vy - vy) < 1.0);
    sim_free(&s);
}

/* The corridor of 5.2: 1000 px for the ship, snapped, never below ground. */
static void test_corridor_bounds(void)
{
    sim_t s;

    load(&s, "portal 1000 700 4 ship\n");     /* center 800: pushed above ground */
    run_until_mode(&s, MODE_SHIP, 1300.0);
    CHECK(s.st.bounds.active);
    CHECK(s.st.bounds.top == -150.0 && s.st.bounds.bottom == 850.0);
    sim_free(&s);
    load(&s, "portal 1000 0 4 ship\n");       /* center 100: -400..600 */
    cross_at(&s, 1000.0, 100.0);
    CHECK(s.st.bounds.top == -400.0 && s.st.bounds.bottom == 600.0);
    sim_free(&s);
    load(&s, "portal 1000 275 4 ship\n");     /* center 375: snapped to -150 */
    cross_at(&s, 1000.0, 375.0);
    CHECK(s.st.bounds.top == -150.0 && s.st.bounds.bottom == 850.0);
    sim_free(&s);
}

/*
** The camera settles on the corridor and then stays locked on it (3.5). It
** is eased onto it, never cut: a portal that teleported the camera read as a
** violent fall on screen.
*/
static void test_camera_locks_on_the_corridor(void)
{
    sim_t s;
    double expected;
    double biggest = 0.0;
    double previous;

    load(&s, "portal 1000 0 4 ship\nblock 100000 700 2\n");
    cross_at(&s, 1000.0, 100.0);
    expected = s.st.bounds.top - (VIEW_HEIGHT - 1000.0) / 2.0;
    for (int i = 0; i < 240 && s.st.player.alive; i++) {
        previous = s.st.cam.pos.y;
        sim_tick(&s, i % 2 ? HELD : NONE);
        if (fabs(s.st.cam.pos.y - previous) > biggest)
            biggest = fabs(s.st.cam.pos.y - previous);
    }
    CHECK(biggest < 30.0);                    /* eased in, not teleported */
    CHECK(s.st.cam.pos.y == expected);        /* one second later: locked */
    for (int i = 0; i < 60 && s.st.player.alive; i++) {
        sim_tick(&s, i % 2 ? HELD : NONE);    /* whatever the player does */
        CHECK(s.st.cam.pos.y == expected);
    }
    sim_free(&s);
}

/* A second ship portal moves the corridor; a cube portal removes it (5.2). */
static void test_corridor_switch_and_exit(void)
{
    sim_t s;

    load(&s, "portal 1000 0 4 ship\nportal 2000 -400 20 ship h=14\n");
    cross_at(&s, 1000.0, 100.0);
    CHECK(s.st.bounds.top == -400.0);
    while (s.st.player.pos.x < 2100.0 && s.st.player.alive)
        sim_tick(&s, HELD);
    CHECK(s.st.player.mode == MODE_SHIP);
    CHECK(s.st.bounds.top == -550.0);         /* the second corridor: center -50 */
    CHECK(s.st.bounds.bottom == 450.0);
    CHECK(is_spent(&s.st, 1));
    sim_free(&s);
    load(&s, "portal 1000 0 4 ship\nportal 2000 -400 20 cube h=14\n");
    cross_at(&s, 1000.0, 100.0);
    while (s.st.player.mode == MODE_SHIP && s.st.player.alive
        && s.st.player.pos.x < 2200.0)
        sim_tick(&s, HELD);
    CHECK(s.st.player.mode == MODE_CUBE);
    CHECK(!s.st.bounds.active);               /* the boundaries are gone at once */
    sim_free(&s);
}

/* The corridor's floor and ceiling are surfaces: they hold, never kill (4.3). */
static void test_ship_flies_in_the_corridor(void)
{
    sim_t s;
    bool touched_ceiling = false;
    bool touched_floor = false;

    /* the far block keeps the level from ending mid-flight */
    load(&s, "portal 1000 0 4 ship\nblock 100000 700 2\n");
    cross_at(&s, 1000.0, 100.0);
    /* a corridor crossing takes about a second, so the phases are 300 ticks */
    for (int i = 0; i < 1800 && s.st.player.alive; i++) {
        sim_tick(&s, i % 600 < 300 ? HELD : NONE);
        touched_ceiling |= s.st.player.pos.y - PLAYER_HALF <= s.st.bounds.top + 1.0;
        touched_floor |= s.st.player.grounded;
        CHECK(s.st.player.pos.y - PLAYER_HALF >= s.st.bounds.top - 0.01);
        CHECK(s.st.player.pos.y + PLAYER_HALF <= s.st.bounds.bottom + 0.01);
    }
    CHECK(s.st.player.alive);                 /* boundaries never kill */
    CHECK(touched_ceiling && touched_floor);
    sim_free(&s);
}

/* Every attempt starts with every interactive object live again (5.1). */
static void test_reset_clears_spent(void)
{
    sim_t s;

    load(&s, "portal 1000 700 2 ship\n");
    run_until_mode(&s, MODE_SHIP, 1300.0);
    CHECK(is_spent(&s.st, 0));
    sim_reset(&s);
    CHECK(!is_spent(&s.st, 0));
    CHECK(s.st.player.mode == MODE_CUBE);
    CHECK(!s.st.bounds.active);
    run_until_mode(&s, MODE_SHIP, 1300.0);
    CHECK(s.st.player.mode == MODE_SHIP);     /* it acts again next attempt */
    sim_free(&s);
}

void test_portal(void)
{
    test_portal_acts_once();
    test_portal_never_moves_the_player();
    test_corridor_bounds();
    test_camera_locks_on_the_corridor();
    test_corridor_switch_and_exit();
    test_ship_flies_in_the_corridor();
    test_reset_clears_spent();
}
