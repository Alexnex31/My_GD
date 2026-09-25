/*
** ALEXNEX PROJECT, 2026
** sim/sweep.c
** File description:
** swept and static tests of the player's shapes against level shapes (4.3, G.3)
*/

#include <math.h>

#include "sim/geom.h"
#include "sim/hitbox.h"
#include "sim/sweep.h"

typedef struct sat {
    double t_in;         /* when every axis overlaps: the contact                */
    double t_out;        /* when the first axis stops overlapping                */
    vec2_t n_in;         /* the normal of the axis entered last, toward the box  */
    int k_in;            /* that axis, -1 while no axis moves                    */
    double offset;       /* the face's line: dot(n_in, p) = offset               */
} sat_t;

/*
** Separating axis theorem over time, for an axis-aligned square of half h
** moving by d. Returns false as soon as one axis can never overlap.
*/
static bool sat_axis(vec2_t c, double h, vec2_t d, int k,
    const hitbox_t *hb, sat_t *s)
{
    vec2_t a = hitbox_axis(hb, k);
    double r = h * (fabs(a.x) + fabs(a.y));
    double lo = hitbox_lo(hb, k) - r;
    double hi = hitbox_hi(hb, k) + r;
    double p = dot(c, a);
    double v = dot(d, a);
    double t0 = 0.0;
    double t1 = 0.0;

    if (v == 0.0)
        return !(p <= lo || p >= hi);        /* touching is not overlapping */
    t0 = (lo - p) / v;
    t1 = (hi - p) / v;
    if (t0 > t1) {
        double tmp = t0;

        t0 = t1;
        t1 = tmp;
    }
    if (t0 > s->t_in) {                      /* strict: on a tie the earlier axis wins */
        s->t_in = t0;
        s->n_in = v > 0.0 ? (vec2_t){-a.x, -a.y} : a;
        s->k_in = k;
        s->offset = v > 0.0 ? -hitbox_lo(hb, k) : hitbox_hi(hb, k);
    }
    s->t_out = fmin(s->t_out, t1);
    return s->t_in < s->t_out;
}

static bool sat_box_poly(vec2_t c, double h, vec2_t d, const hitbox_t *hb,
    sat_t *s)
{
    *s = (sat_t){-INFINITY, INFINITY, {0.0, 0.0}, -1, 0.0};
    for (int k = 0; k < hb->naxes + 2; k++)
        if (!sat_axis(c, h, d, k, hb, s))
            return false;
    return s->t_in <= 1.0 && s->t_out > 0.0;
}

bool sweep_box_poly(vec2_t c, double h, vec2_t d, const hitbox_t *hb,
    contact_t *out)
{
    sat_t s;

    if (!sat_box_poly(c, h, d, hb, &s) || s.k_in < 0)
        return false;                        /* never overlap, or nothing moves */
    if (dot(s.n_in, d) >= 0.0)
        return false;                        /* moving away from, or along, that face */
    out->t = s.t_in < 0.0 ? 0.0 : s.t_in;    /* already overlapping: contact now */
    out->normal = s.n_in;
    out->flat = s.k_in == 0;
    out->offset = s.offset;
    out->surface = 0;
    return true;
}

double sweep_box_touch(vec2_t c, double h, vec2_t d, const hitbox_t *hb)
{
    sat_t s;

    if (!sat_box_poly(c, h, d, hb, &s))
        return INFINITY;
    if (s.k_in < 0)
        return 0.0;                          /* overlapping, and no axis moves */
    return s.t_in < 0.0 ? 0.0 : s.t_in;
}

bool overlap_box_poly(vec2_t c, double h, const hitbox_t *hb)
{
    for (int k = 0; k < hb->naxes + 2; k++) {
        vec2_t a = hitbox_axis(hb, k);
        double r = h * (fabs(a.x) + fabs(a.y));
        double p = dot(c, a);

        if (p + r <= hitbox_lo(hb, k) || p - r >= hitbox_hi(hb, k))
            return false;                    /* apart, or exactly touching */
    }
    return true;
}

/*
** A half-plane: everything on the side of y = sy the surface is solid on.
** The shape reaches it with the point half a size toward it (G.5).
*/
static bool sweep_plane(vec2_t c, double reach, vec2_t d, double sy,
    double side, contact_t *out)
{
    double edge = c.y + side * reach;
    double t = 0.0;

    if (d.y * side <= 0.0)
        return false;                        /* not moving into the surface */
    if ((edge - sy) * side < 0.0) {
        t = (sy - side * reach - c.y) / d.y;
        if (t > 1.0)
            return false;
    }
    out->t = t < 0.0 ? 0.0 : t;
    out->normal = (vec2_t){0.0, -side};
    out->flat = true;
    out->offset = -side * sy;
    out->surface = SURF_GROUND;              /* the caller names the surface */
    return true;
}

