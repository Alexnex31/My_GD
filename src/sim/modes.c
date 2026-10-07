/*
** ALEXNEX PROJECT, 2026
** sim/modes.c
** File description:
** the tables everything reads: object categories, gamemodes (FEATURES 6.1)
*/

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
};

/*
** GD's own numbers (FEATURES 10.1, 10.2). Blue flips and throws the player
** at its new floor; green flips and makes it jump there, a yellow orb in the
** other gravity; black slams it down.
*/
const launch_ops_t LAUNCHES[LAUNCH_KIND_COUNT] = {
    [LAUNCH_YELLOW] = {.name = "yellow", .pad_v = 2.77, .orb_v = 1.91},
    [LAUNCH_PINK] = {.name = "pink", .pad_v = 1.79, .orb_v = 1.37},
    [LAUNCH_RED] = {.name = "red", .pad_v = 3.65, .orb_v = 2.68},
    [LAUNCH_BLUE] = {.name = "blue", .pad_v = -1.37, .orb_v = -1.37,
        .flips = true},
    [LAUNCH_GREEN] = {.name = "green", .orb_v = 1.91, .flips = true},
    [LAUNCH_BLACK] = {.name = "black", .orb_v = -2.6},
};

const mode_ops_t MODES[MODE_COUNT] = {
    [MODE_CUBE] = {.name = "cube", .half = PLAYER_HALF, .inner_half = PLAYER_INNER_HALF,
        .gravity = PER_TICK2(CUBE_GRAVITY),
        .max_fall = PER_TICK(CUBE_MAX_FALL), .head_restitution = -1.0,
        .apply_input = cube_input, .apply_forces = player_apply_gravity,
        .update_rotation = cube_rotation},
    [MODE_SHIP] = {.name = "ship", .half = PLAYER_HALF, .inner_half = PLAYER_INNER_HALF,
        .gravity = PER_TICK2(SHIP_GRAVITY),
        .max_fall = PER_TICK(SHIP_MAX_VY), .head_restitution = BOUNCE_RESTITUTION_SHIP,
        .corridor_height = 1000.0, .bot_decision_ticks = 12,
        .apply_input = ship_input, .apply_forces = player_apply_gravity,
        .update_rotation = ship_rotation},
    [MODE_UFO] = {.name = "ufo", .half = PLAYER_HALF, .inner_half = PLAYER_INNER_HALF,
        .gravity = PER_TICK2(UFO_GRAVITY),
        .max_fall = PER_TICK(UFO_MAX_FALL), .head_restitution = BOUNCE_RESTITUTION_SHIP,
        .corridor_height = 1000.0, .bot_decision_ticks = 12,
        .apply_input = ufo_input, .apply_forces = player_apply_gravity,
        .update_rotation = ufo_rotation},
    [MODE_WAVE] = {.name = "wave", .half = WAVE_HALF, .inner_half = WAVE_HALF,
        .head_restitution = -1.0,
        .corridor_height = 1000.0, .neutral_kills = true,
        .keep_vy_on_surface = true, .bot_decision_ticks = 12,
        .apply_input = wave_input, .apply_forces = wave_forces,
        .update_rotation = wave_rotation},
    [MODE_BALL] = {.name = "ball", .half = PLAYER_HALF, .inner_half = PLAYER_INNER_HALF,
        .gravity = PER_TICK2(BALL_GRAVITY),
        .max_fall = PER_TICK(BALL_MAX_FALL), .head_restitution = BOUNCE_RESTITUTION_SHIP,
        .corridor_height = 800.0,
        .apply_input = ball_input, .apply_forces = player_apply_gravity,
        .update_rotation = ball_rotation},
};

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

int launch_from_name(const char *name)
{
    for (int i = 0; i < LAUNCH_KIND_COUNT; i++)
        if (strcmp(LAUNCHES[i].name, name) == 0)
            return i;
    return -1;
}

double launch_speed(const object_t *o)
{
    if (o->type == OBJ_PAD)
        return LAUNCHES[o->launch].pad_v;
    return o->type == OBJ_ORB ? LAUNCHES[o->launch].orb_v : 0.0;
}

int mode_from_name(const char *name)
{
    for (int i = 0; i < MODE_COUNT; i++)
        if (strcmp(MODES[i].name, name) == 0)
            return i;
    return -1;
}
