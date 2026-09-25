/*
** ALEXNEX PROJECT, 2026
** sim/move.c
** File description:
** one tick of movement: legs, contacts, responses and deaths (4.4, G.8, G.9)
*/

#include <math.h>

#include "sim/geom.h"
#include "sim/hitbox.h"
#include "sim/internal.h"
#include "sim/modes.h"
#include "sim/sweep.h"

/* Everything the tick's movement can reach, step-up room included (G.8). */
static rect_t tick_sweep_bounds(const player_t *p)
{
    const mode_ops_t *m = &MODES[p->mode];
    double ys = p->pos.y;
    double ye = p->pos.y - p->vy * p->gravity_dir;
    double room = m->half - m->inner_half + CONTACT_SKIN;
    rect_t r;

    r.x = p->pos.x - m->half - CONTACT_SKIN;
    r.w = p->vx + 2.0 * (m->half + CONTACT_SKIN);
    r.y = fmin(ys, ye) - m->half - room;
    r.h = fabs(ye - ys) + 2.0 * (m->half + room);
    return r;
}

/* The objects this tick can touch, using the level's x order (4.1). */
static void broadphase(sim_t *s, rect_t sweep)
{
    const level_data_t *lv = &s->lvl;

    while (s->st.first_active < lv->nb_objects
        && lv->objects[s->st.first_active].hitbox.aabb.x + lv->reach <= sweep.x)
        s->st.first_active += 1;
    s->nb_cand = 0;
    for (size_t i = s->st.first_active; i < lv->nb_objects; i++) {
        const hitbox_t *h = &lv->objects[i].hitbox;

        if (h->aabb.x >= sweep.x + sweep.w)
            break;                           /* sorted: nothing further can touch */
        if (rect_overlap(h->aabb, sweep) && s->nb_cand < MAX_CANDIDATES) {
            s->cand[s->nb_cand] = i;
            s->nb_cand += 1;
        }
    }
}

/*
** The only place the player moves. Horizontal distance is never accumulated
** leg by leg: it is the tick's start plus the fraction of the tick travelled
** so far, so a tick split into legs still covers exactly vx (3.2).
*/
static void advance(sim_t *s, vec2_t d, double t)
{
    player_t *p = &s->st.player;

    s->tick_left *= 1.0 - t;
    s->st.distance = s->tick_x0 + p->vx * (1.0 - s->tick_left);
    p->pos.x = PLAYER_SPAWN_X + s->st.distance;
    p->pos.y += d.y * t;
    s->legs += 1;
}

static bool is_passed(const sim_t *s, size_t i)
{
    for (size_t k = 0; k < s->nb_passed; k++)
        if (s->passed[k] == i)
            return true;
    return false;
}

/* Strictly earlier wins, so a tie keeps the candidate tested first (G.9). */
static void keep(event_t *out, const contact_t *c)
{
    if (out->kind != EV_NONE && c->t >= out->t)
        return;
    out->kind = EV_CONTACT;
    out->t = c->t;
    out->contact = *c;
}

/* A contact through the x axis: its normal is exactly horizontal (G.9). */
static bool through_x_axis(const contact_t *c)
{
    return c->normal.y == 0.0;
}

static void try_square(sim_t *s, size_t i, vec2_t d, event_t *out)
{
    const player_t *p = &s->st.player;
    const hitbox_t *h = &s->lvl.objects[i].hitbox;
    double half = MODES[p->mode].half;
    contact_t c;

    if (is_passed(s, i) || overlap_box_poly(p->pos, half, h))
        return;                              /* inside it: the inner box decides */
    if (!sweep_box_poly(p->pos, half, d, h, &c))
        return;
    if (c.flat && c.normal.y * p->gravity_dir > 0.0)
        return;                              /* met from below: the circle's job */
    if (!c.flat && !through_x_axis(&c))
        return;                              /* a tilted face: the circle's job */
    c.surface = (long)i;
    keep(out, &c);
}

/* The ground, and the corridor's boundaries while one is active (G.5). */
static void try_surfaces(sim_t *s, vec2_t d, event_t *out)
{
    const player_t *p = &s->st.player;
    double half = MODES[p->mode].half;
    contact_t c;

    if (sweep_box_plane(p->pos, half, d, GROUND_Y, 1.0, &c)) {
        c.surface = SURF_GROUND;
        keep(out, &c);
    }
    if (!s->st.bounds.active)
        return;
    if (sweep_box_plane(p->pos, half, d, s->st.bounds.bottom, 1.0, &c)) {
        c.surface = SURF_FLOOR;
        keep(out, &c);
    }
    if (sweep_box_plane(p->pos, half, d, s->st.bounds.top, -1.0, &c)) {
        c.surface = SURF_CEIL;
        keep(out, &c);
    }
}

static bool first_event(sim_t *s, vec2_t d, event_t *out)
{
    out->kind = EV_NONE;
    out->t = 1.0;
    for (size_t k = 0; k < s->nb_cand; k++) {
        size_t i = s->cand[k];

        if (OBJ_CATEGORY[s->lvl.objects[i].type] != CAT_NEUTRAL)
            continue;
        try_square(s, i, d, out);            /* the circle and the steps: step 5 */
    }
    try_surfaces(s, d, out);
    return out->kind != EV_NONE;
}

