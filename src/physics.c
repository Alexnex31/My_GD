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

    level->shift += 11 * level->speed;
    if (obj->blocks != NULL) {
        while (obj->blocks[i] != NULL) {
            obj->blocks[i]->pos.x -= 11 * level->speed;
            i += 1;
        }
    }
    i = 0;
    if (obj->spikes != NULL) {
        while (obj->spikes[i] != NULL) {
            obj->spikes[i]->pos.x -= 11 * level->speed;
            i += 1;
        }
    }
}

void apply_gravity(level_t *level)
{
    // Appliquer la gravité
    level->player->vy -= 1.3;
    
    // Limiter la vitesse de chute maximale
    if (level->player->vy < -40)
        level->player->vy = -40;
    
    // Appliquer la vélocité verticale à la position
    level->player->pos.y -= level->player->vy;
}

void check_ground_collision(level_t *level)
{
    // Vérifier collision avec le sol
    if (level->player->pos.y > 790) {
        level->player->pos.y = 790;
        level->player->vy = 0;
        level->player->allow_jump = 'y';
    }
}

void apply_physics(gd_t *gd, level_t *level, object_list_t *obj)
{
    if (level->level_completed == 'y')
        return;
    
    // Déplacer les objets
    move_objects(level, obj);
    
    // Appliquer la gravité
    apply_gravity(level);
    
    // Vérifier collision avec le sol de base
    check_ground_collision(level);
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
    if (level->percent > level->best) {
        level->best = level->percent;
    }
    level->attempts += 1;
    level->curr_attempts += 1;
    player->state = 'd';
    move_objects_back(level, level->objects);
    player->pos = (sfVector2f){350, 790};
    player->vy = 0;
    player->state = 'a';
    player->allow_jump = 'n';
    reset_attempt_display(level);
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
    
    // AABB simple pour les spikes
    float player_left = p1.x - player_radius;
    float player_right = p1.x + player_radius;
    float player_top = p1.y - player_radius;
    float player_bottom = p1.y + player_radius;
    
    float spike_left = p2.x + 30;
    float spike_right = p2.x + 70;
    float spike_top = p2.y + obj->spikes[i]->size * 20;
    float spike_bottom = p2.y + 100;
    
    // Vérifier collision
    if (player_right > spike_left && player_left < spike_right &&
        player_bottom > spike_top && player_top < spike_bottom) {
        player_dead(level->player, level, level->objects);
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
    
    // Définir les limites du bloc
    float block_left = b_pos.x;
    float block_right = b_pos.x + block_size;
    float block_top = b_pos.y;
    float block_bottom = b_pos.y + block_size;
    
    // Définir les limites du joueur
    float player_left = p_pos.x - player_radius + 10;
    float player_right = p_pos.x + player_radius - 10;
    float player_top = p_pos.y - player_radius + 10;
    float player_bottom = p_pos.y + player_radius;
    
    // Vérifier s'il y a collision
    if (player_right <= block_left || player_left >= block_right ||
        player_bottom <= block_top || player_top >= block_bottom) {
        return; // Pas de collision
    }
    
    // Il y a collision - calculer les pénétrations
    float penetration_top = player_bottom - block_top;
    float penetration_bottom = block_bottom - player_top;
    float penetration_left = player_right - block_left;
    
    // Trouver la pénétration minimale
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
    
    // Gérer la collision selon le côté
    if (side == 't') {
        // Collision par le haut - le joueur atterrit sur le bloc
        level->player->pos.y = block_top - player_radius;
        level->player->vy = 0;
        level->player->allow_jump = 'y';
    } else {
        // Collision latérale = mort
        player_dead(level->player, level, level->objects);
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
    
    // Vérifier collisions avec les blocs
    if (obj->blocks != NULL) {
        while (obj->blocks[i] != NULL) {
            if (obj->blocks[i]->pos.x < 600 && obj->blocks[i]->pos.x > 100)
                check_on_block(gd, level, obj, i);
            i += 1;
        }
    }
    
    // Vérifier collisions avec les spikes
    i = 0;
    if (obj->spikes != NULL) {
        while (obj->spikes[i] != NULL) {
            if (obj->spikes[i]->pos.x < 550 && obj->spikes[i]->pos.x > 100)
                check_spike(gd, level, obj, i);
            i += 1;
        }
    }
}
