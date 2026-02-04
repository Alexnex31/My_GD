/*
** ALEXNEX PROJECT, 2026
** physics
** File description:
** functions to apply physics to the player
*/

#include "mygd.h"

void move_objects(level_t *level, object_list_t *obj)
{
    int i = 0;

    level->shift += 10 * level->speed;
    if (obj->blocks != NULL) {
        while (obj->blocks[i] != NULL) {
            obj->blocks[i]->pos.x -= 10 * level->speed;
            i += 1;
        }
    }
    i = 0;
    if (obj->spikes != NULL) {
        while (obj->spikes[i] != NULL) {
            obj->spikes[i]->pos.x -= 10 * level->speed;
            i += 1;
        }
    }
}

void gravity_player(level_t *level, gd_t *gd)
{
    level->player->allow_jump = 'n';
    level->player->pos.y -= level->player->vy;
    level->player->vy -= 1;
    if (level->player->pos.y > 800) {
        level->player->vy = 0;
        level->player->pos.y = 800;
    }
    if (level->player->pos.y >= 800)
        level->player->allow_jump = 'y';
}

void apply_physics(gd_t *gd, level_t *level, object_list_t *obj)
{
    move_objects(level, obj);
    gravity_player(level, gd);
}

void move_objects_back(level_t *level, object_list_t *obj)
{
    int i = 0;

    if (obj->blocks != NULL) {
        while (obj->blocks[i] != NULL) {
            obj->blocks[i]->pos.x += level->shift;
            i += 1;
        }
    }
    i = 0;
    if (obj->spikes != NULL) {
        while (obj->spikes[i] != NULL) {
            obj->spikes[i]->pos.x += level->shift;
            i += 1;
        }
    }
    level->shift = 0;
}

void player_dead(player_t *player, level_t *level, object_list_t *obj)
{
    printf("dead\n");
    level->attempts += 1;
    player->state = 'd';
    move_objects_back(level, level->objects);
    printf("attempts %d\n", level->attempts);
}

void check_spike(gd_t *gd, level_t *level, object_list_t *obj, int i)
{
    sfVector2f p1 = level->player->pos;
    sfVector2f p2 = obj->spikes[i]->pos;

    p2.x += 5;
    p2.y += 5;
    if (p2.x >= p1.x && p2.x <= p1.x + 20 * level->player->size) {
        if (p2.y >= p1.y && p2.y <= p1.y + 20 * level->player->size) {
            player_dead(level->player, level, level->objects);
        }
    }
    if (p2.x >= p1.x && p2.x <= p1.x + 20 * level->player->size) {
        if (p2.y + obj->spikes[i]->size * 10 * 2 >= p1.y && p2.y + obj->spikes[i]->size * 10 * 2 <= p1.y + 20 * level->player->size) {
            player_dead(level->player, level, level->objects);
        }
    }
    if (p2.x + obj->spikes[i]->size * 10 >= p1.x && p2.x + obj->spikes[i]->size * 10 <= p1.x + 20 * level->player->size) {
        if (p2.y >= p1.y && p2.y <= p1.y + 20 * level->player->size) {
            player_dead(level->player, level, level->objects);
        }
    }
    if (p2.x + obj->spikes[i]->size * 10 >= p1.x && p2.x + obj->spikes[i]->size * 10 <= p1.x + 20 * level->player->size) {
        if (p2.y + obj->spikes[i]->size * 10 * 2 >= p1.y && p2.y + obj->spikes[i]->size * 10 * 2 <= p1.y + 20 * level->player->size) {
            player_dead(level->player, level, level->objects);
        }
    }
}

void check_on_block(gd_t *gd, level_t *level, object_list_t *obj, int i)
{
    level->player->allow_jump = 'y';
    return;
}

void check_collisions(gd_t *gd, level_t *level, object_list_t *obj)
{
    int i = 0;

    if (obj->blocks != NULL) {
        while (obj->blocks[i] != NULL) {
            if (obj->blocks[i]->pos.x < 900 && obj->blocks[i]->pos.x > 100)
                check_on_block(gd, level, obj, i);
            i += 1;
        }
    }
    i = 0;
    if (obj->spikes != NULL) {
        while (obj->spikes[i] != NULL) {
            if (obj->spikes[i]->pos.x < 900 && obj->spikes[i]->pos.x > 100)
                check_spike(gd, level, obj, i);
            i += 1;
        }
    }
}
