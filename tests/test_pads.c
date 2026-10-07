/*
** ALEXNEX PROJECT, 2026
** tests/test_pads.c
** File description:
** jump pads: a launch on touch, the same whatever the player did (FEATURES 10.1)
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

#define FAR "block 30000 750 2\n"             /* a level long enough  */
#define UNITS(v) (PER_TICK((v) * V_UNIT))     /* GD's velocity units, per tick */

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

/* Ticks until the first object is spent; false when it never is. */
static bool run_to_the_pad(sim_t *s, input_t in)
{
    for (int i = 0; i < 2000 && s->st.player.alive && !s->st.complete; i++) {
        sim_tick(s, in);
        if (is_spent(&s->st, 0))
            return true;
    }
    return false;
}

/* The highest the player gets above where it is now, before it comes down. */
static double rise(sim_t *s)
{
    double from = s->st.player.pos.y;
    double top = from;

    for (int i = 0; i < 2000 && s->st.player.alive; i++) {
        sim_tick(s, NONE);
        top = fmin(top, s->st.player.pos.y);
        if (s->st.player.grounded)
            break;
    }
    return from - top;
}

/* pad x y size colour: a plate along the bottom of its cell, turned with it. */
static void test_parse(void)
{
    sim_t s;

    load(&s, "pad 1000 750 2 yellow\npad 2000 750 2 pink\npad 3000 750 2 red\n"
        "pad 4000 750 2 blue\npad 5000 0 2 yellow rot=180\n");
    CHECK(warnings == 0 && s.lvl.nb_objects == 5);
    CHECK(s.lvl.objects[0].type == OBJ_PAD);
    CHECK(s.lvl.objects[0].launch == LAUNCH_YELLOW);
    CHECK(s.lvl.objects[3].launch == LAUNCH_BLUE);
    CHECK(s.lvl.objects[0].rect.y == 750.0 && s.lvl.objects[0].rect.h == 100.0);
    CHECK(s.lvl.objects[0].hitbox.aabb.x == 1010.0);
    CHECK(s.lvl.objects[0].hitbox.aabb.w == 80.0);
    CHECK(s.lvl.objects[0].hitbox.aabb.y == 825.0);
    CHECK(s.lvl.objects[0].hitbox.aabb.h == 25.0);
    CHECK(s.lvl.objects[4].hitbox.aabb.y == 0.0);     /* hanging: at the top */
    CHECK(s.lvl.objects[4].hitbox.aabb.h == 25.0);
    CHECK(OBJ_CATEGORY[OBJ_PAD] == CAT_INTERACTIVE);
    sim_free(&s);
    load(&s, "pad 1000 750 2\npad 1000 750 2 green\npad 1000 750 2 black\n"
        "pad 1000 750 2 ship\n");
    CHECK(warnings == 4 && s.lvl.nb_objects == 0);
    sim_free(&s);
}

/* The table: GD's numbers, and which colours each object comes in. */
static void test_table(void)
{
    CHECK(LAUNCHES[LAUNCH_YELLOW].pad_v == 2.77);
    CHECK(LAUNCHES[LAUNCH_PINK].pad_v == 1.79);
    CHECK(LAUNCHES[LAUNCH_RED].pad_v == 3.65);
    CHECK(LAUNCHES[LAUNCH_BLUE].pad_v == -1.37 && LAUNCHES[LAUNCH_BLUE].flips);
    CHECK(LAUNCHES[LAUNCH_GREEN].pad_v == 0.0);
    CHECK(LAUNCHES[LAUNCH_BLACK].pad_v == 0.0);
    CHECK(fabs(2.77 * V_UNIT - 2876.9) < 0.05);
    CHECK(fabs(1.79 * V_UNIT - 1859.1) < 0.05);
    CHECK(fabs(3.65 * V_UNIT - 3790.9) < 0.05);
    for (int i = 0; i < LAUNCH_KIND_COUNT; i++)
        CHECK(launch_from_name(LAUNCHES[i].name) == i);
    CHECK(launch_from_name("purple") == -1);
}

/*
** 10.1: a cube thrown 4.38, 1.83 and 7.60 blocks high. Those are the heights
** of a continuous fall; ticks lose half a tick of the launch speed, 6 px on
** the yellow pad.
*/
static void test_heights(void)
{
    static const char *const level[] = {"pad 1000 750 2 yellow\n" FAR,
        "pad 1000 750 2 pink\n" FAR, "pad 1000 750 2 red\n" FAR};
    static const double speed[] = {2.77, 1.79, 3.65};
    static const double height[] = {438.0, 183.0, 760.0};
    double g = MODES[MODE_CUBE].gravity;
    double v;
    sim_t s;

    for (int i = 0; i < 3; i++) {
        load(&s, level[i]);
        CHECK(run_to_the_pad(&s, NONE));
        CHECK(s.st.player.vy == UNITS(speed[i]));   /* set on the touch */
        CHECK(!s.st.player.grounded && !s.st.player.can_jump);
        CHECK(s.st.player.pos.x + PLAYER_HALF - 1010.0 < PER_TICK(SCROLL_SPEED));
        v = UNITS(speed[i]);
        CHECK(fabs(v * v / (2.0 * g) - height[i]) < 1.0);   /* GD's figure */
        CHECK(fabs(rise(&s) - (height[i] - v / 2.0)) < 1.0);
        CHECK(s.st.player.alive && s.st.player.grounded);
        sim_free(&s);
    }
}

