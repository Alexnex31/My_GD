/*
** ALEXNEX PROJECT, 2026
** sim/zone.c
** File description:
** the jump zone: what lets the player jump, tested last in the tick (4.3, G.6)
*/

#include <math.h>

#include "sim/geom.h"
#include "sim/internal.h"
#include "sim/modes.h"
#include "sim/sweep.h"

#define ZONE_SKIN (2.0 * CONTACT_SKIN)       /* covers the gap kept from slopes */

/*
** The strip of the rigid square below the inner box (above it when gravity is
** flipped), grown by a skin because it is a support test, not a collision.
*/
static rect_t zone_rect(const player_t *p, double *y_line)
{
    const mode_ops_t *m = &MODES[p->mode];
    double g = p->gravity_dir;
    double a = p->pos.y + m->inner_half * g;
    double b = p->pos.y + m->half * g;
    rect_t r;

    *y_line = a;
    r.x = p->pos.x - m->half - ZONE_SKIN;
    r.w = 2.0 * (m->half + ZONE_SKIN);
    r.y = fmin(a, b) - ZONE_SKIN;
    r.h = fabs(b - a) + 2.0 * ZONE_SKIN;
    return r;
}

/* Inside the circle: any neutral shape on the zone's side of the line. */
static bool zone_dark(const player_t *p, const hitbox_t *h, double y_line)
{
    vec2_t clipped[HB_MAX_VERTS + 1];
    int n = clip_half_plane(h->verts, h->nverts, y_line, p->gravity_dir,
        clipped);

    if (n == 0)
        return false;
    return poly_points_distance(p->pos, clipped, n)
        <= MODES[p->mode].half + ZONE_SKIN;
}

static bool segment_in_rect(vec2_t a, vec2_t b, rect_t r)
{
    return fmin(a.x, b.x) <= r.x + r.w && fmax(a.x, b.x) >= r.x
        && fmin(a.y, b.y) <= r.y + r.h && fmax(a.y, b.y) >= r.y;
}

/* The square's corners, outside the circle: flat faces only (4.3). */
static bool zone_light(const hitbox_t *h, rect_t zone)
{
    for (int i = 0; i < h->nverts; i++) {
        vec2_t a = h->verts[i];
        vec2_t b = h->verts[(i + 1) % h->nverts];

        if (h->face_kind[i] == FACE_TILTED)
            continue;
        if (segment_in_rect(a, b, zone))
            return true;
    }
    return false;
}

/* The ground and the corridor's boundaries are flat faces too (4.3). */
static bool zone_surface(const sim_t *s, rect_t zone)
{
    const ship_bounds_t *b = &s->st.bounds;

    if (zone.y + zone.h >= GROUND_Y && zone.y <= GROUND_Y)
        return true;
    if (!b->active)
        return false;
    return (zone.y + zone.h >= b->bottom && zone.y <= b->bottom)
        || (zone.y + zone.h >= b->top && zone.y <= b->top);
}

/*
** Touching alone isn't enough: the player also has to be moving into its
** surface, which is what stops a second jump in mid-air (4.3).
*/
void update_can_jump(sim_t *s)
{
    player_t *p = &s->st.player;
    double y_line = 0.0;
    rect_t zone = zone_rect(p, &y_line);
    bool touched = zone_surface(s, zone);

    for (size_t k = 0; !touched && k < s->nb_cand; k++) {
        const object_t *o = &s->lvl.objects[s->cand[k]];

        if (OBJ_CATEGORY[o->type] != CAT_NEUTRAL)
            continue;
        if (!rect_overlap(o->hitbox.aabb, zone))
            continue;
        touched = zone_dark(p, &o->hitbox, y_line)
            || zone_light(&o->hitbox, zone);
    }
    p->can_jump = touched && p->vy <= p->surface_rise + RISE_EPSILON;
}