bool sweep_box_plane(vec2_t c, double h, vec2_t d, double sy, double side,
    contact_t *out)
{
    return sweep_plane(c, h, d, sy, side, out);
}

bool sweep_circle_plane(vec2_t c, double r, vec2_t d, double sy, double side,
    contact_t *out)
{
    return sweep_plane(c, r, d, sy, side, out);
}

/*
** Closest point of a convex polygon to c, and whether c is inside it (G.4).
** dist is 0 when inside.
*/
static double poly_closest(vec2_t c, const hitbox_t *hb, vec2_t *out)
{
    bool inside = true;
    double best = INFINITY;

    for (int i = 0; i < hb->nverts; i++) {
        vec2_t a = hb->verts[i];
        vec2_t e = vsub(hb->verts[(i + 1) % hb->nverts], a);
        double u = dot(vsub(c, a), e) / dot(e, e);
        vec2_t q;
        double dist;

        if (dot(hb->face_n[i], c) > hb->face_off[i])
            inside = false;
        u = fmin(fmax(u, 0.0), 1.0);
        q = vadd(a, vscale(e, u));
        dist = vlen(vsub(c, q));
        if (dist < best) {
            best = dist;
            *out = q;
        }
    }
    return inside ? 0.0 : best;
}

double poly_distance(vec2_t c, const hitbox_t *hb)
{
    vec2_t q;

    return poly_closest(c, hb, &q);
}

bool overlap_circle_poly(vec2_t c, double r, const hitbox_t *hb)
{
    return poly_distance(c, hb) < r;         /* touching is not overlapping */
}

/* The normal of a contact that is already there at t = 0 (G.4). */
static vec2_t stuck_normal(vec2_t c, const hitbox_t *hb)
{
    vec2_t q;
    int best = 0;

    if (poly_closest(c, hb, &q) > 0.0)
        return vscale(vsub(c, q), 1.0 / vlen(vsub(c, q)));
    for (int i = 1; i < hb->nverts; i++)     /* inside: the shallowest face */
        if (hb->face_off[i] - dot(hb->face_n[i], c)
            < hb->face_off[best] - dot(hb->face_n[best], c))
            best = i;
    return hb->face_n[best];
}

static void circle_faces(vec2_t c, double r, vec2_t d, const hitbox_t *hb,
    face_ok_fn fok, double g, contact_t *out)
{
    for (int i = 0; i < hb->nverts; i++) {
        vec2_t n = hb->face_n[i];
        vec2_t a = hb->verts[i];
        vec2_t b = hb->verts[(i + 1) % hb->nverts];
        double den = dot(n, d);
        double t;
        vec2_t p;

        if ((fok != NULL && !fok(hb, i, g)) || den >= 0.0)
            continue;                        /* filtered out, or not moving into it */
        t = (hb->face_off[i] + r - dot(n, c)) / den;
        if (t < 0.0 || t > 1.0 || t >= out->t)
            continue;
        p = vsub(vadd(c, vscale(d, t)), vscale(n, r));   /* the touch point */
        if (dot(vsub(p, a), vsub(b, a)) < 0.0 || dot(vsub(p, b), vsub(b, a)) > 0.0)
            continue;                        /* beyond the face's ends */
        out->t = t;
        out->normal = n;
        out->flat = false;
    }
}

static void circle_vertices(vec2_t c, double r, vec2_t d, const hitbox_t *hb,
    vertex_ok_fn vok, double g, contact_t *out)
{
    double a = dot(d, d);

    for (int i = 0; a > 0.0 && i < hb->nverts; i++) {
        vec2_t f = vsub(c, hb->verts[i]);
        double b = dot(f, d);
        double cc = dot(f, f) - r * r;
        double disc = b * b - a * cc;
        double t;
        vec2_t n;

        if (vok != NULL && !vok(hb, i, g))
            continue;
        if (disc < 0.0)
            continue;                        /* the path misses this corner */
        t = (-b - sqrt(disc)) / a;           /* the entering root */
        if (t < 0.0 || t > 1.0 || t >= out->t)
            continue;
        n = vscale(vsub(vadd(c, vscale(d, t)), hb->verts[i]), 1.0 / r);
        if (dot(n, d) >= 0.0)
            continue;                        /* moving away from that corner */
        out->t = t;
        out->normal = n;
        out->flat = false;
    }
}

