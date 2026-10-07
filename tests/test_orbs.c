/*
** ALEXNEX PROJECT, 2026
** tests/test_orbs.c
** File description:
** jump orbs: a fresh hold, on exactly the tick of the contact (FEATURES 10.2)
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sim/internal.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "sim/sweep.h"
#include "test.h"

#define NONE ((input_t){false, false})
#define PRESS ((input_t){true, true})
#define HELD ((input_t){true, false})
#define TAP ((input_t){false, true})

#define FAR "block 30000 750 2\n"             /* a level long enough  */
#define UNITS(v) (PER_TICK((v) * V_UNIT))     /* GD's velocity units, per tick */

/*
** A cube dropped from y = 300 falls through an orb whose box is 510..590
** high and 510..590 across: in the air, so nothing but the orb can answer.
*/
#define DROP "start_y 300\n"
#define ORB(colour) "orb 500 500 2 " colour "\n"

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

static bool inside(const sim_t *s, size_t i)
{
    return overlap_box_poly(s->st.player.pos, MODES[s->st.player.mode].half,
        &s->lvl.objects[i].hitbox);
}

/* The dropped cube, no button: the ticks it first and last overlaps the orb. */
static void orb_window(int *first, int *last)
{
    sim_t s;

    load(&s, DROP ORB("yellow") FAR);
    *first = -1;
    *last = -1;
    for (int i = 0; i < 200; i++) {
        sim_tick(&s, NONE);
        if (!inside(&s, 0))
            continue;
        *first = *first < 0 ? i + 1 : *first;  /* after tick i: before i + 1 */
        *last = i + 1;
    }
    CHECK(!is_spent(&s.st, 0) && s.st.player.alive);   /* flown through: live */
    sim_free(&s);
}

/* orb x y size colour: a box a little smaller than its cell. */
static void test_parse(void)
{
    sim_t s;

    load(&s, "orb 1000 500 2 yellow\norb 2000 500 2 pink\norb 3000 500 2 red\n"
        "orb 4000 500 2 blue\norb 5000 500 2 green\norb 6000 500 2 black\n");
    CHECK(warnings == 0 && s.lvl.nb_objects == 6);
    CHECK(s.lvl.objects[0].type == OBJ_ORB);
    CHECK(s.lvl.objects[5].launch == LAUNCH_BLACK);
    CHECK(s.lvl.objects[0].hitbox.aabb.x == 1010.0);
    CHECK(s.lvl.objects[0].hitbox.aabb.y == 510.0);
    CHECK(s.lvl.objects[0].hitbox.aabb.w == 80.0);
    CHECK(s.lvl.objects[0].hitbox.aabb.h == 80.0);
    CHECK(OBJ_CATEGORY[OBJ_ORB] == CAT_INTERACTIVE);
    CHECK(LAUNCHES[LAUNCH_YELLOW].orb_v == 1.91);
    CHECK(LAUNCHES[LAUNCH_PINK].orb_v == 1.37);
    CHECK(LAUNCHES[LAUNCH_RED].orb_v == 2.68);
    CHECK(LAUNCHES[LAUNCH_BLUE].orb_v == -1.37 && LAUNCHES[LAUNCH_BLUE].flips);
    CHECK(LAUNCHES[LAUNCH_GREEN].orb_v == 1.91 && LAUNCHES[LAUNCH_GREEN].flips);
    CHECK(LAUNCHES[LAUNCH_BLACK].orb_v == -2.6 && !LAUNCHES[LAUNCH_BLACK].flips);
    sim_free(&s);
    load(&s, "orb 1000 500 2\norb 1000 500 2 purple\norb 1000 500 2 up\n");
    CHECK(warnings == 3 && s.lvl.nb_objects == 0);
    sim_free(&s);
}

