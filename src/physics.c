/*
** ALEXNEX PROJECT, 2026
** physics
** File description:
** functions to apply physics to the player
*/

#include "mygd.h"

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
    i = 0;
    if (obj->portals != NULL) {
        while (obj->portals[i] != NULL) {
            obj->portals[i]->pos.x += level->shift;
            i += 1;
        }
    }


    if (level->yshift != 0) {
        if (obj->ground != NULL) {
            obj->sprite_ground_pos.y -= level->yshift;
            obj->ground->pos.y -= level->yshift;
        }
        i = 0;
        if (obj->blocks != NULL) {
            while (obj->blocks[i] != NULL) {
                obj->blocks[i]->pos.y -= level->yshift;
                i += 1;
            }
        }
        i = 0;
        if (obj->spikes != NULL) {
            while (obj->spikes[i] != NULL) {
                obj->spikes[i]->pos.y -= level->yshift;
                i += 1;
            }
        }
        i = 0;
        if (obj->portals != NULL) {
            while (obj->portals[i] != NULL) {
                obj->portals[i]->pos.y -= level->yshift;
                i += 1;
            }
        }
    }
    level->shift = 0;
    level->yshift = 0;
}

void load_new_player_texture(player_t *player, char gamemode, gd_t *gd)
{
    if (gamemode == 'c') {
        player->allow_jump = 'n';
        sfSprite_setTexture(player->sprite, gd->res->player_icon, sfTrue);
        return;
    }
    if (gamemode == 'p') {
        sfSprite_setTexture(player->sprite, gd->res->ship_icon, sfTrue);
        return;
    }
}

void player_dead(player_t *player, level_t *level, object_list_t *obj, gd_t *gd)
{
    if (level->percent > level->best) {
        level->best = level->percent;
    }
    level->attempts += 1;
    level->curr_attempts += 1;
    player->state = 'd';
    if (level->objects->portal_blocks != NULL) {
        free_block_list(level->objects->portal_blocks);
        level->objects->portal_blocks = NULL;
    }
    move_objects_back(level, level->objects);
    player->pos = (sfVector2f){350, 790};
    player->vy = 0;
    player->state = 'a';
    player->allow_jump = 'n';
    reset_attempt_display(level);
    load_new_player_texture(player, 'c', gd);
}

void move_objects(level_t *level, object_list_t *obj)
{
    int i = 0;

    level->shift += 12.5f * level->speed;
    if (obj->blocks != NULL) {
        while (obj->blocks[i] != NULL) {
            obj->blocks[i]->pos.x -= 12.5f * level->speed;
            i += 1;
        }
    }
    i = 0;
    if (obj->spikes != NULL) {
        while (obj->spikes[i] != NULL) {
            obj->spikes[i]->pos.x -= 12.5f * level->speed;
            i += 1;
        }
    }
    i = 0;
    if (obj->portals != NULL) {
        while (obj->portals[i] != NULL) {
            obj->portals[i]->pos.x -= 12.5f * level->speed;
            i += 1;
        }
    }

    if (level->objects->portal_blocks != NULL) {
        level->objects->portal_blocks[0]->pos.x -= 12.5f * level->speed;
        level->objects->portal_blocks[1]->pos.x -= 12.5f * level->speed;
    }

    if (level->player->gamemode == 'c') {
        if (level->player->pos.y < 200) {
            float shift = ceilf(200 - level->player->pos.y);
            level->yshift += shift;
            level->player->pos.y += shift;
            if (obj->ground != NULL) {
                obj->ground->pos.y += shift;
                obj->sprite_ground_pos.y += shift;
            }
            i = 0;
            if (obj->blocks != NULL) {
                while (obj->blocks[i] != NULL) {
                    obj->blocks[i]->pos.y += shift;
                    i += 1;
                }
            }
            i = 0;
            if (obj->spikes != NULL) {
                while (obj->spikes[i] != NULL) {
                    obj->spikes[i]->pos.y += shift;
                    i += 1;
                }
            }
            i = 0;
            if (obj->portals != NULL) {
                while (obj->portals[i] != NULL) {
                    obj->portals[i]->pos.y += shift;
                    i += 1;
                }
            }
        }
        if (level->player->pos.y > 790 && level->yshift != 0) {
            float shift = floorf(790 - level->player->pos.y);
            level->yshift += shift;
            level->player->pos.y += shift;
            if (obj->ground != NULL) {
                obj->ground->pos.y += shift;
                obj->sprite_ground_pos.y += shift;
            }
            i = 0;
            if (obj->blocks != NULL) {
                while (obj->blocks[i] != NULL) {
                    obj->blocks[i]->pos.y += shift;
                    i += 1;
                }
            }
            i = 0;
            if (obj->spikes != NULL) {
                while (obj->spikes[i] != NULL) {
                    obj->spikes[i]->pos.y += shift;
                    i += 1;
                }
            }
            i = 0;
            if (obj->portals != NULL) {
                while (obj->portals[i] != NULL) {
                    obj->portals[i]->pos.y += shift;
                    i += 1;
                }
            }
        }
    }
}

