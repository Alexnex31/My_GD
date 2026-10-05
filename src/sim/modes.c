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

int mode_from_name(const char *name)
{
    for (int i = 0; i < MODE_COUNT; i++)
        if (strcmp(MODES[i].name, name) == 0)
            return i;
    return -1;
}
