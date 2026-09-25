/*
** ALEXNEX PROJECT, 2026
** sim/geom.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_GEOM_H
    #define SIM_GEOM_H

    #include <math.h>

    #include "sim/sim_types.h"

/* x grows right, y grows down; polygons are clockwise on screen (G.1) */

static inline double dot(vec2_t a, vec2_t b)
{
    return a.x * b.x + a.y * b.y;
}

static inline double cross(vec2_t a, vec2_t b)
{
    return a.x * b.y - a.y * b.x;
}

static inline vec2_t vadd(vec2_t a, vec2_t b)
{
    return (vec2_t){a.x + b.x, a.y + b.y};
}

static inline vec2_t vsub(vec2_t a, vec2_t b)
{
    return (vec2_t){a.x - b.x, a.y - b.y};
}

static inline vec2_t vscale(vec2_t a, double k)
{
    return (vec2_t){a.x * k, a.y * k};
}

static inline double vlen(vec2_t a)
{
    return sqrt(dot(a, a));
}

bool rect_overlap(rect_t a, rect_t b);   /* touching is not overlapping (3.0) */

#endif