void apply_gravity(level_t *level)
{
    if (level->player->gamemode == 'c') {
        level->player->vy -= 1.4;
        if (level->player->vy > 43)
            level->player->vy = 43;
        if (level->player->vy < -43)
            level->player->vy = -43;
    }
    if (level->player->gamemode == 'p') {
        level->player->vy -= 1.55;
        if (level->player->vy > 28)
            level->player->vy = 28;
        if (level->player->vy < -28)
            level->player->vy = -28;
    }
    level->player->pos.y -= level->player->vy;
}

void check_portal_boundary(gd_t *gd, level_t *level)
{
    float player_radius = 25 * level->player->size;
    float top_kill_y;
    float bottom_kill_y;

    if (level->objects->portal_blocks == NULL || level->player->gamemode == 'c')
        return;
    if (level->player->state == 'd')
        return;
    top_kill_y    = level->objects->portal_blocks[0]->pos.y;
    bottom_kill_y = level->objects->portal_blocks[1]->pos.y;

    if (level->player->pos.y <= 100 + player_radius) {
        level->player->pos.y = 100 + player_radius;
        level->player->vy = 0;
        return;
    }
    if (level->player->pos.y + player_radius > bottom_kill_y) {
        level->player->pos.y = bottom_kill_y - player_radius;
        level->player->vy = 0;
        return;
    }
}

void check_ground_collision(level_t *level)
{
    sfVector2f p_pos = level->player->pos;
    sfVector2f b_pos = level->objects->ground->pos;
    float block_size = 100;
    float player_radius = 25 * level->player->size;

    if (level->player->state == 'd')
        return;
    float block_left = b_pos.x;
    float block_right = b_pos.x + block_size;
    float block_top = b_pos.y;
    float block_bottom = b_pos.y + block_size;
    
    float player_left = p_pos.x - player_radius + 10;
    float player_right = p_pos.x + player_radius - 10;
    float player_top = p_pos.y - player_radius + 10;
    float player_bottom = p_pos.y + player_radius;
    
    if (player_right <= block_left || player_left >= block_right ||
        player_bottom <= block_top || player_top >= block_bottom) {
        return;
    }
    
    float penetration_top = player_bottom - block_top;
    float penetration_bottom = block_bottom - player_top;
    float penetration_left = player_right - block_left;
    
    float min_penetration = penetration_top;
    char side = 't';
    
    if (penetration_bottom < min_penetration) {
        min_penetration = penetration_bottom;
        side = 'b';
    }
    if (penetration_left < min_penetration) {
        min_penetration = penetration_left;
        side = 'l';
    }
    
    if (side == 't') {
        level->player->pos.y = block_top - player_radius;
        level->player->vy = 0;
        level->player->allow_jump = 'y';
    }
}

void apply_physics(gd_t *gd, level_t *level, object_list_t *obj)
{
    if (level->level_completed == 'y')
        return;
    move_objects(level, obj);
    apply_gravity(level);
    check_portal_boundary(gd, level);
}

