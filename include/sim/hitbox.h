/*
** ALEXNEX PROJECT, 2026
** sim/hitbox.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_HITBOX_H
    #define SIM_HITBOX_H

    #include <math.h>

    #include "sim/sim_types.h"

/* Builds shapes once at load, rotation included (4.2, G.2). */
void hitbox_build_poly(hitbox_t *h, const vec2_t *local, int n, rect_t rect,
    double deg);
void hitbox_build_circle(hitbox_t *h, rect_t rect, double radius);
void hitbox_for_object(object_t *o);

/*
** Axis k: 0 is y, 1 is x, then the shape's own axes (G.2). Built once at load
** into one flat array, so a sweep reads them without a branch.
*/
static inline vec2_t hitbox_axis(const hitbox_t *h, int k)
{
    return h->axes[k];
}

static inline double hitbox_lo(const hitbox_t *h, int k)
{
    return h->axis_lo[k];
}

static inline double hitbox_hi(const hitbox_t *h, int k)
{
    return h->axis_hi[k];
}

/* The half extent a square of half h covers on axis k: h * (|a.x| + |a.y|). */
static inline double hitbox_extent(const hitbox_t *h, int k)
{
    return h->axis_extent[k];
}

/* Edge i, if it is horizontal and faces the player's up (G.7). Inline: the
** step search asks it about every edge of every candidate, every leg. */
static inline bool up_facing_horizontal_face(const hitbox_t *h, int i,
    double g, face_t *out)
{
    vec2_t a;
    vec2_t b;

    if (i >= h->nverts || h->face_kind[i] != FACE_HORIZONTAL
        || h->face_n[i].y * g >= 0.0)
        return false;
    a = h->verts[i];
    b = h->verts[(i + 1) % h->nverts];
    out->y = a.y;
    out->x0 = fmin(a.x, b.x);
    out->x1 = fmax(a.x, b.x);
    return true;
}

#endif
