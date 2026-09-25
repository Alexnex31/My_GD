/*
** ALEXNEX PROJECT, 2026
** sim/internal.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_INTERNAL_H
    #define SIM_INTERNAL_H

    #include "sim/sim_types.h"

/* The pieces of one tick (3.4). Only the simulation calls these. */

void player_update_hold(player_t *p, input_t in);
void player_apply_input(player_t *p, input_t in);
void player_apply_gravity(player_t *p);
void player_flip_gravity(player_t *p);
void player_update_rotation(player_t *p);          /* cosmetic (9.4) */

void move_and_collide(sim_t *s);                   /* 4.4 */
void collide_kill_ceiling(player_t *p, const level_data_t *lvl);   /* 4.7 */
void update_can_jump(sim_t *s);                    /* the jump zone (4.3) */
void camera_follow(camera_t *c, const player_t *p, const ship_bounds_t *b);

#endif
