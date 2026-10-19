/*
** ALEXNEX PROJECT, 2026
** sim/modes.c
** File description:
** the tables everything reads: object categories, gamemodes (FEATURES 6.1)
*/

#include <math.h>
#include <string.h>
#include "sim/internal.h"
#include "sim/modes.h"

const obj_category_t OBJ_CATEGORY[OBJ_TYPE_COUNT] = {
    [OBJ_BLOCK] = CAT_NEUTRAL,
    [OBJ_SLOPE] = CAT_NEUTRAL,
    [OBJ_SPIKE] = CAT_HARM,
    [OBJ_PORTAL] = CAT_INTERACTIVE,
    [OBJ_GRAVITY] = CAT_INTERACTIVE,
    [OBJ_PAD] = CAT_INTERACTIVE,
    [OBJ_ORB] = CAT_INTERACTIVE,
    [OBJ_SAW] = CAT_HARM,
    [OBJ_SPEED] = CAT_INTERACTIVE,
    [OBJ_MINI] = CAT_INTERACTIVE,
};

/* Every type so far has a hitbox the player meets: a decoration, which is
** only a picture, will be B1 (FEATURES 11.15). */
z_layer_t obj_default_z_layer(obj_type_t type)
{
    (void)type;
    return Z_T1;
}

int object_draw_cmp(const object_t *a, long placed_a, const object_t *b,
    long placed_b)
{
    if (a->z_layer != b->z_layer)
        return a->z_layer < b->z_layer ? -1 : 1;
    if (a->z_order != b->z_order)
        return a->z_order < b->z_order ? -1 : 1;
    return (placed_a > placed_b) - (placed_a < placed_b);
}

bool object_same(const object_t *a, const object_t *b)
{
    return a->type == b->type && a->rect.x == b->rect.x
        && a->rect.y == b->rect.y && a->rect.w == b->rect.w
        && a->rect.h == b->rect.h && a->rotation == b->rotation
        && a->portal_mode == b->portal_mode
        && a->portal_gravity == b->portal_gravity && a->launch == b->launch
        && a->speed == b->speed && a->portal_mini == b->portal_mini
        && a->z_layer == b->z_layer && a->z_order == b->z_order;
}

const char *z_layer_name(z_layer_t z)
{
    static const char *const NAMES[] = {"b5", "b4", "b3", "b2", "b1", "t1",
        "t2", "t3", "t4"};

    return NAMES[z - Z_LAYER_FIRST];
}

/*
** GD's own numbers (FEATURES 10.1, 10.2, 6.9). Blue flips and throws the
** player at its new floor; green flips and makes it jump there, a yellow orb
** in the other gravity; black slams it down. Blue, green and black are the
** same for a mini player.
*/
const launch_ops_t LAUNCHES[LAUNCH_KIND_COUNT] = {
    [LAUNCH_YELLOW] = {.name = "yellow", .pad_v = 2.77, .orb_v = 1.91,
        .mini_pad_v = 2.13, .mini_orb_v = 1.43},
    [LAUNCH_PINK] = {.name = "pink", .pad_v = 1.79, .orb_v = 1.37,
        .mini_pad_v = 1.32, .mini_orb_v = 0.94},
    [LAUNCH_RED] = {.name = "red", .pad_v = 3.65, .orb_v = 2.68,
        .mini_pad_v = 2.71, .mini_orb_v = 2.05},
    [LAUNCH_BLUE] = {.name = "blue", .pad_v = -1.37, .orb_v = -1.37,
        .mini_pad_v = -1.37, .mini_orb_v = -1.37, .flips = true},
    [LAUNCH_GREEN] = {.name = "green", .orb_v = 1.91, .mini_orb_v = 1.91,
        .flips = true},
    [LAUNCH_BLACK] = {.name = "black", .orb_v = -2.6, .mini_orb_v = -2.6},
};

