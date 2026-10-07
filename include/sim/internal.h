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
void player_apply_gravity(player_t *p);            /* the forces of every mode that falls */
void player_flip_gravity(player_t *p);

/*
** The modes' own code, one file each, reached through MODES[] only
** (FEATURES 6.1). They change the velocity and never move the player. The
** rotation is cosmetic (9.4): computed in the tick so it doesn't depend on
** the frame rate, and left out of the state hash because it decides nothing.
*/
void cube_input(player_t *p, input_t in);
void cube_rotation(player_t *p);
void ship_input(player_t *p, input_t in);
void ship_rotation(player_t *p);
void ufo_input(player_t *p, input_t in);
void ufo_rotation(player_t *p);
void wave_input(player_t *p, input_t in);
void wave_forces(player_t *p);
void wave_rotation(player_t *p);
void ball_input(player_t *p, input_t in);
void ball_rotation(player_t *p);

void move_and_collide(sim_t *s);                   /* 4.4 */
void collide_kill_ceiling(player_t *p, const level_data_t *lvl);   /* 4.7 */
void update_can_jump(sim_t *s);                    /* the jump zone (4.3) */
void leg_touches(sim_t *s, vec2_t d, double t_end);   /* interactive, 4.6 */
void apply_interactive(sim_t *s);                  /* tick step 5 (4.6, 5.1) */
input_t activate_orbs(sim_t *s, input_t in);       /* tick step 0 (FEATURES 10.2) */
void corridor_from_center(sim_t *s, double center);   /* 5.2, and the start */
void camera_follow(camera_t *c, const player_t *p, const ship_bounds_t *b);
double camera_rest_y(const player_t *p, const ship_bounds_t *b);   /* 7.2 */

#endif