void level_complete(level_t *level)
{
    if (level->percent > level->best) {
        level->best = 100;
    }
    level->level_completed = 'y';
    level->speed = 0;
}

void check_spike(gd_t *gd, level_t *level, object_list_t *obj, int i)
{
    sfVector2f p1 = level->player->pos;
    sfVector2f p2 = obj->spikes[i]->pos;
    float player_radius = 25 * level->player->size;

    if (level->player->state == 'd')
        return;
    
    float player_left = p1.x - player_radius;
    float player_right = p1.x + player_radius;
    float player_top = p1.y - player_radius;
    float player_bottom = p1.y + player_radius;
    
    float spike_left = p2.x + 30;
    float spike_right = p2.x + 70;
    float spike_top = p2.y + obj->spikes[i]->size * 10;
    float spike_bottom = p2.y + 100;
    
    if (player_right > spike_left && player_left < spike_right &&
        player_bottom > spike_top && player_top < spike_bottom) {
        player_dead(level->player, level, level->objects, gd);
    }
}

block_t *create_boundary_block(portal_t *portal, gd_t *gd, sfVector2f *pos, sfVector2f *sprite_pos)
{
    block_t *block = malloc(sizeof(block_t));

    block->size = 2;
    block->pos = *pos;
    block->sprite = sfSprite_create();
    sfSprite_setTexture(block->sprite, gd->res->block, sfTrue);
    sfSprite_setPosition(block->sprite, *sprite_pos);
    return block;
}

void create_boundaries(block_t **list, portal_t *portal, gd_t *gd)
{
    sfVector2f top_pos    = {325.0f, 50};
    sfVector2f top_sprite = {0.0f, 0.0f};
    sfVector2f bot_pos    = {325.0f, 950};
    sfVector2f bot_sprite = {0.0f, 950};

    list[0] = create_boundary_block(portal, gd, &top_pos, &top_sprite);
    list[1] = create_boundary_block(portal, gd, &bot_pos, &bot_sprite);
    list[2] = NULL;
}

void remove_create_portal_boundaries(level_t *level, gd_t *gd, portal_t *portal)
{
    if (level->objects->portal_blocks != NULL && level->player->gamemode != portal->gamemode) {
        free_block_list(level->objects->portal_blocks);
        level->objects->portal_blocks = NULL;
        if (portal->gamemode == 'p') {
            level->objects->portal_blocks = malloc(sizeof(block_t *) * 3);
            create_boundaries(level->objects->portal_blocks, portal, gd);
        }
        return;
    }
    if (portal->gamemode == 'p') {
        level->objects->portal_blocks = malloc(sizeof(block_t *) * 3);
        create_boundaries(level->objects->portal_blocks, portal, gd);
    }
}

void portal_shift(level_t *level, portal_t *portal)
{
    float target_y_shift = ((portal->pos.y) - 500) * (-1);
    int i = 0;

    level->player->pos.y += target_y_shift;
    level->yshift += target_y_shift;
    level->player->pos.y += target_y_shift;
    if (level->objects->ground != NULL) {
        level->objects->ground->pos.y += target_y_shift;
        level->objects->sprite_ground_pos.y += target_y_shift;
    }
    i = 0;
    if (level->objects->blocks != NULL) {
        while (level->objects->blocks[i] != NULL) {
            level->objects->blocks[i]->pos.y += target_y_shift;
            i += 1;
        }
    }
    i = 0;
    if (level->objects->spikes != NULL) {
        while (level->objects->spikes[i] != NULL) {
            level->objects->spikes[i]->pos.y += target_y_shift;
            i += 1;
        }
    }
    i = 0;
    if (level->objects->portals != NULL) {
        while (level->objects->portals[i] != NULL) {
            level->objects->portals[i]->pos.y += target_y_shift;
            i += 1;
        }
    }
}

