/*
** ALEXNEX PROJECT, 2026
** tests/test_gravity.c
** File description:
** the mode table, gravity portals and what a flip keeps (FEATURES 6, 9.4)
*/

#include <math.h>
#include <string.h>

#include "sim/internal.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

#define NONE ((input_t){false, false})
#define PRESS ((input_t){true, true})

#define UP_PORTAL "gravity 1000 700 2 up\n"            /* across the ground run */
#define CEILING "block 1100 300 2 w=40 h=2\n"          /* its underside: y = 400 */
#define FAR "block 9000 750 2\n"                      /* a level long enough  */

static int warnings;

static void count_warning(const char *msg)
{
    (void)msg;
    warnings += 1;
}

static void load(sim_t *s, const char *text)
{
    warnings = 0;
    sim_load_mem(s, text, strlen(text), "1", count_warning);
}

static void run(sim_t *s, int ticks)
{
    for (int i = 0; i < ticks && s->st.player.alive; i++)
        sim_tick(s, NONE);
}

static void run_until_flipped(sim_t *s, int dir)
{
    for (int i = 0; i < 2000 && s->st.player.alive
        && s->st.player.gravity_dir != dir; i++)
        sim_tick(s, NONE);
}

/* Every row is whole: a missing function would crash the first tick (6.1). */
static void test_mode_table(void)
{
    mode_ops_t wave = {.half = 15.0, .inner_half = 15.0, .neutral_kills = true};

    for (int i = 0; i < MODE_COUNT; i++) {
        CHECK(MODES[i].name != NULL && mode_from_name(MODES[i].name) == i);
        CHECK(MODES[i].half > 0.0 && MODES[i].inner_half <= MODES[i].half);
        CHECK(MODES[i].apply_input != NULL && MODES[i].apply_forces != NULL);
        CHECK(MODES[i].update_rotation != NULL);
    }
    CHECK(mode_neutral_kill_half(&MODES[MODE_CUBE]) == PLAYER_INNER_HALF);
    CHECK(mode_neutral_kill_half(&wave) == 15.0);
    wave.half = 30.0;                         /* the whole square, not the box */
    CHECK(mode_neutral_kill_half(&wave) == 30.0);
}

/* gravity x y size up|down: a portal's shape, a direction, nothing else. */
static void test_parse(void)
{
    sim_t s;

    load(&s, "gravity 1000 700 2 up\ngravity 2000 700 2 down\n");
    CHECK(warnings == 0 && s.lvl.nb_objects == 2);
    CHECK(s.lvl.objects[0].type == OBJ_GRAVITY);
    CHECK(s.lvl.objects[0].portal_gravity == -1);
    CHECK(s.lvl.objects[1].portal_gravity == 1);
    CHECK(s.lvl.objects[0].rect.x == 990.0 && s.lvl.objects[0].rect.w == 120.0);
    CHECK(s.lvl.objects[0].rect.y == 610.0 && s.lvl.objects[0].rect.h == 280.0);
    CHECK(OBJ_CATEGORY[OBJ_GRAVITY] == CAT_INTERACTIVE);
    sim_free(&s);
    load(&s, "gravity 1000 700 2\ngravity 1000 700 2 sideways\n"
        "gravity 1000 700 2 ship\n");
    CHECK(warnings == 3 && s.lvl.nb_objects == 0);
    sim_free(&s);
}

/*
** A flip keeps the motion (PLAN 3.4): on the tick it happens, the player is
** exactly where it would have been without the portal, moving the same way
** on screen. Only what gravity does next differs.
*/
static void test_flip_keeps_the_motion(void)
{
    sim_t a;
    sim_t b;

    load(&a, UP_PORTAL);
    load(&b, "portal 9000 700 2 cube\n");     /* the same run, no flip */
    for (int i = 0; i < 2000 && a.st.player.gravity_dir == 1; i++) {
        sim_tick(&a, i == 80 ? PRESS : NONE);  /* in the air at the portal */
        sim_tick(&b, i == 80 ? PRESS : NONE);
    }
    CHECK(a.st.player.gravity_dir == -1 && a.st.player.alive);
    CHECK(!b.st.player.grounded && b.st.player.vy != 0.0);
    CHECK(a.st.player.pos.x == b.st.player.pos.x);
    CHECK(a.st.player.pos.y == b.st.player.pos.y);
    CHECK(a.st.player.vy == -b.st.player.vy);  /* the same speed on screen */
    CHECK(a.st.player.vx == b.st.player.vx);
    CHECK(a.st.player.mode == MODE_CUBE && !a.st.bounds.active);
    CHECK(!a.st.player.grounded && !a.st.player.can_jump);
    CHECK(a.st.player.hold == b.st.player.hold);
    sim_tick(&a, NONE);
    sim_tick(&b, NONE);
    CHECK(a.st.player.pos.y < b.st.player.pos.y);   /* now it falls upward */
    sim_free(&a);
    sim_free(&b);
}

