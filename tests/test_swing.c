/*
** ALEXNEX PROJECT, 2026
** tests/test_swing.c
** File description:
** the swing: each press turns its gravity round (FEATURES 10.5)
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sim/bot.h"
#include "sim/internal.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "sim/sweep.h"
#include "test.h"

#define NONE ((input_t){false, false})
#define PRESS ((input_t){true, true})
#define HELD ((input_t){true, false})
#define SWING "start_gamemode swing\n"
#define FAR "block 60000 750 2\n"             /* a level long enough  */
#define SWING_CROSS_TICKS 235           /* 0.98 s, as the ship falls */
#define TOP (GROUND_Y - 1000.0 + PLAYER_HALF)  /* resting on the ceiling */

static void load(sim_t *s, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

/* The ship's body, fall, corridor, bounce and icon; a press of its own. */
static void test_row(void)
{
    const mode_ops_t *m = &MODES[MODE_SWING];
    sim_t s;

    CHECK(mode_from_name("swing") == MODE_SWING);
    CHECK(m->half == PLAYER_HALF && m->inner_half == PLAYER_INNER_HALF);
    CHECK(m->gravity == MODES[MODE_SHIP].gravity);
    CHECK(m->max_fall == MODES[MODE_SHIP].max_fall);
    CHECK(m->corridor_height == 1000.0);
    CHECK(m->head_restitution == BOUNCE_RESTITUTION_SHIP);
    CHECK(m->bot_decision_ticks == 12 && !m->neutral_kills);
    CHECK(m->apply_input == swing_input);
    CHECK(m->apply_forces == player_apply_gravity);
    CHECK(m->update_rotation == ship_rotation);
    load(&s, SWING "portal 3000 650 2 swing\n" FAR);
    CHECK(s.lvl.skipped_lines == 0 && s.st.player.mode == MODE_SWING);
    CHECK(s.lvl.objects[0].portal_mode == MODE_SWING);
    CHECK(s.st.bounds.active && s.st.bounds.bottom == GROUND_Y);
    CHECK(s.st.bounds.top == GROUND_Y - 1000.0);
    CHECK(s.st.player.grounded && s.st.player.gravity_dir == 1);
    sim_free(&s);
}

/* Ticks from a press until it rests on the other side of the corridor. */
static int cross(sim_t *s, input_t after, double *fastest)
{
    int t = 0;

    *fastest = 0.0;
    sim_tick(s, PRESS);
    for (t = 1; t < 1000 && !s->st.player.grounded; t++) {
        *fastest = fmax(*fastest, fabs(s->st.player.pos.y
            - s->st.player.prev_pos.y));
        sim_tick(s, after);
    }
    return t;
}

/*
** A press on the floor sends it to the ceiling, no push: its gravity turns
** round and it falls there, at the ship's 45-degree cap at most. The next
** press brings it back down in the same time.
*/
static void test_crossing(void)
{
    double cap = MODES[MODE_SWING].max_fall;
    double fastest;
    int up;
    int down;
    sim_t s;

    load(&s, SWING FAR);
    up = cross(&s, NONE, &fastest);
    CHECK(s.st.player.alive && s.st.player.grounded);
    CHECK(s.st.player.gravity_dir == -1 && s.st.player.pos.y == TOP);
    CHECK(fabs(fastest - cap) < 1e-9);       /* it reached the cap, no more */
    down = cross(&s, NONE, &fastest);
    CHECK(s.st.player.gravity_dir == 1);
    CHECK(s.st.player.pos.y == PLAYER_SPAWN_Y);
    CHECK(up == down);
    CHECK(up == SWING_CROSS_TICKS);
    sim_free(&s);
}

/* Holding does nothing: one flip per press, and none for a carried hold. */
static void test_holding_does_nothing(void)
{
    double fastest;
    int up;
    sim_t s;

    load(&s, SWING FAR);
    up = cross(&s, HELD, &fastest);
    CHECK(up == SWING_CROSS_TICKS && s.st.player.gravity_dir == -1);
    for (int i = 0; i < 300; i++) {
        sim_tick(&s, HELD);
        CHECK(s.st.player.gravity_dir == -1 && s.st.player.pos.y == TOP);
    }
    sim_free(&s);
    load(&s, SWING FAR);                     /* a hold carried into the attempt */
    for (int i = 0; i < 100; i++)
        sim_tick(&s, HELD);
    CHECK(s.st.player.grounded && s.st.player.gravity_dir == 1);
    CHECK(s.st.player.pos.y == PLAYER_SPAWN_Y);
    sim_free(&s);
}

/*
** A press in the air keeps the motion (3.4): on that tick the speed down the
** screen changes by one tick of gravity, the other way round, and nothing
** more. Each press flips, as often as it is pressed.
*/
static void test_air_flip(void)
{
    double g = MODES[MODE_SWING].gravity;
    double before;
    double after;
    sim_t s;

    load(&s, SWING FAR);
    sim_tick(&s, PRESS);
    for (int i = 0; i < 30; i++)
        sim_tick(&s, NONE);
    before = s.st.player.pos.y - s.st.player.prev_pos.y;
    CHECK(before < 0.0);                     /* going up the screen */
    sim_tick(&s, PRESS);
    after = s.st.player.pos.y - s.st.player.prev_pos.y;
    CHECK(s.st.player.gravity_dir == 1 && s.st.player.hold == HOLD_USED);
    CHECK(fabs(after - before - g) < 1e-9);  /* gravity down now, no jolt */
    for (int i = 0, dir = 1; i < 6; i++) {
        sim_tick(&s, i % 2 == 0 ? NONE : PRESS);   /* a new press each time */
        dir = i % 2 == 0 ? dir : -dir;
        CHECK(s.st.player.gravity_dir == dir);
    }
    sim_free(&s);
}

/*
** A fresh press inside an orb is the orb's (10.2): it launches and the
** gravity stays. A press that flipped is used, and an orb met while it is
** held does nothing.
*/
static void test_swing_orbs(void)
{
    static const char LEVEL[] = SWING "orb 700 750 2 yellow\n" FAR;
    sim_t s;
    bool inside = false;

    load(&s, LEVEL);
    while (s.st.player.pos.x + PLAYER_HALF < 720.0)  /* inside it */
        sim_tick(&s, NONE);
    sim_tick(&s, PRESS);
    CHECK(is_spent(&s.st, 0) && s.st.player.gravity_dir == 1);
    CHECK(s.st.player.vy > 0.0);             /* the orb's jump */
    sim_free(&s);
    load(&s, LEVEL);
    while (s.st.player.pos.x + PLAYER_HALF < 660.0)
        sim_tick(&s, NONE);
    sim_tick(&s, PRESS);                     /* a flip, just before it */
    for (int i = 0; i < 100; i++) {
        sim_tick(&s, HELD);
        inside = inside || overlap_box_poly(s.st.player.pos, PLAYER_HALF,
            &s.lvl.objects[0].hitbox);
    }
    CHECK(inside);                           /* it did go through it */
    CHECK(!is_spent(&s.st, 0) && s.st.player.gravity_dir == -1);
    sim_free(&s);
}

/* A block's underside sends it back with 30% of its speed; nothing dies. */
static void test_ceiling_bounce(void)
{
    sim_t s;
    double before = 0.0;

    load(&s, SWING "block 300 460 2 w=20\n" FAR);
    s.st.player.vy = PER_TICK(2000);         /* a pad's kind of speed */
    s.st.player.grounded = false;
    for (int i = 0; i < 60 && s.st.player.vy > 0.0; i++) {
        before = s.st.player.vy;
        sim_tick(&s, NONE);
    }
    CHECK(s.st.player.alive && s.st.player.vy < 0.0);
    CHECK(fabs(s.st.player.vy + before * BOUNCE_RESTITUTION_SHIP) < 0.2);
    sim_free(&s);
}

/*
** Its gravity is kept through a mode portal (6.1): a swing flipped at the
** cube portal is a flipped cube, without a corridor. A pad still launches it.
*/
static void test_portals(void)
{
    sim_t s;

    load(&s, SWING "portal 2000 -150 2 cube\n" FAR);
    sim_tick(&s, PRESS);
    while (s.st.player.alive && s.st.player.mode == MODE_SWING
        && s.st.tick < 1000)
        sim_tick(&s, NONE);
    CHECK(s.st.player.alive && s.st.player.mode == MODE_CUBE);
    CHECK(s.st.player.gravity_dir == -1 && !s.st.bounds.active);
    sim_free(&s);
    load(&s, "portal 1200 650 2 swing\n" FAR);
    while (s.st.player.mode == MODE_CUBE && s.st.tick < 1000)
        sim_tick(&s, NONE);
    CHECK(s.st.player.mode == MODE_SWING && s.st.bounds.active);
    CHECK(s.st.player.gravity_dir == 1);
    sim_free(&s);
    load(&s, SWING "pad 700 750 2 yellow\n" FAR);
    while (s.st.player.vy <= 0.0 && s.st.tick < 200)
        sim_tick(&s, NONE);
    CHECK(fabs(s.st.player.vy - PER_TICK(2.77 * V_UNIT))
        <= MODES[MODE_SWING].gravity);
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

/* A wall up from the floor, then one down from the ceiling: flip, flip back. */
static void test_swing_bot(void)
{
    static const char WALLS[] = "block 1800 500 2 h=10\n"
        "block 3200 0 2 h=10\n";
    char text[256];
    sim_t s;
    bot_result_t r;

    snprintf(text, sizeof(text), "start_gamemode cube\n%s", WALLS);
    load(&s, text);
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_NO_PATH);         /* as a cube */
    bot_result_free(&r);
    sim_free(&s);
    snprintf(text, sizeof(text), SWING "%s", WALLS);
    load(&s, text);
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && replays(&s, &r));
    bot_result_free(&r);
    sim_free(&s);
}

void test_swing(void)
{
    test_row();
    test_crossing();
    test_holding_does_nothing();
    test_air_flip();
    test_swing_orbs();
    test_ceiling_bounce();
    test_portals();
    test_swing_bot();
}