void check_portal(gd_t *gd, level_t *level, object_list_t *obj, int i)
{
    sfVector2f p1 = level->player->pos;
    sfVector2f p2 = obj->portals[i]->pos;
    float player_radius = 25 * level->player->size;

    if (level->player->state == 'd')
        return;
    
    float player_left = p1.x - player_radius;
    float player_right = p1.x + player_radius;
    float player_top = p1.y - player_radius;
    float player_bottom = p1.y + player_radius;
    
    float spike_left = p2.x;
    float spike_right = p2.x + obj->portals[i]->size * 50;
    float spike_top = p2.y;
    float spike_bottom = p2.y + obj->portals[i]->size * 100;
    
    if (player_right > spike_left && player_left < spike_right &&
        player_bottom > spike_top && player_top < spike_bottom) {
        if (obj->portals[i]->gamemode != 'c') {
            portal_shift(level, level->objects->portals[i]);
        }
        remove_create_portal_boundaries(level, gd, obj->portals[i]);
        load_new_player_texture(level->player, obj->portals[i]->gamemode, gd);
        level->player->gamemode = obj->portals[i]->gamemode;
    }
}

void check_on_block(gd_t *gd, level_t *level, object_list_t *obj, int i)
{
    sfVector2f p_pos = level->player->pos;
    sfVector2f b_pos = obj->blocks[i]->pos;
    float block_size = obj->blocks[i]->size * 50;
    float player_radius = 25 * level->player->size;

    if (level->player->state == 'd')
        return;
    float block_left = b_pos.x;
    float block_right = b_pos.x + block_size;
    float block_top = b_pos.y;
    float block_bottom = b_pos.y + block_size;

    float player_left = p_pos.x - player_radius + 10;
    float player_right = p_pos.x + player_radius - 10;
    float player_top = p_pos.y - player_radius + 10;
    float player_bottom = p_pos.y + player_radius;

    if (player_right <= block_left || player_left >= block_right ||
        player_bottom <= block_top || player_top >= block_bottom) {
        return;
    }

    float penetration_top = player_bottom - block_top;
    float penetration_bottom = block_bottom - player_top;
    float penetration_left = player_right - block_left;

    float min_penetration = penetration_top;
    char side = 't';
    
    if (penetration_bottom < min_penetration) {
        min_penetration = penetration_bottom;
        side = 'b';
    }
    if (penetration_left < min_penetration) {
        min_penetration = penetration_left;
        side = 'l';
    }

    if (side == 't') {
        level->player->pos.y = block_top - player_radius;
        level->player->vy = 0;
        level->player->allow_jump = 'y';
    } else {
        if (side == 'l') {
            player_dead(level->player, level, level->objects, gd);
            return;
        }
        if (level->player->gamemode == 'p' && side == 'b') {
            level->player->pos.y = block_bottom + player_radius;
            level->player->vy = 0;
            return;
        }
        player_dead(level->player, level, level->objects, gd);
    }
}

void check_collisions(gd_t *gd, level_t *level, object_list_t *obj)
{
    int i = 0;

    if (level->level_completed == 'y')
        return;
    
    if (level->shift >= level->level_end) {
        level_complete(level);
        return;
    }

    if (level->player->gamemode == 'c')
        level->player->allow_jump = 'n';

    check_ground_collision(level);
    if (obj->blocks != NULL) {
        while (obj->blocks[i] != NULL) {
            if (obj->blocks[i]->pos.x < 600 && obj->blocks[i]->pos.x > 100)
                check_on_block(gd, level, obj, i);
            i += 1;
        }
    }
    
    i = 0;
    if (obj->spikes != NULL) {
        while (obj->spikes[i] != NULL) {
            if (obj->spikes[i]->pos.x < 550 && obj->spikes[i]->pos.x > 100)
                check_spike(gd, level, obj, i);
            i += 1;
        }
    }

    i = 0;
    if (obj->portals != NULL) {
        while (obj->portals[i] != NULL) {
            if (obj->portals[i]->pos.x < 550 && obj->portals[i]->pos.x > 100)
                check_portal(gd, level, obj, i);
            i += 1;
        }
    }
}