/*
** What a mode is at either size. Each row below gives a mode's shared part,
** then its size, then only the numbers that size changes: mini (FEATURES
** 10.4) can't drift from its mode by a field forgotten in one table.
*/
#define FULL_SIZE .half = PLAYER_HALF, .inner_half = PLAYER_INNER_HALF
#define MINI_SIZE .half = PLAYER_HALF * MINI_SCALE, \
    .inner_half = PLAYER_INNER_HALF * MINI_SCALE

#define CUBE .name = "cube", .max_fall = PER_TICK(CUBE_MAX_FALL), \
    .head_restitution = -1.0, .apply_input = cube_input, \
    .apply_forces = player_apply_gravity, .update_rotation = cube_rotation
#define SHIP .name = "ship", .gravity = PER_TICK2(SHIP_GRAVITY), \
    .max_fall = PER_TICK(SHIP_MAX_VY), \
    .head_restitution = BOUNCE_RESTITUTION_SHIP, \
    .thrust = PER_TICK2(SHIP_THRUST), .corridor_height = 1000.0, \
    .bot_decision_ticks = 12, .apply_input = ship_input, \
    .apply_forces = player_apply_gravity, .update_rotation = ship_rotation
#define UFO .name = "ufo", .gravity = PER_TICK2(UFO_GRAVITY), \
    .max_fall = PER_TICK(UFO_MAX_FALL), \
    .head_restitution = BOUNCE_RESTITUTION_SHIP, .corridor_height = 1000.0, \
    .bot_decision_ticks = 12, .apply_input = ufo_input, \
    .apply_forces = player_apply_gravity, .update_rotation = ufo_rotation
#define WAVE .name = "wave", .head_restitution = -1.0, \
    .corridor_height = 1000.0, .neutral_kills = true, \
    .keep_vy_on_surface = true, .orbs_flip_only = true, \
    .bot_decision_ticks = 12, .apply_input = wave_input, \
    .apply_forces = wave_forces, .update_rotation = wave_rotation
#define BALL .name = "ball", .gravity = PER_TICK2(BALL_GRAVITY), \
    .max_fall = PER_TICK(BALL_MAX_FALL), \
    .head_restitution = BOUNCE_RESTITUTION_SHIP, \
    .jump_v = PER_TICK(BALL_FLIP_V), .corridor_height = 800.0, \
    .bot_taps = true, .apply_input = ball_input, .apply_forces = player_apply_gravity, \
    .update_rotation = ball_rotation
#define ROBOT .name = "robot", .gravity = PER_TICK2(CUBE_GRAVITY), \
    .max_fall = PER_TICK(CUBE_MAX_FALL), .head_restitution = -1.0, \
    .jump_v = PER_TICK(ROBOT_RISE_V), .apply_input = robot_input, \
    .apply_forces = robot_forces, .update_rotation = robot_rotation
#define SWING .name = "swing", .gravity = PER_TICK2(SWING_GRAVITY), \
    .max_fall = PER_TICK(SWING_MAX_VY), \
    .head_restitution = BOUNCE_RESTITUTION_SHIP, .corridor_height = 1000.0, \
    .bot_decision_ticks = 12, .apply_input = swing_input, \
    .apply_forces = player_apply_gravity, .update_rotation = ship_rotation
#define SPIDER .name = "spider", .gravity = PER_TICK2(CUBE_GRAVITY), \
    .max_fall = PER_TICK(CUBE_MAX_FALL), .head_restitution = -1.0, \
    .corridor_height = 900.0, .bot_taps = true, .apply_input = spider_input, \
    .apply_forces = player_apply_gravity, .update_rotation = robot_rotation

