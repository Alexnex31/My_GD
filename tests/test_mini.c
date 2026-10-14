/*
** ALEXNEX PROJECT, 2026
** tests/test_mini.c
** File description:
** mini: half the size, GD's mini numbers, size portals (FEATURES 10.4)
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor/ed_history.h"
#include "editor/ed_level.h"
#include "editor/ed_props.h"
#include "sim/bot.h"
#include "sim/internal.h"
#include "sim/level.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

#define NONE ((input_t){false, false})
#define PRESS ((input_t){true, true})
#define HELD ((input_t){true, false})
#define FAR "block 60000 750 2\n"             /* a level long enough  */
#define MINI "start_size mini\nstart_y 825\n"  /* mini, on the ground  */
#define MINI_HALF (PLAYER_HALF * MINI_SCALE)
#define TUNNEL "block 1000 740 2 w=20 h=1\n"   /* 60 px over the ground */

static void load(sim_t *s, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

/* Half of every size; GD's mini numbers, the rest each mode's own. */
static void test_tables(void)
{
    for (int m = 0; m < MODE_COUNT; m++) {
        CHECK(strcmp(MINI_MODES[m].name, MODES[m].name) == 0);
        CHECK(MINI_MODES[m].half == MODES[m].half / 2.0);
        CHECK(MINI_MODES[m].inner_half == MODES[m].inner_half / 2.0);
        CHECK(MINI_MODES[m].corridor_height == MODES[m].corridor_height);
        CHECK(MINI_MODES[m].max_fall == MODES[m].max_fall);
        CHECK(MINI_MODES[m].apply_input == MODES[m].apply_input);
        CHECK(MINI_MODES[m].head_restitution == MODES[m].head_restitution);
        CHECK(MINI_MODES[m].bot_decision_ticks == MODES[m].bot_decision_ticks);
        if (m != MODE_CUBE) {
            CHECK(MINI_MODES[m].gravity == MODES[m].gravity);
        }
        if (m != MODE_CUBE && m != MODE_UFO) {
            CHECK(MINI_MODES[m].jump_v == MODES[m].jump_v);
        }
    }
    CHECK(MINI_MODES[MODE_CUBE].gravity == PER_TICK2(MINI_CUBE_GRAVITY));
    CHECK(MINI_MODES[MODE_CUBE].jump_v == PER_TICK(1.41 * V_UNIT));
    CHECK(MINI_MODES[MODE_UFO].jump_v == PER_TICK(MINI_UFO_JUMP_V));
    CHECK(MINI_MODES[MODE_ROBOT].rise_ticks == 51);
    CHECK(MINI_MODES[MODE_WAVE].slope == 2.0 && MODES[MODE_WAVE].slope == 1.0);
    CHECK(MINI_MODES[MODE_SHIP].thrust == MODES[MODE_SHIP].thrust);
}

/* The line, its box, a bad word, and the line written back. */
static void test_grammar(void)
{
    static const char TEXT[] = "mini 1000 650 2 on\nmini 2000 650 2 off\n"
        "mini 3000 650 2 big\nstart 4000 600 ship up 2 mini\n"
        "start 4100 600 cube mini 2\n" FAR;
    ed_level_t lv;
    char *out;
    sim_t s;

    load(&s, TEXT);
    CHECK(s.lvl.skipped_lines == 2);         /* "big", and mini before the speed */
    CHECK(s.lvl.objects[0].type == OBJ_MINI && s.lvl.objects[0].portal_mini);
    CHECK(s.lvl.objects[1].type == OBJ_MINI && !s.lvl.objects[1].portal_mini);
    CHECK(s.lvl.objects[0].rect.w == 100.0 * PORTAL_BOX_W);
    CHECK(s.lvl.objects[0].rect.h == 100.0 * PORTAL_BOX_H);
    CHECK(OBJ_CATEGORY[OBJ_MINI] == CAT_INTERACTIVE);
    CHECK(s.lvl.nb_starts == 1 && s.lvl.starts[0].mini);
    CHECK(s.lvl.starts[0].speed == 2.0 && s.lvl.starts[0].gravity_dir == -1);
    sim_free(&s);
    ed_level_from_text(&lv, TEXT, strlen(TEXT), "1");
    out = ed_level_text(&lv);
    CHECK(strstr(out, "mini 1000 650 2 on\n") != NULL);
    CHECK(strstr(out, "mini 2000 650 2 off\n") != NULL);
    CHECK(strstr(out, "start 4000 600 ship up 2 mini\n") != NULL);
    free(out);
    ed_level_free(&lv);
}

/* A jump from the ground with the button down `hold` ticks: its height. */
static double height(const char *text, int hold, int *air)
{
    sim_t s;
    double from;
    double apex = 0.0;
    int t;

    load(&s, text);
    from = s.st.player.pos.y;
    for (t = 0; t < 2000; t++) {
        sim_tick(&s, t == 0 ? PRESS : (t < hold ? HELD : NONE));
        if (t > 0 && s.st.player.grounded)
            break;
        apex = fmax(apex, from - s.st.player.pos.y);
    }
    *air = t + 1;
    sim_free(&s);
    return apex;
}

/*
** GD's mini heights, measured: the cube's 1.3583 blocks, the UFO's 1.2, the
** robot's 2.7666 at most (FEATURES 6.9). Full size is untouched.
*/
static void test_heights(void)
{
    int air;

    CHECK(fabs(height(MINI FAR, 1, &air) - 135.83) < 0.05);
    CHECK(air == 91);
    CHECK(fabs(height(MINI "start_gamemode ufo\n" FAR, 1, &air) - 120.0)
        < 0.05);
    CHECK(fabs(height(MINI "start_gamemode robot\n" FAR, 500, &air)
        - 275.6) < 0.05);
    CHECK(fabs(height(FAR, 1, &air) - 213.32) < 0.05 && air == 102);
    CHECK(fabs(height("start_gamemode robot\n" FAR, 500, &air) - 349.2)
        < 0.05);
}

/* The mini wave climbs two px for each one it goes right (6.9). */
static void test_mini_wave(void)
{
    sim_t s;
    double y;

    load(&s, "start_size mini\nstart_gamemode wave\nstart_y 400\n" FAR);
    y = s.st.player.pos.y;
    sim_tick(&s, PRESS);
    CHECK(s.st.player.vy == 2.0 * s.st.player.vx);
    CHECK(fabs(y - s.st.player.pos.y - 2.0 * s.st.player.vx) < 1e-9);
    sim_tick(&s, NONE);
    CHECK(s.st.player.vy == -2.0 * s.st.player.vx);
    sim_free(&s);
}

/* Mini pads and orbs launch with GD's mini speeds; blue is the same. */
static void test_launches(void)
{
    sim_t s;
    object_t pad = {.type = OBJ_PAD, .launch = LAUNCH_YELLOW};
    object_t orb = {.type = OBJ_ORB, .launch = LAUNCH_RED};

    CHECK(launch_speed_for(&pad, true) == 2.13);
    CHECK(launch_speed_for(&pad, false) == 2.77);
    CHECK(launch_speed_for(&orb, true) == 2.05);
    pad.launch = LAUNCH_BLUE;
    CHECK(launch_speed_for(&pad, true) == launch_speed_for(&pad, false));
    orb.launch = LAUNCH_GREEN;
    CHECK(launch_speed_for(&orb, true) == 1.91);
    load(&s, MINI "pad 700 750 2 yellow\n" FAR);
    while (s.st.player.vy <= 0.0 && s.st.tick < 200)
        sim_tick(&s, NONE);
    CHECK(fabs(s.st.player.vy - PER_TICK(2.13 * V_UNIT))
        <= MINI_MODES[MODE_CUBE].gravity);   /* a mini pad's, not 2.77 */
    sim_free(&s);
}

/*
** A size portal: the same center, nothing else changed, spent once. Mini on
** the ground, the player is in the air and falls to it; full size again, it
** is put back on the ground it would cross.
*/
static void test_portals(void)
{
    sim_t s;
    double vx;

    load(&s, "mini 700 650 2 on\nmini 1500 650 2 off\n" FAR);
    vx = s.st.player.vx;
    while (!s.st.player.mini && s.st.tick < 200)
        sim_tick(&s, NONE);
    CHECK(s.st.player.mini && s.st.player.pos.y == PLAYER_SPAWN_Y);
    CHECK(is_spent(&s.st, 0) && s.st.player.vx == vx);
    CHECK(s.st.player.mode == MODE_CUBE && s.st.player.gravity_dir == 1);
    for (int i = 0; i < 40; i++)
        sim_tick(&s, NONE);
    CHECK(s.st.player.grounded);
    CHECK(s.st.player.pos.y == GROUND_Y - MINI_HALF);
    while (s.st.player.mini && s.st.tick < 400)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.mini && s.st.player.alive);
    CHECK(s.st.player.pos.y == PLAYER_SPAWN_Y);
    sim_tick(&s, NONE);
    CHECK(s.st.player.grounded && s.st.player.alive);
    sim_free(&s);
}

