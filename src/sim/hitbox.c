/*
** ALEXNEX PROJECT, 2026
** sim/hitbox.c
** File description:
** builds an object's collision shape once at load (4.2, G.2)
*/

#include <math.h>

#include "sim/geom.h"
#include "sim/hitbox.h"

#define AXIS_SAME 1e-12         /* two axes closer than this separate identically */

bool rect_overlap(rect_t a, rect_t b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w
        && a.y < b.y + b.h && b.y < a.y + a.h;
}

static double grid(double v)    /* 1/1024 px grid: exact, and the same everywhere */
{
    return round(v * 1024.0) / 1024.0;
}

static vec2_t rotate_point(vec2_t p, vec2_t c, double deg)
{
    double r = deg * M_PI / 180.0;
    double cs = cos(r);
    double sn = sin(r);

    return (vec2_t){c.x + (p.x - c.x) * cs - (p.y - c.y) * sn,
        c.y + (p.x - c.x) * sn + (p.y - c.y) * cs};
}

static rect_t bounds_of(const vec2_t *v, int n)
{
    rect_t r = {v[0].x, v[0].y, 0.0, 0.0};
    double max_x = v[0].x;
    double max_y = v[0].y;

    for (int i = 1; i < n; i++) {
        r.x = fmin(r.x, v[i].x);
        r.y = fmin(r.y, v[i].y);
        max_x = fmax(max_x, v[i].x);
        max_y = fmax(max_y, v[i].y);
    }
    r.w = max_x - r.x;
    r.h = max_y - r.y;
    return r;
}

/* With y down, clockwise on screen means a positive signed area (G.2). */
static void ensure_clockwise(vec2_t *v, int n)
{
    double area = 0.0;

    for (int i = 0; i < n; i++)
        area += cross(v[i], v[(i + 1) % n]);
    if (area >= 0.0)
        return;
    for (int i = 0; i < n / 2; i++) {
        vec2_t tmp = v[i];

        v[i] = v[n - 1 - i];
        v[n - 1 - i] = tmp;
    }
}

static void build_faces(hitbox_t *h)
{
    for (int i = 0; i < h->nverts; i++) {
        vec2_t a = h->verts[i];
        vec2_t b = h->verts[(i + 1) % h->nverts];
        vec2_t e = vsub(b, a);

        h->face_kind[i] = FACE_TILTED;
        if (a.y == b.y)
            h->face_kind[i] = FACE_HORIZONTAL;
        else if (a.x == b.x)
            h->face_kind[i] = FACE_VERTICAL;
        h->face_n[i] = vscale((vec2_t){e.y, -e.x}, 1.0 / vlen(e));
        h->face_off[i] = dot(h->face_n[i], a);
    }
}

static bool axis_is_new(const hitbox_t *h, vec2_t n)
{
    if (n.x == 0.0 || n.y == 0.0)       /* x and y are always tested anyway */
        return false;
    for (int i = 0; i < h->naxes; i++)
        if (fabs(dot(n, h->axes[i])) >= 1.0 - AXIS_SAME)
            return false;
    return true;
}

static void build_axes(hitbox_t *h)
{
    h->naxes = 0;
    for (int i = 0; i < h->nverts; i++) {
        vec2_t n = h->face_n[i];
        int k = h->naxes;

        if (!axis_is_new(h, n))
            continue;
        h->axes[k] = n;
        h->axis_lo[k] = dot(h->verts[0], n);
        h->axis_hi[k] = h->axis_lo[k];
        for (int j = 1; j < h->nverts; j++) {
            h->axis_lo[k] = fmin(h->axis_lo[k], dot(h->verts[j], n));
            h->axis_hi[k] = fmax(h->axis_hi[k], dot(h->verts[j], n));
        }
        h->naxes += 1;
    }
}

void hitbox_build_poly(hitbox_t *h, const vec2_t *local, int n, rect_t rect,
    double deg)
{
    vec2_t c = {rect.x + rect.w / 2.0, rect.y + rect.h / 2.0};

    h->kind = SHAPE_POLY;
    h->nverts = n;
    for (int i = 0; i < n; i++) {
        vec2_t v = rotate_point(local[i], c, deg);

        h->verts[i] = (vec2_t){grid(v.x), grid(v.y)};
    }
    ensure_clockwise(h->verts, n);
    h->aabb = bounds_of(h->verts, n);
    build_faces(h);
    build_axes(h);
}

void hitbox_build_circle(hitbox_t *h, rect_t rect, double radius)
{
    h->kind = SHAPE_CIRCLE;
    h->nverts = 0;
    h->naxes = 0;
    h->center = (vec2_t){grid(rect.x + rect.w / 2.0), grid(rect.y + rect.h / 2.0)};
    h->radius = grid(radius);
    h->aabb = (rect_t){h->center.x - h->radius, h->center.y - h->radius,
        h->radius * 2.0, h->radius * 2.0};
}

static void rect_corners(rect_t r, vec2_t *out)
{
    out[0] = (vec2_t){r.x, r.y};
    out[1] = (vec2_t){r.x + r.w, r.y};
    out[2] = (vec2_t){r.x + r.w, r.y + r.h};
    out[3] = (vec2_t){r.x, r.y + r.h};
}

/* Local shape per type, before rotation (4.2). */
void hitbox_for_object(object_t *o)
{
    rect_t r = o->rect;
    vec2_t v[HB_MAX_VERTS];

    if (o->type == OBJ_SLOPE) {
        v[0] = (vec2_t){r.x, r.y + r.h};
        v[1] = (vec2_t){r.x + r.w, r.y};
        v[2] = (vec2_t){r.x + r.w, r.y + r.h};
        hitbox_build_poly(&o->hitbox, v, 3, r, o->rotation);
        return;
    }
    if (o->type == OBJ_SPIKE)
        r = (rect_t){r.x + 0.3 * r.w, r.y + 0.2 * r.h, 0.4 * r.w, 0.8 * r.h};
    rect_corners(r, v);
    hitbox_build_poly(&o->hitbox, v, 4, o->rect, o->rotation);
}

vec2_t hitbox_axis(const hitbox_t *h, int k)
{
    if (k == 0)
        return (vec2_t){0.0, 1.0};
    if (k == 1)
        return (vec2_t){1.0, 0.0};
    return h->axes[k - 2];
}

double hitbox_lo(const hitbox_t *h, int k)
{
    if (k == 0)
        return h->aabb.y;
    if (k == 1)
        return h->aabb.x;
    return h->axis_lo[k - 2];
}

double hitbox_hi(const hitbox_t *h, int k)
{
    if (k == 0)
        return h->aabb.y + h->aabb.h;
    if (k == 1)
        return h->aabb.x + h->aabb.w;
    return h->axis_hi[k - 2];
}

bool up_facing_horizontal_face(const hitbox_t *h, int i, double g, face_t *out)
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
