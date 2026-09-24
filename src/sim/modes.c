/*
** ALEXNEX PROJECT, 2026
** sim/modes.c
** File description:
** header file for my_gd project
*/

#include <string.h>
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
        .max_fall = PER_TICK(CUBE_MAX_FALL), .head_restitution = -1.0},
    [MODE_SHIP] = {.name = "ship", .half = PLAYER_HALF, .inner_half = PLAYER_INNER_HALF,
        .gravity = PER_TICK2(SHIP_GRAVITY),
        .max_fall = PER_TICK(SHIP_MAX_VY), .head_restitution = BOUNCE_RESTITUTION_SHIP,
        .corridor_height = 1000.0},
};

int mode_from_name(const char *name)
{
    for (int i = 0; i < MODE_COUNT; i++)
        if (strcmp(MODES[i].name, name) == 0)
            return i;
    return -1;
}