bool sweep_circle_poly(vec2_t c, double r, vec2_t d, const hitbox_t *hb,
    face_ok_fn fok, vertex_ok_fn vok, double g, contact_t *out)
{
    contact_t best = {INFINITY, {0.0, 0.0}, false, 0.0, 0};

    if (poly_distance(c, hb) < r) {          /* already overlapping: contact now */
        out->t = 0.0;
        out->normal = stuck_normal(c, hb);
        out->flat = false;
        out->offset = 0.0;
        out->surface = 0;
        return dot(out->normal, d) < 0.0;
    }
    circle_faces(c, r, d, hb, fok, g, &best);
    circle_vertices(c, r, d, hb, vok, g, &best);
    if (best.t == INFINITY)
        return false;
    *out = best;
    return true;
}

double sweep_circle_touch(vec2_t c, double r, vec2_t d, const hitbox_t *hb)
{
    contact_t best = {INFINITY, {0.0, 0.0}, false, 0.0, 0};

    if (poly_distance(c, hb) < r)
        return 0.0;
    circle_faces(c, r, d, hb, NULL, 0.0, &best);
    circle_vertices(c, r, d, hb, NULL, 0.0, &best);
    return best.t;
}

int clip_half_plane(const vec2_t *in, int n, double y_line, double g,
    vec2_t *out)
{
    int m = 0;

    for (int i = 0; i < n; i++) {
        vec2_t a = in[i];
        vec2_t b = in[(i + 1) % n];
        double da = (a.y - y_line) * g;      /* >= 0: on the kept side */
        double db = (b.y - y_line) * g;

        if (da >= 0.0) {
            out[m] = a;
            m += 1;
        }
        if ((da > 0.0 && db < 0.0) || (da < 0.0 && db > 0.0)) {
            double u = da / (da - db);

            out[m] = vadd(a, vscale(vsub(b, a), u));
            m += 1;
        }
    }
    return m;
}

/* The point q inside a rectangle of half extents (hx, hy), moving by v (G.10). */
static double rect_entry(vec2_t q, vec2_t v, double hx, double hy)
{
    double half[2] = {hx, hy};
    double pos[2] = {q.x, q.y};
    double vel[2] = {v.x, v.y};
    double t_in = -INFINITY;
    double t_out = INFINITY;

    for (int k = 0; k < 2; k++) {
        double t0 = 0.0;
        double t1 = 0.0;

        if (vel[k] == 0.0) {
            if (fabs(pos[k]) >= half[k])
                return INFINITY;
            continue;
        }
        t0 = (-half[k] - pos[k]) / vel[k];
        t1 = (half[k] - pos[k]) / vel[k];
        t_in = fmax(t_in, fmin(t0, t1));
        t_out = fmin(t_out, fmax(t0, t1));
    }
    if (t_in >= t_out || t_in > 1.0 || t_out <= 0.0)
        return INFINITY;
    return t_in < 0.0 ? 0.0 : t_in;
}

double sweep_box_disc(vec2_t c, double h, vec2_t d, vec2_t center, double r)
{
    vec2_t q = vsub(center, c);              /* the disc, in the square's frame */
    vec2_t v = vscale(d, -1.0);
    double a = dot(d, d);
    double best = fmin(rect_entry(q, v, h + r, h), rect_entry(q, v, h, h + r));

    for (int i = 0; a > 0.0 && i < 4; i++) {
        vec2_t k = {i < 2 ? -h : h, i % 2 == 0 ? -h : h};
        vec2_t f = vsub(q, k);
        double b = dot(f, d);
        double cc = dot(f, f) - r * r;
        double disc = b * b - a * cc;
        double t;

        if (disc < 0.0)
            continue;
        t = (b - sqrt(disc)) / a;
        if (t >= 0.0 && t <= 1.0 && t < best)
            best = t;
    }
    return best;
}

/* The same distance, for a bare vertex list: the clipped jump zone (G.6). */
double poly_points_distance(vec2_t c, const vec2_t *v, int n)
{
    bool inside = n > 2;
    double best = INFINITY;

    for (int i = 0; i < n; i++) {
        vec2_t a = v[i];
        vec2_t e = vsub(v[(i + 1) % n], a);
        double len = dot(e, e);
        double u = len > 0.0 ? dot(vsub(c, a), e) / len : 0.0;
        vec2_t q;

        if (cross(e, vsub(c, a)) < 0.0)      /* clockwise on screen: inside is >= 0 */
            inside = false;
        u = fmin(fmax(u, 0.0), 1.0);
        q = vadd(a, vscale(e, u));
        best = fmin(best, vlen(vsub(c, q)));
    }
    return inside ? 0.0 : best;
}
