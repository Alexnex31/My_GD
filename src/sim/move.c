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
    p->pos.x = s->lvl.hdr.start.pos.x + s->st.distance;
    p->pos.y += d.y * t;
    s->legs += 1;
}

/* A step's lift: straight against gravity, so it costs no horizontal travel. */
static void advance_y(sim_t *s, double dy)
{
    s->st.player.pos.y += dy;
    s->legs += 1;
}

/* The overlay's record of what this tick did (9.6). Physics never reads it. */
static void log_debug(sim_t *s, debug_kind_t kind, vec2_t normal)
{
    if (s->nb_dbg >= MAX_DEBUG_EVENTS)
        return;
    s->dbg[s->nb_dbg].pos = s->st.player.pos;
    s->dbg[s->nb_dbg].normal = normal;
    s->dbg[s->nb_dbg].kind = kind;
    s->nb_dbg += 1;
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

/* The caller has checked is_passed and that the square is clear of the shape. */
static void try_square(sim_t *s, size_t i, vec2_t d, event_t *out)
{
    const player_t *p = &s->st.player;
    const hitbox_t *h = &s->lvl.objects[i].hitbox;
    double half = MODES[p->mode].half;
    contact_t c;

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

/*
** When this leg kills: the inner box against neutral objects (the whole
** square for a mode they always kill), or the rigid square against harm,
** whichever comes first. INFINITY when neither does.
** Surfaces are not tested: they never kill (4.3).
*/
static double leg_death_time(sim_t *s, vec2_t d)
{
    const player_t *p = &s->st.player;
    const mode_ops_t *m = &MODES[p->mode];
    double death = INFINITY;
    double t;

    for (size_t k = 0; k < s->nb_cand; k++) {
        const object_t *o = &s->lvl.objects[s->cand[k]];

        if (OBJ_CATEGORY[o->type] == CAT_NEUTRAL)
            t = sweep_box_touch(p->pos, mode_neutral_kill_half(m), d,
                &o->hitbox);
        else if (OBJ_CATEGORY[o->type] == CAT_HARM)
            t = sweep_box_touch(p->pos, m->half, d, &o->hitbox);
        else
            continue;
        death = fmin(death, t);
    }
    return death;
}

/* The circle owns tilted faces, and every face or corner facing a ceiling. */
static bool circle_face_ok(const hitbox_t *h, int i, double g)
{
    return h->face_kind[i] == FACE_TILTED || h->face_n[i].y * g > 0.0;
}

static bool circle_vertex_ok(const hitbox_t *h, int i, double g)
{
    int prev = (i + h->nverts - 1) % h->nverts;

    if (h->face_kind[prev] == FACE_TILTED && h->face_kind[i] == FACE_TILTED)
        return true;                         /* a corner between two slopes */
    return h->face_n[prev].y * g > 0.0 || h->face_n[i].y * g > 0.0;
}

/* Same preconditions, for the circle: the caller knows it is clear of it. */
static void try_circle(sim_t *s, size_t i, vec2_t d, event_t *out)
{
    const player_t *p = &s->st.player;
    const hitbox_t *h = &s->lvl.objects[i].hitbox;
    double half = MODES[p->mode].half;
    contact_t c;

    if (!sweep_circle_clear(p->pos, half, d, h, circle_face_ok,
        circle_vertex_ok, p->gravity_dir, &c))
        return;
    c.surface = (long)i;
    keep(out, &c);
}

/*
** Both shapes against one object. The circle is inscribed in the square, so
** a square that doesn't overlap the shape proves the circle doesn't either:
** the circle's own distance test is needed only in the rare case where the
** square is already inside (4.3).
*/
static void try_shapes(sim_t *s, size_t i, vec2_t d, event_t *out)
{
    const player_t *p = &s->st.player;
    const hitbox_t *h = &s->lvl.objects[i].hitbox;
    double half = MODES[p->mode].half;
    bool in_square;

    if (is_passed(s, i))
        return;
    in_square = overlap_box_poly(p->pos, half, h);
    if (!in_square)
        try_square(s, i, d, out);            /* inside it: the inner box decides */
    if (in_square && overlap_circle_poly(p->pos, half, h))
        return;
    try_circle(s, i, d, out);
}

/* The corridor's ceiling, or the ground once gravity is flipped (G.7). */
static bool lift_hits_surface(const sim_t *s, double y, double lift)
{
    const player_t *p = &s->st.player;
    double g = p->gravity_dir;
    double top = (y - lift * g) - MODES[p->mode].half * g;
    double ceiling;

    if (s->st.bounds.active)
        ceiling = g > 0.0 ? s->st.bounds.top : s->st.bounds.bottom;
    else if (g < 0.0)
        ceiling = GROUND_Y;
    else
        return false;                        /* nothing above an open level */
    return (top - ceiling) * g < 0.0;
}

/*
** When the inner box's leading side reaches a horizontal face that sits
** between it and the feet, and the player could jump then, it is a step (G.7).
*/
static bool step_event(const sim_t *s, vec2_t d, vec2_t vel, face_t f,
    double *t_out)
{
    const player_t *p = &s->st.player;
    const mode_ops_t *m = &MODES[p->mode];
    double g = p->gravity_dir;
    double lead = p->pos.x + m->inner_half;
    double t = lead >= f.x0 ? 0.0 : (f.x0 - lead) / d.x;
    double y = p->pos.y + d.y * t;
    double feet = y + m->half * g;

    if (d.x <= 0.0 || t > 1.0)
        return false;
    if (vel.y > p->surface_rise + RISE_EPSILON)
        return false;                        /* rising: it keeps its motion */
    if (p->pos.x + d.x * t - m->half >= f.x1)
        return false;                        /* the square is past the face */
    if ((f.y - (y + m->inner_half * g)) * g < 0.0 || (feet - f.y) * g <= 0.0)
        return false;                        /* the inner box decides, or too low */
    if (lift_hits_surface(s, y, (feet - f.y) * g))
        return false;
    *t_out = t;
    return true;
}

static void try_steps(sim_t *s, vec2_t d, vec2_t vel, event_t *out)
{
    double g = s->st.player.gravity_dir;
    face_t f;
    double t;

    for (size_t k = 0; k < s->nb_cand; k++) {
        const object_t *o = &s->lvl.objects[s->cand[k]];

        if (OBJ_CATEGORY[o->type] != CAT_NEUTRAL)
            continue;
        for (int i = 0; i < o->hitbox.nverts; i++) {
            if (!up_facing_horizontal_face(&o->hitbox, i, g, &f))
                continue;
            if (!step_event(s, d, vel, f, &t) || t > out->t)
                continue;
            /* a contact at the same time wins; among steps, the highest one */
            if (t == out->t && (out->kind != EV_STEP
                || (f.y - out->step.y) * g >= 0.0))
                continue;
            out->kind = EV_STEP;
            out->t = t;
            out->step = f;
        }
    }
}

/*
** The circle, along a lift: anything it runs into on the way up kills, in
** every mode (4.4). Shapes it already overlaps are skipped: climbing a block
** means the circle is inside that block's corner before the lift even starts.
*/
static double circle_lift_death(sim_t *s, vec2_t lift)
{
    const player_t *p = &s->st.player;
    double half = MODES[p->mode].half;
    double death = INFINITY;

    for (size_t k = 0; k < s->nb_cand; k++) {
        const object_t *o = &s->lvl.objects[s->cand[k]];

        if (OBJ_CATEGORY[o->type] != CAT_NEUTRAL
            || overlap_circle_poly(p->pos, half, &o->hitbox))
            continue;
        death = fmin(death,
            sweep_circle_touch_clear(p->pos, half, lift, &o->hitbox));
    }
    return death;
}

/* The lift is a leg of its own: checked like any other, then it lands (4.4). */
static void step_up(sim_t *s, const face_t *f, vec2_t *vel)
{
    player_t *p = &s->st.player;
    const mode_ops_t *m = &MODES[p->mode];
    double g = p->gravity_dir;
    double target = f->y - m->half * g;
    vec2_t lift = {0.0, target - p->pos.y};
    double death = fmin(leg_death_time(s, lift), circle_lift_death(s, lift));

    if (death <= 1.0) {
        advance_y(s, lift.y * death);
        log_debug(s, DBG_DEATH, (vec2_t){0.0, 0.0});
        p->alive = false;
        return;
    }
    leg_touches(s, lift, 1.0);
    advance_y(s, lift.y);
    log_debug(s, DBG_STEP, (vec2_t){0.0, 0.0});
    p->pos.y = target;                       /* exact, like settle on a face */
    p->grounded = true;
    p->surface_rise = 0.0;
    p->support_normal = (vec2_t){0.0, -g};
    p->vy = 0.0;
    vel->y = 0.0;
}

static bool first_event(sim_t *s, vec2_t d, vec2_t vel, event_t *out)
{
    out->kind = EV_NONE;
    out->t = 1.0;
    for (size_t k = 0; k < s->nb_cand; k++) {
        size_t i = s->cand[k];

        if (OBJ_CATEGORY[s->lvl.objects[i].type] != CAT_NEUTRAL)
            continue;
        try_shapes(s, i, d, out);
    }
    try_surfaces(s, d, out);
    try_steps(s, d, vel, out);               /* contacts first, then steps (G.9) */
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
static void land(sim_t *s, const contact_t *c, vec2_t *vel)
{
    player_t *p = &s->st.player;
    double rise = vel->x * (c->normal.x / c->normal.y) * p->gravity_dir;

    log_debug(s, DBG_LAND, c->normal);
    settle(p, c, MODES[p->mode].half);
    p->grounded = true;
    p->support_normal = c->normal;
    p->surface_rise = rise;
    vel->y = rise;
    if (!MODES[p->mode].keep_vy_on_surface)
        p->vy = rise;
}

/* A ceiling. Surfaces never kill: a fatal head hit becomes a stop (4.3). */
static void head_hit(sim_t *s, const contact_t *c, vec2_t *vel)
{
    player_t *p = &s->st.player;
    double e = MODES[p->mode].head_restitution;
    double follow = vel->x * (c->normal.x / c->normal.y) * p->gravity_dir;

    log_debug(s, e < 0.0 && !is_surface(c->surface) ? DBG_DEATH : DBG_HEAD,
        c->normal);
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
    log_debug(s, DBG_PASS, c->normal);
    if (!is_surface(c->surface) && s->nb_passed < MAX_CANDIDATES) {
        s->passed[s->nb_passed] = (size_t)c->surface;
        s->nb_passed += 1;
    }
}

static void respond(sim_t *s, const contact_t *c, vec2_t *vel)
{
    const player_t *p = &s->st.player;
    double up_dot = -c->normal.y * p->gravity_dir;

    if (up_dot >= FLOOR_MIN_DOT)
        land(s, c, vel);
    else if (up_dot <= -FLOOR_MIN_DOT)
        head_hit(s, c, vel);
    else
        pass_into(s, c);
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
    s->nb_dbg = 0;                           /* the overlay's record (9.6) */
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

        double death;

        if (!last)
            first_event(s, d, vel, &e);
        death = leg_death_time(s, d);
        if (death <= e.t) {
            advance(s, d, death);            /* death stops everything (4.4) */
            log_debug(s, DBG_DEATH, (vec2_t){0.0, 0.0});
            p->alive = false;
            return;
        }
        leg_touches(s, d, e.t);
        advance(s, d, e.t);
        if (e.kind == EV_CONTACT)
            respond(s, &e.contact, &vel);
        else if (e.kind == EV_STEP)
            step_up(s, &e.step, &vel);
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