/*
** A press while already inside: the orb acts before anything else on that
** tick. Every colour, its own speed, and gravity from that tick on.
*/
static void test_press_inside(void)
{
    static const char *const level[] = {DROP ORB("yellow") FAR,
        DROP ORB("pink") FAR, DROP ORB("red") FAR, DROP ORB("blue") FAR,
        DROP ORB("green") FAR, DROP ORB("black") FAR};
    static const double speed[] = {1.91, 1.37, 2.68, -1.37, 1.91, -2.6};
    double g = MODES[MODE_CUBE].gravity;
    int first;
    int last;
    sim_t s;

    orb_window(&first, &last);
    CHECK(first > 20 && last > first + 5);
    for (int k = 0; k < 6; k++) {
        load(&s, level[k]);
        for (int i = 0; i < first + 2; i++)
            sim_tick(&s, NONE);
        CHECK(inside(&s, 0) && !is_spent(&s.st, 0));
        sim_tick(&s, PRESS);
        CHECK(is_spent(&s.st, 0) && s.st.player.hold == HOLD_USED);
        CHECK(s.st.player.gravity_dir == (k == 3 || k == 4 ? -1 : 1));
        if (k == 5)                            /* at the fall cap: no more */
            CHECK(s.st.player.vy == UNITS(-2.6));
        else
            CHECK(s.st.player.vy == UNITS(speed[k]) - g);
        sim_free(&s);
    }
}

/*
** A hold started before the orb and still down when the square reaches it:
** it acts on that very tick, not one earlier, not one later.
*/
static void test_buffered_on_the_exact_tick(void)
{
    int first;
    int last;
    sim_t s;

    orb_window(&first, &last);
    load(&s, DROP ORB("yellow") FAR);
    for (int i = 0; i < first - 1; i++) {
        sim_tick(&s, i == 5 ? PRESS : i > 5 ? HELD : NONE);
        CHECK(!is_spent(&s.st, 0));
    }
    CHECK(s.st.player.hold == HOLD_FRESH);     /* in the air: nothing used it */
    sim_tick(&s, HELD);                        /* the tick that reaches it */
    CHECK(is_spent(&s.st, 0) && s.st.player.hold == HOLD_USED);
    CHECK(s.st.player.vy == UNITS(1.91));      /* set after the move (step 5) */
    sim_free(&s);
}

/* No press memory: released before the contact, the click is lost. */
static void test_released_before(void)
{
    int first;
    int last;
    sim_t s;

    orb_window(&first, &last);
    load(&s, DROP ORB("yellow") FAR);
    for (int i = 0; i < last + 20; i++)
        sim_tick(&s, i == 5 ? PRESS : i > 5 && i < first - 2 ? HELD : NONE);
    CHECK(!is_spent(&s.st, 0) && s.st.player.alive);
    sim_free(&s);
}

/* A hold that already jumped is used: it passes through, a new press acts. */
static void test_used_hold(void)
{
    sim_t s;
    bool was_inside = false;

    load(&s, "orb 650 550 2 yellow\n" FAR);    /* on the way up of a jump */
    sim_tick(&s, PRESS);
    CHECK(s.st.player.hold == HOLD_USED);
    for (int i = 0; i < 60; i++) {
        sim_tick(&s, HELD);
        was_inside = was_inside || inside(&s, 0);
    }
    CHECK(was_inside && !is_spent(&s.st, 0));
    sim_reset(&s);
    sim_tick(&s, PRESS);
    for (int i = 0; i < 200 && !inside(&s, 0); i++)
        sim_tick(&s, HELD);
    sim_tick(&s, NONE);                        /* released, then pressed */
    CHECK(inside(&s, 0));
    sim_tick(&s, PRESS);
    CHECK(is_spent(&s.st, 0));
    sim_free(&s);
}

/* One orb per hold: the second of two wants a press of its own. */
static void test_one_orb_per_hold(void)
{
    int first;
    int last;
    sim_t s;

    orb_window(&first, &last);
    load(&s, DROP ORB("black") "orb 500 600 2 yellow\n" FAR);
    for (int i = 0; i < first + 2; i++)
        sim_tick(&s, NONE);
    sim_tick(&s, PRESS);                       /* the black one: slammed down */
    CHECK(is_spent(&s.st, 0) && !is_spent(&s.st, 1));
    for (int i = 0; i < 40 && !inside(&s, 1); i++)
        sim_tick(&s, HELD);
    CHECK(inside(&s, 1));
    sim_tick(&s, HELD);
    CHECK(!is_spent(&s.st, 1));                /* the same hold: nothing */
    sim_tick(&s, NONE);
    sim_tick(&s, PRESS);
    CHECK(is_spent(&s.st, 1) && s.st.player.vy > 0.0);
    sim_free(&s);
}

