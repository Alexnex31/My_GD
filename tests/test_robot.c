/*
** ALEXNEX PROJECT, 2026
** tests/test_robot.c
** File description:
** the robot: it rises while the button is held, then falls (FEATURES 10.5)
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sim/bot.h"
#include "sim/internal.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

#define NONE ((input_t){false, false})
#define PRESS ((input_t){true, true})
#define HELD ((input_t){true, false})
#define ROBOT "start_gamemode robot\n"
#define FAR "block 60000 750 2\n"             /* a level long enough  */
#define RISE (PER_TICK(ROBOT_RISE_V))

static void load(sim_t *s, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

/* A cube's body and fall, no corridor, and a death on a ceiling. */
static void test_row(void)
{
    const mode_ops_t *m = &MODES[MODE_ROBOT];
    sim_t s;

    CHECK(mode_from_name("robot") == MODE_ROBOT);
    CHECK(m->half == PLAYER_HALF && m->inner_half == PLAYER_INNER_HALF);
    CHECK(m->gravity == MODES[MODE_CUBE].gravity);
    CHECK(m->max_fall == MODES[MODE_CUBE].max_fall);
    CHECK(m->corridor_height == 0.0 && m->head_restitution < 0.0);
    CHECK(RISE == 4.3275);                   /* the normal scroll speed: 45 deg */
    load(&s, ROBOT "portal 3000 650 2 robot\n" FAR);
    CHECK(s.lvl.skipped_lines == 0 && s.st.player.mode == MODE_ROBOT);
    CHECK(s.lvl.objects[0].portal_mode == MODE_ROBOT);
    CHECK(!s.st.bounds.active && s.st.player.grounded);
    CHECK(s.st.player.boost_ticks == 0);
    sim_free(&s);
}

/* What the fall adds once the rise is over: each tick's speed, while it rises. */
static double coast(void)
{
    double g = MODES[MODE_ROBOT].gravity;
    double sum = 0.0;

    for (double v = RISE - g; v > 0.0; v -= g)
        sum += v;
    return sum;
}

/*
** A jump with the button down for `hold` ticks: how high it went, and how
** many ticks it rose at the steady speed.
*/
static double jump(sim_t *s, int hold, int *steady)
{
    double from = s->st.player.pos.y;
    double apex = 0.0;
    int g = s->st.player.gravity_dir;

    *steady = 0;
    for (int t = 0; t < 2000; t++) {
        sim_tick(s, t == 0 ? PRESS : (t < hold ? HELD : NONE));
        if (!s->st.player.alive || (t > 0 && s->st.player.grounded))
            break;
        *steady += s->st.player.vy == RISE;
        apex = fmax(apex, (from - s->st.player.pos.y) * g);
    }
    return apex;
}

/*
** Held all the way: 68 ticks at the steady speed, 4.3275 px each, then the
** fall's own arc on top. GD's robot reaches 3.5111 blocks.
*/
static void test_full_hold(void)
{
    sim_t s;
    int steady = 0;
    double apex;
    double y;

    load(&s, ROBOT FAR);
    y = s.st.player.pos.y;
    sim_tick(&s, PRESS);
    CHECK(s.st.player.vy == RISE && s.st.player.pos.y == y - RISE);
    CHECK(s.st.player.boost_ticks == ROBOT_BOOST_TICKS - 1);
    CHECK(s.st.player.hold == HOLD_USED && !s.st.player.grounded);
    for (int t = 1; t < ROBOT_BOOST_TICKS; t++) {
        sim_tick(&s, HELD);
        CHECK(s.st.player.vy == RISE);       /* no gravity while it rises */
    }
    CHECK(fabs(y - s.st.player.pos.y - ROBOT_BOOST_TICKS * RISE) < 1e-9);
    CHECK(s.st.player.boost_ticks == 0);
    sim_tick(&s, HELD);                      /* the limit: it falls from here */
    CHECK(s.st.player.vy == RISE - MODES[MODE_ROBOT].gravity);
    sim_free(&s);
    load(&s, ROBOT FAR);
    apex = jump(&s, 2000, &steady);
    CHECK(steady == ROBOT_BOOST_TICKS);
    CHECK(fabs(apex - ROBOT_BOOST_TICKS * RISE - coast()) < 1e-6);
    CHECK(fabs(apex - 349.20) < 0.01);       /* GD's 3.5111 blocks, less 2 px */
    CHECK(s.st.player.alive && s.st.player.grounded);
    sim_free(&s);
}

/* Let go early and it falls at once: the height follows the hold. */
static void test_release(void)
{
    sim_t s;
    int steady = 0;
    double tap;
    double half;

    load(&s, ROBOT FAR);
    tap = jump(&s, 1, &steady);
    CHECK(steady == 1);                      /* the takeoff's tick only */
    CHECK(fabs(tap - RISE - coast()) < 1e-6 && tap < 60.0);
    sim_reset(&s);
    half = jump(&s, 30, &steady);
    CHECK(steady == 30);
    CHECK(fabs(half - 30.0 * RISE - coast()) < 1e-6);
    CHECK(half > tap + 100.0 && half < 349.0 - 100.0);
    sim_free(&s);
}

/* One rise per jump: a second press in the air does nothing. */
static void test_no_second_rise(void)
{
    sim_t s;
    double vy;

    load(&s, ROBOT FAR);
    sim_tick(&s, PRESS);
    for (int t = 0; t < 10; t++)
        sim_tick(&s, HELD);
    sim_tick(&s, NONE);                      /* let go */
    CHECK(s.st.player.boost_ticks == 0 && s.st.player.vy < RISE);
    vy = s.st.player.vy;
    sim_tick(&s, PRESS);                     /* again, in the air */
    CHECK(s.st.player.boost_ticks == 0);
    CHECK(s.st.player.vy == vy - MODES[MODE_ROBOT].gravity);
    sim_tick(&s, HELD);
    CHECK(fabs(s.st.player.vy - vy + 2.0 * MODES[MODE_ROBOT].gravity) < 1e-12);
    sim_free(&s);
}

/* A button still down at the landing jumps again on the next tick, a full
** rise each time (PLAN 3.4). */
static void test_held_through_landing(void)
{
    sim_t s;
    int landings = 0;
    int rises = 0;

    load(&s, ROBOT FAR);
    sim_tick(&s, PRESS);
    for (int t = 0; t < 1200 && s.st.player.alive; t++) {
        bool was = s.st.player.grounded;

        sim_tick(&s, HELD);
        landings += !was && s.st.player.grounded;
        rises += s.st.player.boost_ticks == ROBOT_BOOST_TICKS - 1;
        if (was)                             /* never two ticks on the ground */
            CHECK(!s.st.player.grounded);
    }
    CHECK(s.st.player.alive && landings >= 3 && rises == landings);
    sim_free(&s);
}

/* As high at any speed: the rise is tied to the normal one (6.9). */
static void test_any_speed(void)
{
    sim_t s;
    int steady = 0;
    double normal;
    double fast;

    load(&s, ROBOT FAR);
    normal = jump(&s, 2000, &steady);
    sim_free(&s);
    load(&s, ROBOT "start_speed 4\n" FAR);
    fast = jump(&s, 2000, &steady);
    CHECK(fast == normal && steady == ROBOT_BOOST_TICKS);
    CHECK(s.st.player.vx == 8.0);
    sim_free(&s);
}

/* Upside down it is the same jump, toward the screen's bottom. */
static void test_flipped(void)
{
    sim_t s;
    int steady = 0;
    double up;
    double down;

    load(&s, ROBOT FAR);
    up = jump(&s, 2000, &steady);
    sim_free(&s);
    load(&s, ROBOT "start_gravity flipped\nstart_y 150\n"
        "block 0 0 2 w=1200 h=2\n");         /* standing under a ceiling */
    CHECK(s.st.player.gravity_dir == -1);
    sim_tick(&s, NONE);
    CHECK(s.st.player.grounded && s.st.player.pos.y == 150.0);
    down = jump(&s, 2000, &steady);
    CHECK(fabs(down - up) < 1e-9 && steady == ROBOT_BOOST_TICKS);
    CHECK(s.st.player.alive);
    sim_free(&s);
}

/* What takes over its speed ends the rise: a ceiling, a pad, a portal. */
static void test_rise_ends(void)
{
    sim_t s;

    load(&s, ROBOT "block 300 450 2 w=20 h=2\n" FAR);      /* 200 px overhead */
    sim_tick(&s, PRESS);
    for (int t = 0; t < 200 && s.st.player.alive; t++)
        sim_tick(&s, HELD);
    CHECK(!s.st.player.alive);               /* its head, like a cube's */
    sim_free(&s);
    load(&s, ROBOT "pad 500 550 2 pink rot=180\n" FAR);    /* in its way up */
    sim_tick(&s, PRESS);
    while (!is_spent(&s.st, 0) && s.st.tick < 400)
        sim_tick(&s, HELD);
    CHECK(is_spent(&s.st, 0) && s.st.player.boost_ticks == 0);
    CHECK(s.st.player.vy == PER_TICK(1.79 * V_UNIT));
    sim_tick(&s, HELD);
    CHECK(s.st.player.vy < PER_TICK(1.79 * V_UNIT));       /* it falls again */
    sim_free(&s);
    load(&s, ROBOT "portal 450 600 2 cube\n" FAR);
    sim_tick(&s, PRESS);
    while (s.st.player.mode == MODE_ROBOT && s.st.tick < 400)
        sim_tick(&s, HELD);
    CHECK(s.st.player.mode == MODE_CUBE && s.st.player.boost_ticks == 0);
    sim_free(&s);
    load(&s, ROBOT "gravity 450 600 2 up\n" FAR);
    sim_tick(&s, PRESS);
    while (s.st.player.gravity_dir == 1 && s.st.tick < 400)
        sim_tick(&s, HELD);
    CHECK(s.st.player.gravity_dir == -1 && s.st.player.boost_ticks == 0);
    sim_free(&s);
}

/*
** The click that makes it jump is used from the takeoff on (PLAN 3.4): held,
** it rises through an orb without activating it, and the rise goes on. Only
** a new press, after letting go, takes an orb.
*/
static void test_ignores_orbs(void)
{
    static const char LEVEL[] = ROBOT "orb 500 550 2 yellow\n" FAR;
    sim_t s;
    bool inside = false;

    load(&s, LEVEL);
    sim_tick(&s, PRESS);
    CHECK(s.st.player.hold == HOLD_USED);
    for (int t = 1; t < ROBOT_BOOST_TICKS; t++) {
        sim_tick(&s, HELD);
        inside = inside || (fabs(s.st.player.pos.x - 550.0) < 40.0
            && fabs(s.st.player.pos.y - 600.0) < 40.0);
        CHECK(s.st.player.vy == RISE && s.st.player.hold == HOLD_USED);
    }
    CHECK(inside && !is_spent(&s.st, 0));    /* went through it, untouched */
    sim_free(&s);
    load(&s, LEVEL);
    sim_tick(&s, PRESS);
    for (int t = 0; t < 20; t++)
        sim_tick(&s, HELD);
    sim_tick(&s, NONE);                      /* let go under the orb */
    while (!is_spent(&s.st, 0) && s.st.tick < 200 && s.st.player.pos.x < 700.0)
        sim_tick(&s, s.st.player.pos.x + PLAYER_HALF > 520.0
            && s.st.player.hold == HOLD_NONE ? PRESS : NONE);
    CHECK(is_spent(&s.st, 0));               /* a new click does take it */
    CHECK(fabs(s.st.player.vy - PER_TICK(1.91 * V_UNIT))
        <= MODES[MODE_ROBOT].gravity);       /* the orb's jump, not a rise */
    CHECK(s.st.player.boost_ticks == 0 && s.st.player.hold == HOLD_USED);
    sim_free(&s);
}

/* The rise left is part of the run: hashed, saved and restored. */
static void test_state(void)
{
    sim_t s;
    sim_snapshot_t snap;
    uint64_t before;

    load(&s, ROBOT FAR);
    sim_tick(&s, PRESS);
    sim_snapshot_init(&snap, &s);
    sim_snapshot_save(&snap, &s);
    before = sim_physics_hash(&s);
    s.st.player.boost_ticks -= 1;
    CHECK(sim_physics_hash(&s) != before);
    sim_tick(&s, NONE);
    sim_snapshot_restore(&s, &snap);
    CHECK(sim_physics_hash(&s) == before);
    CHECK(s.st.player.boost_ticks == ROBOT_BOOST_TICKS - 1);
    sim_reset(&s);
    CHECK(s.st.player.boost_ticks == 0);
    sim_snapshot_free(&snap);
    sim_free(&s);
}

static bool replays(sim_t *s, const bot_result_t *r)
{
    bool prev = false;

    sim_reset(s);
    for (long t = 0; t < r->ticks; t++) {
        sim_tick(s, (input_t){r->inputs[t], r->inputs[t] && !prev});
        prev = r->inputs[t];
        if (!s->st.player.alive)
            return false;
    }
    return s->st.complete;
}

/*
** A wall three blocks tall: over a cube's jump, under a robot's. And a spike
** under a low ceiling too thick to jump onto, which only a short hop passes:
** the bot has to let go early, so it decides while the robot rises too.
*/
static void test_robot_bot(void)
{
    static const char WALL[] = "block 1800 550 2 h=6\n";
    static const char HOP[] = "block 1700 0 2 w=10 h=12\nspike 2000 750 2\n";
    char text[256];
    sim_t s;
    bot_result_t r;

    load(&s, WALL);
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_NO_PATH);         /* as a cube */
    bot_result_free(&r);
    sim_free(&s);
    snprintf(text, sizeof(text), ROBOT "%s", WALL);
    load(&s, text);
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && replays(&s, &r));
    bot_result_free(&r);
    sim_free(&s);
    load(&s, HOP);
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_NO_PATH);         /* a cube's jump hits the ceiling */
    bot_result_free(&r);
    sim_free(&s);
    snprintf(text, sizeof(text), ROBOT "%s", HOP);
    load(&s, text);
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && replays(&s, &r));
    bot_result_free(&r);
    sim_free(&s);
}

void test_robot(void)
{
    test_row();
    test_full_hold();
    test_release();
    test_no_second_rise();
    test_held_through_landing();
    test_any_speed();
    test_flipped();
    test_rise_ends();
    test_ignores_orbs();
    test_state();
    test_robot_bot();
}
