/*
** ALEXNEX PROJECT, 2026
** sim/hitbox.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_HITBOX_H
    #define SIM_HITBOX_H

    #include "sim/sim_types.h"

/* Builds shapes once at load, rotation included (4.2, G.2). */
void hitbox_build_poly(hitbox_t *h, const vec2_t *local, int n, rect_t rect,
    double deg);
void hitbox_build_circle(hitbox_t *h, rect_t rect, double radius);
void hitbox_for_object(object_t *o);

/* Axis k: 0 is y, 1 is x, then the shape's own axes (G.2). */
vec2_t hitbox_axis(const hitbox_t *h, int k);
double hitbox_lo(const hitbox_t *h, int k);
double hitbox_hi(const hitbox_t *h, int k);

/* Edge i, if it is horizontal and faces the player's up (G.7). */
bool up_facing_horizontal_face(const hitbox_t *h, int i, double g, face_t *out);

#endif