const mode_ops_t MODES[MODE_COUNT] = {
    [MODE_CUBE] = {CUBE, FULL_SIZE, .gravity = PER_TICK2(CUBE_GRAVITY),
        .jump_v = PER_TICK(CUBE_JUMP_V)},
    [MODE_SHIP] = {SHIP, FULL_SIZE},
    [MODE_UFO] = {UFO, FULL_SIZE, .jump_v = PER_TICK(UFO_JUMP_V)},
    [MODE_WAVE] = {WAVE, .half = WAVE_HALF, .inner_half = WAVE_HALF,
        .slope = 1.0},
    [MODE_BALL] = {BALL, FULL_SIZE},
    [MODE_ROBOT] = {ROBOT, FULL_SIZE, .rise_ticks = ROBOT_BOOST_TICKS},
    [MODE_SWING] = {SWING, FULL_SIZE},
    [MODE_SPIDER] = {SPIDER, FULL_SIZE},
};

/*
** Mini (FEATURES 10.4): half the size, and GD's mini numbers where it has
** any: the cube's jump and gravity, the UFO's hop, the robot's longest
** rise, the wave's 2:1 climb. The rest is each mode's own. Corridors don't
** change: a mini ship flies in the same 1000 px.
*/
const mode_ops_t MINI_MODES[MODE_COUNT] = {
    [MODE_CUBE] = {CUBE, MINI_SIZE, .gravity = PER_TICK2(MINI_CUBE_GRAVITY),
        .jump_v = PER_TICK(MINI_CUBE_JUMP_V)},
    [MODE_SHIP] = {SHIP, MINI_SIZE},
    [MODE_UFO] = {UFO, MINI_SIZE, .jump_v = PER_TICK(MINI_UFO_JUMP_V)},
    [MODE_WAVE] = {WAVE, .half = WAVE_HALF * MINI_SCALE,
        .inner_half = WAVE_HALF * MINI_SCALE, .slope = MINI_WAVE_SLOPE},
    [MODE_BALL] = {BALL, MINI_SIZE},
    [MODE_ROBOT] = {ROBOT, MINI_SIZE, .rise_ticks = MINI_ROBOT_TICKS},
    [MODE_SWING] = {SWING, MINI_SIZE},
    [MODE_SPIDER] = {SPIDER, MINI_SIZE},
};

const mode_ops_t *player_mode(const player_t *p)
{
    return p->mini ? &MINI_MODES[p->mode] : &MODES[p->mode];
}

/*
** The square that dies on a neutral object: the small inner box, or the whole
** rigid square for a mode nothing may touch (the wave, FEATURES 6.4).
*/
double mode_neutral_kill_half(const mode_ops_t *m)
{
    return m->neutral_kills ? m->half : m->inner_half;
}

double modes_tallest_corridor(void)
{
    double tallest = 0.0;

    for (int i = 0; i < MODE_COUNT; i++)
        if (MODES[i].corridor_height > tallest)
            tallest = MODES[i].corridor_height;
    return tallest;
}

/* Over every mode and size: what the broadphase keeps behind the player (4.1). */
double modes_widest_half(void)
{
    double widest = 0.0;

    for (int i = 0; i < MODE_COUNT; i++)
        widest = fmax(widest, fmax(MODES[i].half, MINI_MODES[i].half));
    return widest;
}

int launch_from_name(const char *name)
{
    for (int i = 0; i < LAUNCH_KIND_COUNT; i++)
        if (strcmp(LAUNCHES[i].name, name) == 0)
            return i;
    return -1;
}

/* What it does to a player of that size: GD has mini pads and orbs (6.9). */
double launch_speed_for(const object_t *o, bool mini)
{
    const launch_ops_t *l = NULL;

    if (o->type != OBJ_PAD && o->type != OBJ_ORB)
        return 0.0;
    l = &LAUNCHES[o->launch];
    if (o->type == OBJ_PAD)
        return mini ? l->mini_pad_v : l->pad_v;
    return mini ? l->mini_orb_v : l->orb_v;
}

double launch_speed(const object_t *o)
{
    return launch_speed_for(o, false);
}

int mode_from_name(const char *name)
{
    for (int i = 0; i < MODE_COUNT; i++)
        if (strcmp(MODES[i].name, name) == 0)
            return i;
    return -1;
}