static void settle(player_t *p, const contact_t *c, double half)
{
    if (c->flat) {                           /* exactly on the face's own line */
        p->pos.y = (c->offset + half) * c->normal.y;
        return;
    }
    p->pos.y += c->normal.y * CONTACT_SKIN;  /* the circle, a skin off a slope */
}

/* Supported: the player slides along the surface at its own rise speed. */
static void land(player_t *p, const contact_t *c, vec2_t *vel)
{
    double rise = vel->x * (c->normal.x / c->normal.y) * p->gravity_dir;

    settle(p, c, MODES[p->mode].half);
    p->grounded = true;
    p->support_normal = c->normal;
    p->surface_rise = rise;
    vel->y = rise;
    if (!MODES[p->mode].keep_vy_on_surface)
        p->vy = rise;
}

/* A ceiling. Surfaces never kill: a fatal head hit becomes a stop (4.3). */
static void head_hit(player_t *p, const contact_t *c, vec2_t *vel)
{
    double e = MODES[p->mode].head_restitution;
    double follow = vel->x * (c->normal.x / c->normal.y) * p->gravity_dir;

    if (e < 0.0 && !is_surface(c->surface)) {
        p->alive = false;
        return;
    }
    e = e < 0.0 ? 0.0 : e;
    p->pos.y += c->normal.y * CONTACT_SKIN;
    p->vy = p->vy > PER_TICK(BOUNCE_MIN_SPEED) ? -p->vy * e : 0.0;
    if (p->vy > follow)
        p->vy = follow;                      /* follow a ceiling that comes down */
    vel->y = p->vy;
}

/* Steeper than 50 degrees: the player goes into it and keeps its speed. */
static void pass_into(sim_t *s, const contact_t *c)
{
    if (!is_surface(c->surface) && s->nb_passed < MAX_CANDIDATES) {
        s->passed[s->nb_passed] = (size_t)c->surface;
        s->nb_passed += 1;
    }
}

static void respond(sim_t *s, const contact_t *c, vec2_t *vel)
{
    player_t *p = &s->st.player;
    double up_dot = -c->normal.y * p->gravity_dir;

    if (up_dot >= FLOOR_MIN_DOT)
        land(p, c, vel);
    else if (up_dot <= -FLOOR_MIN_DOT)
        head_hit(p, c, vel);
    else
        pass_into(s, c);
}

/*
** The inner box against neutral objects, and the rigid square against harm,
** over [0, t_end] of this leg. The earliest touch kills, where it happened.
** Surfaces are not tested: they never kill (4.3).
*/
static bool leg_deaths(sim_t *s, vec2_t d, double t_end)
{
    player_t *p = &s->st.player;
    const mode_ops_t *m = &MODES[p->mode];
    double death = INFINITY;
    double t;

    for (size_t k = 0; k < s->nb_cand; k++) {
        const object_t *o = &s->lvl.objects[s->cand[k]];

        if (OBJ_CATEGORY[o->type] == CAT_NEUTRAL)
            t = sweep_box_touch(p->pos, m->inner_half, d, &o->hitbox);
        else if (OBJ_CATEGORY[o->type] == CAT_HARM)
            t = sweep_box_touch(p->pos, m->half, d, &o->hitbox);
        else
            continue;
        death = fmin(death, t);
    }
    if (death > t_end)
        return false;
    advance(s, d, death);
    p->alive = false;
    return true;
}

static void leg_touches(sim_t *s, vec2_t d, double t_end)
{
    (void)s;                                 /* interactive objects: step 6 (4.6) */
    (void)d;
    (void)t_end;
}

/* A surface can't be crossed (4.3): only a bug could put the player past one. */
static bool crossed_surface(const sim_t *s)
{
    const player_t *p = &s->st.player;

    if (p->pos.y > GROUND_Y)
        return true;
    return s->st.bounds.active
        && (p->pos.y > s->st.bounds.bottom || p->pos.y < s->st.bounds.top);
}

void move_and_collide(sim_t *s)
{
    player_t *p = &s->st.player;
    vec2_t vel = {p->vx, p->vy};

    broadphase(s, tick_sweep_bounds(p));
    s->legs = 0;
    s->nb_touch = 0;
    s->nb_passed = 0;
    s->tick_x0 = s->st.distance;
    s->tick_left = 1.0;
    p->grounded = false;                     /* surface_rise keeps its last value */
    for (int n = 0; p->alive && s->tick_left > 0.0; n++) {
        bool last = n == MAX_CONTACTS;       /* the contact budget ran out */
        vec2_t d = {vel.x * s->tick_left,
            last ? 0.0 : -vel.y * p->gravity_dir * s->tick_left};
        event_t e = {.kind = EV_NONE, .t = 1.0};

        if (!last)
            first_event(s, d, &e);
        if (leg_deaths(s, d, e.t))
            return;                          /* death stops everything (4.4) */
        leg_touches(s, d, e.t);
        advance(s, d, e.t);
        if (e.kind == EV_CONTACT)
            respond(s, &e.contact, &vel);
    }
    if (p->alive && crossed_surface(s))
        p->alive = false;
    if (p->alive && !p->grounded)
        p->surface_rise = 0.0;
}

/* Only flipped gravity has a ceiling to fall into (4.7). */
void collide_kill_ceiling(player_t *p, const level_data_t *lvl)
{
    if (p->gravity_dir < 0 && p->pos.y - MODES[p->mode].half < lvl->kill_y)
        p->alive = false;
}