/*
** Orb first: on the ground inside an orb, a press is the orb's and not a
** jump. The hold is then used, and still jumps off the surface it lands on.
*/
static void test_orb_before_the_surface_jump(void)
{
    double g = MODES[MODE_CUBE].gravity;
    sim_t s;

    load(&s, "orb 1000 750 2 red\n" FAR);
    while (!inside(&s, 0))
        sim_tick(&s, NONE);
    sim_tick(&s, NONE);
    CHECK(s.st.player.grounded && s.st.player.can_jump);
    sim_tick(&s, PRESS);
    CHECK(is_spent(&s.st, 0));
    CHECK(s.st.player.vy == UNITS(2.68) - g);  /* not the jump's 1.9522 */
    for (int i = 0; i < 600 && !s.st.player.grounded; i++)
        sim_tick(&s, HELD);
    CHECK(s.st.player.grounded);
    sim_tick(&s, HELD);                        /* a used hold still jumps */
    CHECK(s.st.player.vy == PER_TICK(CUBE_JUMP_V) - g);
    sim_free(&s);
}

/* The other modes: the orb takes the click a hop or a flip would have had. */
static void test_other_modes(void)
{
    sim_t s;

    load(&s, "start_gamemode ufo\norb 1000 750 2 red\n" FAR);
    while (!inside(&s, 0))
        sim_tick(&s, NONE);
    sim_tick(&s, PRESS);
    CHECK(is_spent(&s.st, 0));
    CHECK(s.st.player.vy == UNITS(2.68) - MODES[MODE_UFO].gravity);
    sim_free(&s);
    load(&s, "start_gamemode ball\norb 1000 750 2 yellow\n" FAR);
    while (!inside(&s, 0))
        sim_tick(&s, NONE);
    sim_tick(&s, PRESS);
    CHECK(is_spent(&s.st, 0) && s.st.player.gravity_dir == 1);   /* no flip */
    CHECK(s.st.player.vy == UNITS(1.91) - MODES[MODE_BALL].gravity);
    sim_free(&s);
    load(&s, "start_gamemode ball\norb 1000 750 2 blue\n" FAR);
    while (!inside(&s, 0))
        sim_tick(&s, NONE);
    sim_tick(&s, PRESS);                       /* the orb's flip, not its own */
    CHECK(s.st.player.gravity_dir == -1);
    CHECK(s.st.player.vy == UNITS(-1.37) - MODES[MODE_BALL].gravity);
    sim_free(&s);
}

/*
** Green is a yellow orb in the other gravity: the flip, then a jump away
** from the new floor. On screen the cube first drops, then falls up. Blue
** throws it straight at its new floor.
*/
static void test_green_jumps_blue_throws(void)
{
    int first;
    int last;
    sim_t s;
    double y;

    orb_window(&first, &last);
    load(&s, DROP ORB("green") FAR);
    for (int i = 0; i < first + 2; i++)
        sim_tick(&s, NONE);
    y = s.st.player.pos.y;
    sim_tick(&s, PRESS);
    CHECK(s.st.player.gravity_dir == -1 && s.st.player.vy > 0.0);
    CHECK(s.st.player.pos.y > y + 5.0);        /* away from its new floor */
    for (int i = 0; i < 60; i++)
        sim_tick(&s, NONE);
    CHECK(s.st.player.vy < 0.0 && s.st.player.alive);   /* then it falls up */
    sim_free(&s);
    load(&s, DROP ORB("blue") FAR);
    for (int i = 0; i < first + 2; i++)
        sim_tick(&s, NONE);
    y = s.st.player.pos.y;
    sim_tick(&s, PRESS);
    CHECK(s.st.player.gravity_dir == -1 && s.st.player.vy < 0.0);
    CHECK(s.st.player.pos.y < y - 5.0);        /* straight up the screen */
    sim_free(&s);
}

/* A ship's hold is never used by flying: held for a while, it takes the orb. */
static void test_ship(void)
{
    sim_t s;

    load(&s, "start_gamemode ship\nstart_y 300\norb 1500 -150 2 red\n" FAR);
    for (int i = 0; i < 600 && !is_spent(&s.st, 0); i++)
        sim_tick(&s, (input_t){true, i == 0});   /* up along the ceiling */
    CHECK(is_spent(&s.st, 0) && s.st.player.alive);
    CHECK(s.st.player.hold == HOLD_USED);
    sim_free(&s);
}

void test_orbs(void)
{
    test_parse();
    test_press_inside();
    test_buffered_on_the_exact_tick();
    test_released_before();
    test_used_hold();
    test_one_orb_per_hold();
    test_orb_before_the_surface_jump();
    test_other_modes();
    test_green_jumps_blue_throws();
    test_ship();
}
