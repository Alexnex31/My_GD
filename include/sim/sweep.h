/*
** ALEXNEX PROJECT, 2026
** sim/sweep.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_SWEEP_H
    #define SIM_SWEEP_H

    #include "sim/sim_types.h"

/*
** A "sweep" moves one of the player's shapes by d and reports the first moment
** it meets an obstacle (4.3). Touching is never overlapping (3.0), so a shape
** resting on a face or sliding along it reports nothing.
** The touch versions return the time in 0..1, or INFINITY when there is none.
*/

/* The circle only meets some faces and corners of a shape (G.9): these filters
** say which. Passing NULL allows all of them. */
typedef bool (*face_ok_fn)(const hitbox_t *h, int i, double g);
typedef bool (*vertex_ok_fn)(const hitbox_t *h, int i, double g);

bool sweep_box_poly(vec2_t c, double h, vec2_t d, const hitbox_t *hb,
    contact_t *out);
double sweep_box_touch(vec2_t c, double h, vec2_t d, const hitbox_t *hb);
bool overlap_box_poly(vec2_t c, double h, const hitbox_t *hb);

/* The ground and the corridor's boundaries: half-planes (G.5).
** side = +1 when the solid part is below sy, -1 when it is above. */
bool sweep_box_plane(vec2_t c, double h, vec2_t d, double sy, double side,
    contact_t *out);
bool sweep_circle_plane(vec2_t c, double r, vec2_t d, double sy, double side,
    contact_t *out);

/* The circle against a polygon: the polygon inflated by r (G.4). */
bool sweep_circle_poly(vec2_t c, double r, vec2_t d, const hitbox_t *hb,
    face_ok_fn fok, vertex_ok_fn vok, double g, contact_t *out);
double sweep_circle_touch(vec2_t c, double r, vec2_t d, const hitbox_t *hb);
double poly_distance(vec2_t c, const hitbox_t *hb);     /* 0 when inside */
double poly_points_distance(vec2_t c, const vec2_t *v, int n);   /* same, raw */
bool overlap_circle_poly(vec2_t c, double r, const hitbox_t *hb);

/* The jump zone clips a shape to one half-plane before testing it (G.6).
** Keeps the side where (y - y_line) * g >= 0; out holds up to n + 1 vertices. */
int clip_half_plane(const vec2_t *in, int n, double y_line, double g,
    vec2_t *out);

/* The rigid square against a saw's disc (G.10): the first touch, or INFINITY. */
double sweep_box_disc(vec2_t c, double h, vec2_t d, vec2_t center, double r);

#endif