/* The function itself (PLAN 3.4): the sign, the rise speed, and off the floor. */
static void test_flip_function(void)
{
    player_t p = {.pos = {10.0, 20.0}, .vx = 4.0, .vy = 3.0, .gravity_dir = 1,
        .grounded = true, .can_jump = true, .hold = HOLD_FRESH, .alive = true};

    player_flip_gravity(&p);
    CHECK(p.gravity_dir == -1 && p.vy == -3.0 && p.vx == 4.0);
    CHECK(!p.grounded && !p.can_jump);        /* its floor is the other way now */
    CHECK(p.pos.x == 10.0 && p.pos.y == 20.0 && p.hold == HOLD_FRESH);
    player_flip_gravity(&p);
    CHECK(p.gravity_dir == 1 && p.vy == 3.0);
}

/* It acts once, and one the player already agrees with is used up too. */
static void test_acts_once(void)
{
    sim_t s;

    load(&s, "gravity 1000 700 2 down\n");
    run(&s, 400);
    CHECK(s.st.player.alive && s.st.player.gravity_dir == 1);
    CHECK(is_spent(&s.st, 0) && s.st.player.pos.y == PLAYER_SPAWN_Y);
    sim_free(&s);
    load(&s, UP_PORTAL CEILING);
    run_until_flipped(&s, -1);
    CHECK(is_spent(&s.st, 0));
    sim_tick(&s, NONE);                       /* still inside it: no second flip */
    CHECK(s.st.player.gravity_dir == -1);
    sim_free(&s);
}

/* The underside of a block is a floor: exactly 50 px under it (6.5). */
static void test_lands_on_the_ceiling(void)
{
    sim_t s;

    load(&s, UP_PORTAL CEILING);
    run(&s, 500);
    CHECK(s.st.player.alive && s.st.player.gravity_dir == -1);
    CHECK(s.st.player.grounded && s.st.player.can_jump);
    CHECK(s.st.player.pos.y == 450.0);
    CHECK(s.st.player.vy == 0.0 && !s.st.bounds.active);
    sim_tick(&s, PRESS);                      /* and it jumps from there: down */
    sim_tick(&s, NONE);
    CHECK(s.st.player.pos.y > 450.0 && s.st.player.vy > 0.0);
    sim_free(&s);
}

/* Up, then down again: back on the ground it left. */
static void test_there_and_back(void)
{
    sim_t s;

    load(&s, UP_PORTAL CEILING "gravity 2000 400 2 down\n");
    run_until_flipped(&s, -1);
    run_until_flipped(&s, 1);
    CHECK(s.st.player.alive && s.st.player.pos.x > 1900.0);
    run(&s, 300);
    CHECK(s.st.player.alive && s.st.player.grounded);
    CHECK(s.st.player.pos.y == PLAYER_SPAWN_Y);
    sim_free(&s);
}

/* Nothing above a flipped cube: the kill ceiling ends the run (6.4). */
static void test_nothing_above(void)
{
    sim_t s;

    load(&s, UP_PORTAL FAR);
    run(&s, 2000);
    CHECK(!s.st.player.alive && !s.st.complete);
    CHECK(s.st.player.gravity_dir == -1);
    CHECK(s.st.player.pos.y - PLAYER_HALF < s.lvl.kill_y);
    sim_free(&s);
}

/*
** Gravity and gamemode are independent (6.6): a gravity portal leaves the
** ship its corridor, where it now rests against the ceiling; a mode portal
** leaves the gravity alone and takes the corridor away.
*/
static void test_independent_of_modes(void)
{
    sim_t s;
    ship_bounds_t before;

    load(&s, "start_gamemode ship\nstart_y 400\n" UP_PORTAL FAR);
    before = s.st.bounds;
    CHECK(before.active);
    run(&s, 900);
    CHECK(s.st.player.alive && s.st.player.gravity_dir == -1);
    CHECK(s.st.player.mode == MODE_SHIP && s.st.bounds.active);
    CHECK(s.st.bounds.top == before.top && s.st.bounds.bottom == before.bottom);
    CHECK(s.st.player.grounded);
    CHECK(s.st.player.pos.y == before.top + PLAYER_HALF);
    sim_free(&s);
    load(&s, "start_gamemode ship\nstart_gravity flipped\nstart_y 400\n"
        "portal 1000 -100 2 cube\n" FAR);   /* where it rides: y = -100 */
    while (s.st.player.alive && s.st.player.mode == MODE_SHIP
        && s.st.player.pos.x < 2000.0)
        sim_tick(&s, NONE);
    CHECK(s.st.player.mode == MODE_CUBE && s.st.player.gravity_dir == -1);
    CHECK(!s.st.bounds.active);
    sim_free(&s);
}

void test_gravity(void)
{
    test_mode_table();
    test_parse();
    test_flip_keeps_the_motion();
    test_flip_function();
    test_acts_once();
    test_lands_on_the_ceiling();
    test_there_and_back();
    test_nothing_above();
    test_independent_of_modes();
}
