/*
** ALEXNEX PROJECT, 2026
** level
** File description:
** functions to create and manage a level
*/

#include "mygd.h"

void free_block(block_t *block)
{
    sfSprite_destroy(block->sprite);
    free(block);
}

void free_spike(spike_t *spike)
{
    sfSprite_destroy(spike->sprite);
    free(spike);
}

void free_block_list(block_t **list)
{
    int i = 0;

    if (list == NULL)
        return;
    while (list[i] != NULL) {
        free_block(list[i]);
        i += 1;
    }
    free(list);
}

void free_spike_list(spike_t **list)
{
    int i = 0;

    if (list == NULL)
        return;
    while (list[i] != NULL) {
        free_spike(list[i]);
        i += 1;
    }
    free(list);
}

void free_objects(object_list_t *obj_l)
{
    if (obj_l != NULL) {
        free_block(obj_l->ground);
        free_spike_list(obj_l->spikes);
        free_block_list(obj_l->blocks);
        free(obj_l);
    }
}

void free_level(level_t *level)
{
    if (level == NULL)
        return;
    free_player(level->player);
    if (level->background != NULL)
        sfSprite_destroy(level->background);
    free_objects(level->objects);
    free(level);
}

void print_objects(gd_t *gd, level_t *level, object_list_t *obj)
{
    int i = 0;

    if (obj->ground != NULL)
        sfRenderWindow_drawSprite(gd->w, obj->ground->sprite, NULL);
    if (obj->blocks != NULL) {
        while (obj->blocks[i] != NULL) {
            if (obj->blocks[i]->pos.x < 2000 && obj->blocks[i]->pos.x > -100) {
                sfSprite_setPosition(obj->blocks[i]->sprite, obj->blocks[i]->pos);
                sfRenderWindow_drawSprite(gd->w, obj->blocks[i]->sprite, NULL);
            }
            i += 1;
        }
    }
    i = 0;
    if (obj->spikes != NULL) {
        while (obj->spikes[i] != NULL) {
            if (obj->spikes[i]->pos.x < 2000 && obj->spikes[i]->pos.x > -100) {
                sfSprite_setPosition(obj->spikes[i]->sprite, obj->spikes[i]->pos);
                sfRenderWindow_drawSprite(gd->w, obj->spikes[i]->sprite, NULL);
            }
            i += 1;
        }
    }
}

void print_player(gd_t *gd, level_t *level, object_list_t *obj)
{
    if (level->player != NULL) {
        if (level->player->sprite != NULL) {
            sfSprite_setPosition(level->player->sprite, level->player->pos);
            sfRenderWindow_drawSprite(gd->w, level->player->sprite, NULL);
        }
    }
}

void print_level(gd_t *gd, level_t *level)
{
    if (level->background != NULL)
        sfRenderWindow_drawSprite(gd->w, level->background, NULL);
    print_objects(gd, level, level->objects);
    print_player(gd, level, level->objects);
    apply_physics(gd, level, level->objects);
    check_collisions(gd, level, level->objects);
}

level_t *start_level(gd_t *gd)
{
    level_t *level = malloc(sizeof(level_t));
    char levelpath[256];

    level->background = sfSprite_create();
    if (level->background == NULL) {
        printf("Error: Failed to create background sprite\n");
        level->objects = malloc(sizeof(object_list_t));
        level->best = 0.0f;
        level->percent = 0.0f;
        level->attempts = 1;
        level->lvl = gd->selected_level;
        return level;
    }
    
    if (gd->res->level_background == NULL) {
        printf("Error: level_background texture is NULL\n");
    } else {
        sfSprite_setTexture(level->background, gd->res->level_background, sfTrue);
    }
    level->player = create_player(gd);
    level->objects = malloc(sizeof(object_list_t));
    level->speed = 1;
    level->shift = 0;
    level->best = 0.0f;
    level->percent = 0.0f;
    level->attempts = 0;
    level->lvl = gd->selected_level;
    snprintf(levelpath, 256, "levels/level%d", gd->selected_level);
    load_level_data(levelpath, level, gd);
    level->attempts += 1;
    return level;
}