/* A robot made mini while it rises keeps rising, no longer than mini may. */
static void test_robot_rise(void)
{
    sim_t s;

    load(&s, "start_gamemode robot\nmini 400 650 2 on\n" FAR);
    sim_tick(&s, PRESS);                     /* the portal is touched at once */
    CHECK(s.st.player.mini && is_spent(&s.st, 0));
    CHECK(s.st.player.boost_ticks == MINI_ROBOT_TICKS);
    sim_tick(&s, HELD);
    CHECK(s.st.player.vy == MINI_MODES[MODE_ROBOT].jump_v);
    sim_free(&s);
}

/* Growing back against the corridor's ceiling: pushed off it, not through. */
static void test_grow_in_a_corridor(void)
{
    sim_t s;
    double top;

    load(&s, "start_size mini\nstart_gamemode ship\nstart_gravity flipped\n"
        "mini 1500 -100 2 off\n" FAR);
    top = s.st.bounds.top;
    while (s.st.player.mini && s.st.tick < 400)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.mini && s.st.player.alive);
    CHECK(s.st.player.pos.y == top + PLAYER_HALF);
    sim_free(&s);
}

/*
** A tunnel 60 px high: a mini cube runs through it, a full one can't, and
** one that grows back inside it dies there.
*/
static void test_tunnel(void)
{
    char text[256];
    sim_t s;

    snprintf(text, sizeof(text), "%s", "mini 600 650 2 on\n" TUNNEL FAR);
    load(&s, text);
    while (s.st.player.alive && s.st.player.pos.x < 2200.0)
        sim_tick(&s, NONE);
    CHECK(s.st.player.alive);
    sim_free(&s);
    load(&s, TUNNEL FAR);
    while (s.st.player.alive && s.st.player.pos.x < 2200.0)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.alive && s.st.player.pos.x < 1000.0);
    sim_free(&s);
    snprintf(text, sizeof(text), "%s", "mini 600 650 2 on\n"
        "mini 1400 650 2 off\n" TUNNEL FAR);
    load(&s, text);
    while (s.st.player.alive && s.st.player.pos.x < 2200.0)
        sim_tick(&s, NONE);
    CHECK(!s.st.player.alive);
    CHECK(s.st.player.pos.x > 1350.0 && s.st.player.pos.x < 1500.0);
    sim_free(&s);
}

