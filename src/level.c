/*
** ALEXNEX PROJECT, 2026
** level
** File description:
** functions to create and manage a level
*/

#include "mygd.h"

void free_objects(object_list_t *obj_l)
{
    if (obj_l != NULL)
        free(obj_l);
}

void free_level(level_t *level)
{
    free_objects(level->objects);
    free(level);
}

void print_level(gd_t *gd, level_t *level)
{
    return;
}

level_t *start_level(gd_t *gd)
{
    level_t *level = malloc(sizeof(level_t));

    level->objects = malloc(sizeof(object_list_t));
    level->best = 0.0f;
    level->percent = 0.0f;
    level->attempts = 1;
    level->lvl = 1;
    load_level_data("levels/level1", level, gd);
    return level;
}
