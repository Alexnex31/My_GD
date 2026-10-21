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
    const mode_ops_t *m = player_mode(p);
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

/*
** The objects this tick can touch, using the level's x order (4.1). The
** cursor only moves forward, so it keeps everything the biggest player could
** reach from here: a size or mode portal can grow this one into objects
** behind it, and the jump zone reaches a skin further than the square. That
** biggest player is PLAYER_HALF, which test_constants checks against every
** mode at both sizes. It stops at the first object still ahead of that: a
** long one keeps the cursor only while it lasts.
*/
static void broadphase(sim_t *s, rect_t sweep)
{
    const level_data_t *lv = &s->lvl;
    double behind = s->st.player.pos.x - PLAYER_HALF - ZONE_SKIN;
    const rect_t *box = NULL;

    while (s->st.first_active < lv->nb_objects) {
        box = &lv->objects[s->st.first_active].hitbox.aabb;
        if (box->x + box->w > behind)
            break;
        s->st.first_active += 1;
    }
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

/* A step's lift or the spider's press: no horizontal travel at all. */
static void advance_y(sim_t *s, vec2_t d, double t)
{
    s->st.player.pos.y += d.y * t;
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
    double half = player_mode(p)->half;
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

/* The one list every surface rule reads: nothing else names them (G.5). */
int current_surfaces(const sim_t *s, surface_t out[3])
{
    const ship_bounds_t *b = &s->st.bounds;

    out[0] = (surface_t){GROUND_Y, 1.0, SURF_GROUND};
    if (!b->active)
        return 1;
    out[1] = (surface_t){b->bottom, 1.0, SURF_FLOOR};
    out[2] = (surface_t){b->top, -1.0, SURF_CEIL};
    return 3;
}

static void try_surfaces(sim_t *s, vec2_t d, event_t *out)
{
    const player_t *p = &s->st.player;
    double half = player_mode(p)->half;
    surface_t surf[3];
    int n = current_surfaces(s, surf);
    contact_t c;

    for (int k = 0; k < n; k++) {
        if (!sweep_box_plane(p->pos, half, d, surf[k].y, surf[k].side, &c))
            continue;
        c.surface = surf[k].id;
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
    const mode_ops_t *m = player_mode(p);
    double death = INFINITY;
    double t;

    for (size_t k = 0; k < s->nb_cand; k++) {
        const object_t *o = &s->lvl.objects[s->cand[k]];

        if (OBJ_CATEGORY[o->type] == CAT_NEUTRAL)
            t = sweep_box_touch(p->pos, mode_neutral_kill_half(m), d,
                &o->hitbox);
        else if (OBJ_CATEGORY[o->type] != CAT_HARM)
            continue;
        else if (o->hitbox.kind == SHAPE_CIRCLE)         /* a saw (G.10) */
            t = sweep_box_disc(p->pos, m->half, d, o->hitbox.center,
                o->hitbox.radius);
        else
            t = sweep_box_touch(p->pos, m->half, d, &o->hitbox);
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
    double half = player_mode(p)->half;
    contact_t c;

    if (!sweep_circle_clear(p->pos, half, d, h, circle_face_ok,
        circle_vertex_ok, p->gravity_dir, &c))
        return;
    c.surface = (long)i;
    keep(out, &c);
}

/*
** The circle already inside a shape, which no sweep sees: a player that grew
** (10.4) or changed mode, or a tick's last leg (MAX_CONTACTS), can put it
** there. The face it is least deep in is its way out. When that face is one
** of the circle's, a slope or a ceiling, a move going deeper into it meets it
** now: the player is put back out of it (push_out) and the contact acts as
** any other. A wall is still the inner box's, and so is a flat top: the
** step-up lifts the player onto it (G.7).
*/
static void try_circle_inside(sim_t *s, size_t i, vec2_t d, event_t *out)
{
    const player_t *p = &s->st.player;
    const hitbox_t *h = &s->lvl.objects[i].hitbox;
    int f = 0;
    contact_t c;

    for (int k = 1; k < h->nverts; k++)
        if (dot(h->face_n[k], p->pos) - h->face_off[k]
            > dot(h->face_n[f], p->pos) - h->face_off[f])
            f = k;
    if (!circle_face_ok(h, f, p->gravity_dir)
        || fabs(h->face_n[f].y) < FLOOR_MIN_DOT)
        return;                              /* a wall: the inner box decides */
    if (dot(h->face_n[f], d) >= -CONTACT_SKIN)
        return;                              /* along it, or out of it */
    c = (contact_t){0.0, h->face_n[f], false, h->face_off[f], (long)i, true};
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
    double half = player_mode(p)->half;
    bool in_square;

    if (is_passed(s, i))
        return;
    in_square = overlap_box_poly(p->pos, half, h);
    if (!in_square)
        try_square(s, i, d, out);            /* inside it: the inner box decides */
    if (in_square && overlap_circle_poly(p->pos, half, h)) {
        try_circle_inside(s, i, d, out);
        return;
    }
    try_circle(s, i, d, out);
}

/*
** A surface solid on the side the lift goes to: the corridor's ceiling, or
** the ground and the corridor's floor once gravity is flipped (G.7).
*/
static bool lift_hits_surface(const sim_t *s, double y, double lift)
{
    const player_t *p = &s->st.player;
    double g = p->gravity_dir;
    double top = (y - lift * g) - player_mode(p)->half * g;
    surface_t surf[3];
    int n = current_surfaces(s, surf);

    for (int k = 0; k < n; k++)
        if (surf[k].side == -g && (top - surf[k].y) * g < 0.0)
            return true;
    return false;
}

/*
** When the inner box's leading side reaches a horizontal face that sits
** between it and the feet, and the player could jump then, it is a step (G.7).
*/
static bool step_event(const sim_t *s, vec2_t d, vec2_t vel, face_t f,
    double *t_out)
{
    const player_t *p = &s->st.player;
    const mode_ops_t *m = player_mode(p);
    double g = p->gravity_dir;
    double lead = p->pos.x + m->inner_half;
    double t;
    double y;
    double feet;

    if (d.x <= 0.0)
        return false;                        /* only a move to the right steps */
    t = lead >= f.x0 ? 0.0 : (f.x0 - lead) / d.x;
    if (t > 1.0)
        return false;
    y = p->pos.y + d.y * t;
    feet = y + m->half * g;
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
    double half = player_mode(p)->half;
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

typedef void (*advance_fn)(sim_t *s, vec2_t d, double t);

/*
** How every leg ends, in the order of 4.4: a death no later than the leg's
** event stops everything there; otherwise what the leg went through is
** touched and the player moves to the event. False when it died.
*/
static bool finish_leg(sim_t *s, vec2_t d, double death, double t_end,
    advance_fn move)
{
    if (death <= t_end) {
        move(s, d, death);
        log_debug(s, DBG_DEATH, (vec2_t){0.0, 0.0});
        s->st.player.alive = false;
        return false;
    }
    leg_touches(s, d, t_end);
    move(s, d, t_end);
    return true;
}

/* The lift is a leg of its own: checked like any other, then it lands (4.4). */
static void step_up(sim_t *s, const face_t *f, vec2_t *vel)
{
    player_t *p = &s->st.player;
    const mode_ops_t *m = player_mode(p);
    double g = p->gravity_dir;
    double target = f->y - m->half * g;
    vec2_t lift = {0.0, target - p->pos.y};
    double death = fmin(leg_death_time(s, lift), circle_lift_death(s, lift));

    if (!finish_leg(s, lift, death, 1.0, advance_y))
        return;
    log_debug(s, DBG_STEP, (vec2_t){0.0, 0.0});
    p->pos.y = target;                       /* exact, like settle on a face */
    p->grounded = true;
    p->surface_rise = 0.0;
    p->support_normal = (vec2_t){0.0, -g};
    p->vy = 0.0;
    vel->y = 0.0;
}

static void first_event(sim_t *s, vec2_t d, vec2_t vel, bool steps,
    event_t *out)
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
    if (steps)
        try_steps(s, d, vel, out);           /* contacts first, then steps (G.9) */
}

/* Where a surface would be crossed (4.3): past its rest position. */
static bool past_a_surface(const sim_t *s, double y)
{
    double half = player_mode(&s->st.player)->half;
    surface_t surf[3];
    int n = current_surfaces(s, surf);

    for (int k = 0; k < n; k++)
        if ((y - (surf[k].y - surf[k].side * half)) * surf[k].side > 0.0)
            return true;
    return false;
}

/*
** The circle, a skin off the face it met. Never across a surface (4.3): a
** player held between the two, which push_out couldn't move, stays put.
*/
static void skin_off(sim_t *s, const contact_t *c)
{
    double y = s->st.player.pos.y + c->normal.y * CONTACT_SKIN;

    if (!past_a_surface(s, y))
        s->st.player.pos.y = y;
}

static void settle(sim_t *s, const contact_t *c, double half)
{
    player_t *p = &s->st.player;

    if (c->flat) {                           /* exactly on the face's own line */
        p->pos.y = (c->offset + half) * c->normal.y;
        return;
    }
    skin_off(s, c);                          /* the circle, a skin off a slope */
}

/* Supported: the player slides along the surface at its own rise speed. */
static void land(sim_t *s, const contact_t *c, vec2_t *vel)
{
    player_t *p = &s->st.player;
    double rise = vel->x * (c->normal.x / c->normal.y) * p->gravity_dir;

    log_debug(s, DBG_LAND, c->normal);
    settle(s, c, player_mode(p)->half);
    p->grounded = true;
    p->support_normal = c->normal;
    p->surface_rise = rise;
    vel->y = rise;
    if (!player_mode(p)->keep_vy_on_surface)
        p->vy = rise;
}

/* A ceiling. Surfaces never kill: a fatal head hit becomes a stop (4.3). */
static void head_hit(sim_t *s, const contact_t *c, vec2_t *vel)
{
    player_t *p = &s->st.player;
    double e = player_mode(p)->head_restitution;
    double follow = vel->x * (c->normal.x / c->normal.y) * p->gravity_dir;

    log_debug(s, e < 0.0 && !is_surface(c->surface) ? DBG_DEATH : DBG_HEAD,
        c->normal);
    if (e < 0.0 && !is_surface(c->surface)) {
        p->alive = false;
        return;
    }
    e = e < 0.0 ? 0.0 : e;
    p->boost_ticks = 0;                      /* a robot's rise ends on what stops it */
    skin_off(s, c);
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

/*
** A slope or a ceiling the circle is already in (try_circle_inside): the
** player goes straight out of it, up or down, until the circle touches it. It
** is a leg checked like a step's lift (G.7), and then the contact acts as any
** other: it lands, or hits its head. Where that would take it past a surface,
** it stays where it is (4.3), and the contact still acts. False when it died.
*/
static bool push_out(sim_t *s, const contact_t *c)
{
    player_t *p = &s->st.player;
    double half = player_mode(p)->half;
    double y = (c->offset + half - c->normal.x * p->pos.x) / c->normal.y;
    vec2_t move = {0.0, y - p->pos.y};
    double death;

    if (past_a_surface(s, y))
        return true;
    death = fmin(leg_death_time(s, move), circle_lift_death(s, move));
    if (!finish_leg(s, move, death, 1.0, advance_y))
        return false;
    p->pos.y = y;                            /* exact, like a step's landing */
    return true;
}

static void respond(sim_t *s, const contact_t *c, vec2_t *vel)
{
    const player_t *p = &s->st.player;
    double up_dot = -c->normal.y * p->gravity_dir;

    if (c->inside && !push_out(s, c))
        return;
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
    double y = s->st.player.pos.y;
    surface_t surf[3];
    int n = current_surfaces(s, surf);

    for (int k = 0; k < n; k++)
        if ((y - surf[k].y) * surf[k].side > 0.0)
            return true;
    return false;
}

/*
** Where a straight move along the gravity ends: the nearest surface solid on
** that side, or the kill line when there is none (4.7).
*/
static double far_side(const sim_t *s)
{
    double g = s->st.player.gravity_dir;
    double far = s->lvl.kill_y;
    bool found = false;
    surface_t surf[3];
    int n = current_surfaces(s, surf);

    for (int k = 0; k < n; k++) {
        if (surf[k].side != g || (found && (surf[k].y - far) * g >= 0.0))
            continue;
        far = surf[k].y;
        found = true;
    }
    return far;
}

/*
** The spider's press (10.5): straight to its new floor, before the tick's own
** move. What is on the way acts as in any move: a spike kills, a portal or a
** pad is touched, a block or a slope stops it, a steep face is gone through.
** With nothing above, it is carried to the kill line and dies there (4.7).
** A step is not on the way: nothing moves sideways.
** It arrives as a grounded player does: at its floor's slide, with one tick
** of gravity past it.
** The contact budget doesn't apply: the press is one straight move, and only
** steep faces let it go on, each one at most once since it is then passed.
** The bound is for a full list of passed objects, which forgets the rest.
*/
static void teleport(sim_t *s)
{
    player_t *p = &s->st.player;
    double half = player_mode(p)->half;
    double far = far_side(s);
    vec2_t d = {0.0, far - p->pos.y};
    vec2_t vel = {p->vx, 0.0};               /* a slope it lands on: its slide */
    rect_t r = {p->pos.x - half - CONTACT_SKIN,
        fmin(p->pos.y, far) - half - CONTACT_SKIN,
        2.0 * (half + CONTACT_SKIN), fabs(d.y) + 2.0 * (half + CONTACT_SKIN)};

    p->teleport = false;
    broadphase(s, r);
    for (int n = 0; n <= MAX_CANDIDATES && !p->grounded; n++) {
        event_t e;

        first_event(s, d, vel, false, &e);
        if (!finish_leg(s, d, leg_death_time(s, d), e.t, advance_y))
            return;
        d.y *= 1.0 - e.t;
        if (e.kind != EV_CONTACT)
            break;
        respond(s, &e.contact, &vel);
    }
    p->vy = (p->grounded ? p->surface_rise : 0.0) - player_mode(p)->gravity;
}

void move_and_collide(sim_t *s)
{
    player_t *p = &s->st.player;
    vec2_t vel;

    s->nb_dbg = 0;                           /* the overlay's record (9.6) */
    s->legs = 0;
    s->nb_touch = 0;
    s->nb_passed = 0;
    if (p->teleport) {
        teleport(s);
        p->prev_pos.y = p->pos.y;            /* drawn there at once, not gliding */
    }
    if (!p->alive)
        return;
    vel = (vec2_t){p->vx, p->vy};
    broadphase(s, tick_sweep_bounds(p));
    s->tick_x0 = s->st.distance;
    s->tick_left = 1.0;
    p->grounded = false;                     /* surface_rise keeps its last value */
    for (int n = 0; p->alive && s->tick_left > 0.0; n++) {
        bool last = n == MAX_CONTACTS;       /* the contact budget ran out */
        vec2_t d = {vel.x * s->tick_left,
            last ? 0.0 : -vel.y * p->gravity_dir * s->tick_left};
        event_t e = {.kind = EV_NONE, .t = 1.0};

        if (!last)
            first_event(s, d, vel, true, &e);
        if (!finish_leg(s, d, leg_death_time(s, d), e.t, advance))
            return;                          /* death stops everything (4.4) */
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
    if (p->gravity_dir < 0 && p->pos.y - player_mode(p)->half < lvl->kill_y)
        p->alive = false;
}