/* The size is run state: from the start's line, hashed, saved, restored. */
static void test_state(void)
{
    sim_t s;
    sim_snapshot_t snap;
    uint64_t before;

    load(&s, "start_size mini\nstart 3000 825 ship\n" FAR);
    CHECK(s.st.player.mini);
    sim_reset_at(&s, &s.lvl.starts[0]);
    CHECK(!s.st.player.mini && s.st.player.mode == MODE_SHIP);
    sim_reset(&s);
    CHECK(s.st.player.mini);
    sim_snapshot_init(&snap, &s);
    sim_snapshot_save(&snap, &s);
    before = sim_physics_hash(&s);
    s.st.player.mini = false;
    CHECK(sim_physics_hash(&s) != before);
    sim_snapshot_restore(&s, &snap);
    CHECK(sim_physics_hash(&s) == before && s.st.player.mini);
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
** A spike in a tunnel 200 px high, under a wall nothing climbs: a cube's
** jump hits the roof, a mini cube's clears it.
*/
static void test_mini_bot(void)
{
    static const char LEVEL[] = "block 1000 -500 2 w=20 h=23\n"
        "spike 1500 800 1\n";
    char text[256];
    sim_t s;
    bot_result_t r;

    load(&s, LEVEL);
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_NO_PATH);
    bot_result_free(&r);
    sim_free(&s);
    snprintf(text, sizeof(text), "mini 600 650 2 on\n%s", LEVEL);
    load(&s, text);
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && replays(&s, &r));
    bot_result_free(&r);
    sim_free(&s);
}

/* The editor: on and off as the Kind, a start position's Size, undone. */
static void test_mini_editor(void)
{
    static const char TEXT[] = "mini 1000 650 2 on\nstart 2000 600 cube\n";
    ed_level_t lv;
    ed_history_t h = {0};
    char text[64];

    ed_level_from_text(&lv, TEXT, strlen(TEXT), "1");
    CHECK(lv.count == 2);
    lv.objects[0].selected = true;
    CHECK(ed_prop_applies(&lv, ED_PROP_KIND));
    CHECK(!ed_prop_applies(&lv, ED_PROP_SIZE));
    ed_prop_text(&lv, ED_PROP_KIND, text, sizeof(text));
    CHECK(strcmp(text, "on") == 0);
    CHECK(ed_prop_step(&lv, &h, ED_PROP_KIND, 1));
    CHECK(!lv.objects[0].obj.portal_mini);
    lv.objects[0].selected = false;
    lv.objects[1].selected = true;
    CHECK(lv.objects[1].is_start && ed_prop_applies(&lv, ED_PROP_SIZE));
    ed_prop_text(&lv, ED_PROP_SIZE, text, sizeof(text));
    CHECK(strcmp(text, "normal") == 0);
    CHECK(ed_prop_step(&lv, &h, ED_PROP_SIZE, 1));
    CHECK(lv.objects[1].start.mini);
    CHECK(strcmp(ed_prop_name(ED_PROP_SIZE), "Size") == 0);
    ed_undo(&lv, &h);
    CHECK(!ed_find(&lv, lv.objects[1].id)->start.mini);
    ed_undo(&lv, &h);
    CHECK(lv.objects[0].obj.portal_mini);
    ed_history_free(&h);
    ed_level_free(&lv);
}

void test_mini(void)
{
    test_tables();
    test_grammar();
    test_heights();
    test_mini_wave();
    test_launches();
    test_portals();
    test_robot_rise();
    test_grow_in_a_corridor();
    test_tunnel();
    test_state();
    test_mini_bot();
    test_mini_editor();
}