/* It sets the speed: the same launch for a cube falling onto it. */
static void test_sets_the_speed(void)
{
    sim_t s;

    load(&s, "pad 1000 750 2 yellow\n" FAR);
    for (int i = 0; i < 2000 && !is_spent(&s.st, 0); i++)
        sim_tick(&s, i == 60 ? PRESS : NONE);  /* a jump that lands on it */
    CHECK(is_spent(&s.st, 0) && s.st.player.pos.x > 1020.0);
    CHECK(s.st.player.vy == UNITS(2.77));
    sim_tick(&s, HELD);                        /* the launch is not a surface */
    CHECK(s.st.player.vy == UNITS(2.77) - MODES[MODE_CUBE].gravity);
    sim_free(&s);
}

/* Once, then it is spent: still there, no longer touched, live next attempt. */
static void test_acts_once(void)
{
    sim_t s;

    load(&s, "pad 1000 750 2 pink w=12\n" FAR);   /* long enough to land on */
    CHECK(run_to_the_pad(&s, NONE));
    rise(&s);
    CHECK(s.st.player.grounded && s.st.player.pos.x < 1600.0);
    CHECK(s.st.player.vy == 0.0);
    sim_tick(&s, NONE);
    CHECK(s.st.player.grounded && s.st.player.vy == 0.0);
    sim_reset(&s);
    CHECK(!is_spent(&s.st, 0) && run_to_the_pad(&s, NONE));
    sim_free(&s);
}

/* No mode's cap cuts it: a ship leaves a red pad at 3.65, above its own 1.0. */
static void test_never_capped(void)
{
    sim_t s;

    load(&s, "start_gamemode ship\npad 1000 750 2 red\n" FAR);
    CHECK(run_to_the_pad(&s, NONE));
    CHECK(s.st.player.vy == UNITS(3.65));
    CHECK(s.st.player.vy > PER_TICK(SHIP_MAX_VY));
    sim_tick(&s, HELD);                        /* thrust adds nothing above */
    CHECK(s.st.player.vy == UNITS(3.65) - MODES[MODE_SHIP].gravity);
    sim_free(&s);
    load(&s, "start_gamemode ufo\npad 1000 750 2 yellow\n" FAR);
    CHECK(run_to_the_pad(&s, NONE));
    CHECK(s.st.player.vy == UNITS(2.77));
    sim_free(&s);
}

/* Blue: the gravity flips, then a speed toward the new floor. */
static void test_blue(void)
{
    sim_t s;
    double y;

    load(&s, "pad 1000 750 2 blue\nblock 1100 300 2 w=20\n" FAR);
    CHECK(run_to_the_pad(&s, NONE));
    CHECK(s.st.player.gravity_dir == -1);
    CHECK(s.st.player.vy == UNITS(-1.37));
    y = s.st.player.pos.y;
    sim_tick(&s, NONE);
    CHECK(s.st.player.pos.y < y - 5.0);        /* thrown up the screen at once */
    for (int i = 0; i < 400 && !s.st.player.grounded; i++)
        sim_tick(&s, NONE);
    CHECK(s.st.player.alive && s.st.player.pos.y == 450.0);
    sim_free(&s);
}

/* Hanging from a ceiling, for a flipped player: it throws down the screen. */
static void test_on_a_ceiling(void)
{
    sim_t s;
    double y;

    load(&s, "start_gamemode ufo\nstart_gravity flipped\n"
        "pad 1400 -150 2 yellow rot=180\n" FAR);
    CHECK(run_to_the_pad(&s, NONE));
    CHECK(s.st.player.gravity_dir == -1 && s.st.player.vy == UNITS(2.77));
    y = s.st.player.pos.y;
    sim_tick(&s, NONE);
    CHECK(s.st.player.pos.y > y + 10.0);
    sim_free(&s);
    load(&s, "start_gamemode ufo\npad 1400 -150 2 blue rot=180\n" FAR);
    for (int i = 0; i < 400; i++)
        sim_tick(&s, NONE);                    /* on the floor: out of reach */
    CHECK(!is_spent(&s.st, 0));
    sim_free(&s);
}

/* The wave sets its own speed every tick: only a blue pad's flip shows. */
static void test_under_a_wave(void)
{
    sim_t s;
    double y;

    load(&s, "start_gamemode wave\npad 1000 750 2 yellow\n" FAR);
    CHECK(run_to_the_pad(&s, NONE));
    y = s.st.player.pos.y;
    sim_tick(&s, NONE);
    CHECK(s.st.player.pos.y == y);             /* still sliding on the ground */
    sim_free(&s);
    load(&s, "start_gamemode wave\npad 1000 750 2 blue\n" FAR);
    CHECK(run_to_the_pad(&s, NONE));
    y = s.st.player.pos.y;
    sim_tick(&s, NONE);
    CHECK(s.st.player.gravity_dir == -1);
    CHECK(s.st.player.pos.y == y - PER_TICK(SCROLL_SPEED));   /* its new floor */
    sim_free(&s);
}

void test_pads(void)
{
    test_parse();
    test_table();
    test_heights();
    test_sets_the_speed();
    test_acts_once();
    test_never_capped();
    test_blue();
    test_on_a_ceiling();
    test_under_a_wave();
}
