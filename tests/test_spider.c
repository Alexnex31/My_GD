/*
** ALEXNEX PROJECT, 2026
** tests/test_spider.c
** File description:
** the spider: a press takes it straight to the surface opposite (FEATURES 10.5)
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
#define SPIDER "start_gamemode spider\n"
#define FAR "block 60000 750 2\n"             /* a level long enough  */
#define TOP (GROUND_Y - 900.0 + PLAYER_HALF)   /* resting on the ceiling */

static void load(sim_t *s, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

/* A cube's body and fall, a 900 px corridor, a ceiling that kills. */
static void test_row(void)
{
    const mode_ops_t *m = &MODES[MODE_SPIDER];
    sim_t s;

    CHECK(mode_from_name("spider") == MODE_SPIDER);
    CHECK(m->half == PLAYER_HALF && m->inner_half == PLAYER_INNER_HALF);
    CHECK(m->gravity == MODES[MODE_CUBE].gravity);
    CHECK(m->max_fall == MODES[MODE_CUBE].max_fall);
    CHECK(m->corridor_height == 900.0 && m->head_restitution < 0.0);
    CHECK(m->bot_decision_ticks == 0 && !m->neutral_kills);
    CHECK(m->apply_input == spider_input);
    CHECK(m->update_rotation == robot_rotation);
    load(&s, SPIDER "portal 3000 650 2 spider\n" FAR);
    CHECK(s.lvl.skipped_lines == 0 && s.st.player.mode == MODE_SPIDER);
    CHECK(s.lvl.objects[0].portal_mode == MODE_SPIDER);
    CHECK(s.st.bounds.active && s.st.bounds.bottom == GROUND_Y);
    CHECK(s.st.bounds.top == GROUND_Y - 900.0);
    CHECK(s.st.player.grounded && !s.st.player.teleport);
    sim_free(&s);
}

/*
** A press: on the ceiling at the end of the same tick, flipped and standing,
** and the tick still covers its whole vx. A new press brings it back down.
*/
static void test_across(void)
{
    sim_t s;
    double x;

    load(&s, SPIDER FAR);
    x = s.st.player.pos.x;
    sim_tick(&s, PRESS);
    CHECK(s.st.player.alive && s.st.player.gravity_dir == -1);
    CHECK(s.st.player.pos.y == TOP && s.st.player.grounded);
    CHECK(s.st.player.vy == 0.0 && !s.st.player.teleport);
    CHECK(s.st.player.pos.x == x + s.st.player.vx);
    CHECK(s.st.player.hold == HOLD_USED && s.st.player.can_jump);
    sim_tick(&s, NONE);
    CHECK(s.st.player.pos.y == TOP && s.st.player.grounded);
    sim_tick(&s, PRESS);
    CHECK(s.st.player.gravity_dir == 1);
    CHECK(s.st.player.pos.y == PLAYER_SPAWN_Y && s.st.player.grounded);
    sim_free(&s);
}

/*
** One press, one crossing: held, it stays where it went. A hold carried into
** the attempt is fresh, so it crosses on the first tick, as the ball flips.
*/
static void test_hold(void)
{
    sim_t s;

    load(&s, SPIDER FAR);
    sim_tick(&s, PRESS);
    for (int i = 0; i < 200; i++) {
        sim_tick(&s, HELD);
        CHECK(s.st.player.gravity_dir == -1 && s.st.player.pos.y == TOP);
    }
    sim_free(&s);
    load(&s, SPIDER FAR);
    sim_tick(&s, HELD);
    CHECK(s.st.player.gravity_dir == -1 && s.st.player.pos.y == TOP);
    sim_free(&s);
}

/*
** In the air it can't: the press waits, fresh, and it crosses on the first
** tick it can jump, here the one it lands on.
*/
static void test_from_the_air(void)
{
    sim_t s;
    int t;

    load(&s, SPIDER FAR);
    s.st.player.pos.y = 300.0;
    s.st.player.grounded = false;
    s.st.player.can_jump = false;
    sim_tick(&s, PRESS);
    CHECK(s.st.player.gravity_dir == 1 && s.st.player.hold == HOLD_FRESH);
    for (t = 0; t < 200 && s.st.player.gravity_dir == 1; t++)
        sim_tick(&s, HELD);
    CHECK(s.st.player.gravity_dir == -1 && s.st.player.pos.y == TOP);
    CHECK(t > 20 && s.st.player.alive);
    sim_free(&s);
}

/* The first thing above: a block's underside, a slope's, and back down. */
static void test_what_is_above(void)
{
    sim_t s;

    load(&s, SPIDER "block 300 300 2 w=8 h=2\n" FAR);
    sim_tick(&s, PRESS);
    CHECK(s.st.player.alive && s.st.player.grounded);
    CHECK(s.st.player.pos.y == 400.0 + PLAYER_HALF);
    CHECK(s.st.player.gravity_dir == -1);
    sim_free(&s);
    load(&s, SPIDER "slope 200 300 2 w=8 h=4 rot=180\n" FAR);
    sim_tick(&s, PRESS);
    CHECK(s.st.player.alive && s.st.player.grounded);
    CHECK(s.st.player.pos.y > 300.0 && s.st.player.pos.y < 500.0 + PLAYER_HALF);
    CHECK(s.st.player.support_normal.x != 0.0);   /* it stands on the slope */
    CHECK(s.st.player.support_normal.y > 0.0);    /* a face looking down */
    for (int i = 0; i < 40; i++) {
        sim_tick(&s, NONE);                  /* it follows the slope down */
        CHECK(s.st.player.alive && s.st.player.grounded);
    }
    sim_free(&s);
    load(&s, SPIDER "block 300 300 2 w=8 h=2\nblock 300 600 2 w=8 h=2\n" FAR);
    s.st.player.pos.y = 500.0;               /* between two blocks */
    s.st.player.grounded = false;
    for (int i = 0; i < 100 && !s.st.player.grounded; i++)
        sim_tick(&s, NONE);
    CHECK(s.st.player.pos.y == 600.0 - PLAYER_HALF);
    sim_tick(&s, PRESS);
    CHECK(s.st.player.alive && s.st.player.pos.y == 400.0 + PLAYER_HALF);
    sim_tick(&s, NONE);
    sim_tick(&s, PRESS);
    CHECK(s.st.player.alive && s.st.player.pos.y == 600.0 - PLAYER_HALF);
    sim_free(&s);
}

/*
** A face steeper than 50 degrees is gone into, as a wall is (4.4), and the
** crossing goes on: the tip of a slope over the square's edge, beside the
** inner box, doesn't stop it on its way to the ceiling.
*/
static void test_steep_face(void)
{
    sim_t s;

    load(&s, SPIDER "slope 270 300 2 w=1 h=4 rot=180\n" FAR);
    sim_tick(&s, PRESS);
    CHECK(s.st.player.alive && s.st.player.grounded);
    CHECK(s.st.player.pos.y == TOP);
    CHECK(s.nb_dbg >= 1 && s.dbg[0].kind == DBG_PASS);
    sim_free(&s);
}

/* A spike on the way kills it there, and so does one where it would land. */
static void test_spikes(void)
{
    sim_t s;

    load(&s, SPIDER "spike 300 300 2 rot=180\n" FAR);
    sim_tick(&s, PRESS);
    CHECK(!s.st.player.alive);
    CHECK(s.st.player.pos.y > 400.0 && s.st.player.pos.y < 500.0);
    sim_free(&s);
    load(&s, SPIDER "spike 300 -50 2 rot=180\n" FAR);
    sim_tick(&s, PRESS);
    CHECK(!s.st.player.alive && s.st.player.pos.y > TOP);
    sim_free(&s);
    load(&s, SPIDER "spike 600 -50 2 rot=180\n" FAR);   /* further on: no */
    sim_tick(&s, PRESS);
    CHECK(s.st.player.alive && s.st.player.pos.y == TOP);
    sim_free(&s);
}

/*
** Interactive objects on the way are touched, once, as in any move: a cube
** portal halfway up makes it a cube on its ceiling, still flipped.
*/
static void test_portal_on_the_way(void)
{
    sim_t s;

    load(&s, SPIDER "portal 330 200 2 cube\n" FAR);
    sim_tick(&s, PRESS);
    CHECK(s.st.player.alive && is_spent(&s.st, 0));
    CHECK(s.st.player.mode == MODE_CUBE && s.st.player.gravity_dir == -1);
    CHECK(!s.st.bounds.active);
    sim_free(&s);
}

/* Nothing above but the kill line: it goes there and dies (4.7). */
static void test_nothing_above(void)
{
    sim_t s;

    load(&s, SPIDER FAR);
    s.st.bounds.active = false;              /* a spider out of its corridor */
    sim_tick(&s, PRESS);
    CHECK(!s.st.player.alive);
    CHECK(s.st.player.pos.y - PLAYER_HALF < s.lvl.kill_y);
    CHECK(fabs(s.st.player.pos.y - s.lvl.kill_y) < 1.0);   /* there, not past */
    sim_free(&s);
}

/* The request lives inside one tick: never in the state between two. */
static void test_state(void)
{
    sim_t s;
    sim_snapshot_t snap;
    uint64_t before;

    load(&s, SPIDER FAR);
    sim_snapshot_init(&snap, &s);
    sim_snapshot_save(&snap, &s);
    before = sim_physics_hash(&s);
    for (int i = 0; i < 300; i++) {
        sim_tick(&s, i % 40 == 0 ? PRESS : NONE);
        CHECK(!s.st.player.teleport);
    }
    sim_snapshot_restore(&s, &snap);
    CHECK(sim_physics_hash(&s) == before);
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

/* Ten spikes in a row: no jump clears them, the ceiling does. */
static void test_spider_bot(void)
{
    char text[512] = SPIDER;
    char cube[512] = "";
    sim_t s;
    bot_result_t r;

    for (int i = 0; i < 10; i++)
        snprintf(text + strlen(text), sizeof(text) - strlen(text),
            "spike %d 750 2\n", 1500 + 100 * i);
    snprintf(cube, sizeof(cube), "%s", text + strlen(SPIDER));
    load(&s, cube);
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_NO_PATH);         /* as a cube */
    bot_result_free(&r);
    sim_free(&s);
    load(&s, text);
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && replays(&s, &r));
    bot_result_free(&r);
    sim_free(&s);
}

void test_spider(void)
{
    test_row();
    test_across();
    test_hold();
    test_from_the_air();
    test_what_is_above();
    test_steep_face();
    test_spikes();
    test_portal_on_the_way();
    test_nothing_above();
    test_state();
    test_spider_bot();
}
