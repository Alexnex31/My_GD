/*
** ALEXNEX PROJECT, 2026
** editor/ed_props.c
** File description:
** an object's properties: turn, size, kind; a start position's (FEATURES 11.4, 11.8)
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "editor/ed_props.h"
#include "sim/constants.h"
#include "sim/modes.h"

    #define TURN_STEP 15.0

static const double SPEEDS[] = {0.5, 1.0, 2.0, 3.0, 4.0};
    #define NB_SPEEDS ((int)(sizeof(SPEEDS) / sizeof(SPEEDS[0])))

const char *ed_prop_name(ed_prop_t p)
{
    static const char *const NAMES[ED_PROP_COUNT] = {"Turn", "Width",
        "Height", "Kind", "Gravity", "Speed"};

    return NAMES[p];
}

static bool has_kind(const ed_object_t *o)
{
    return o->is_start || o->obj.type == OBJ_PORTAL
        || o->obj.type == OBJ_GRAVITY || o->obj.type == OBJ_PAD
        || o->obj.type == OBJ_ORB;
}

static bool has(const ed_object_t *o, ed_prop_t p)
{
    if (p == ED_PROP_KIND)
        return has_kind(o);
    if (p == ED_PROP_GRAVITY || p == ED_PROP_SPEED)
        return o->is_start;
    return !o->is_start;
}

static const char *kind_text(const ed_object_t *o)
{
    if (o->is_start)
        return MODES[o->start.mode].name;
    if (o->obj.type == OBJ_PORTAL)
        return MODES[o->obj.portal_mode].name;
    if (o->obj.type == OBJ_GRAVITY)
        return o->obj.portal_gravity < 0 ? "up" : "down";
    return LAUNCHES[o->obj.launch].name;
}

static void text_of(const ed_object_t *o, ed_prop_t p, char *out, size_t size)
{
    if (p == ED_PROP_TURN)
        snprintf(out, size, "%.10g", o->obj.rotation);
    if (p == ED_PROP_W)
        snprintf(out, size, "%.10g", o->obj.rect.w / UNIT);
    if (p == ED_PROP_H)
        snprintf(out, size, "%.10g", o->obj.rect.h / UNIT);
    if (p == ED_PROP_KIND)
        snprintf(out, size, "%s", kind_text(o));
    if (p == ED_PROP_GRAVITY)
        snprintf(out, size, "%s", o->start.gravity_dir < 0 ? "up" : "down");
    if (p == ED_PROP_SPEED)
        snprintf(out, size, "x%.10g", o->start.speed_mult);
}

bool ed_prop_applies(const ed_level_t *lv, ed_prop_t p)
{
    for (size_t i = 0; i < lv->count; i++)
        if (lv->objects[i].selected && has(&lv->objects[i], p))
            return true;
    return false;
}

void ed_prop_text(const ed_level_t *lv, ed_prop_t p, char *out, size_t size)
{
    char one[64];
    bool first = true;

    snprintf(out, size, "%s", "");
    for (size_t i = 0; i < lv->count; i++) {
        if (!lv->objects[i].selected || !has(&lv->objects[i], p))
            continue;
        text_of(&lv->objects[i], p, one, sizeof(one));
        if (!first && strcmp(one, out) != 0) {
            snprintf(out, size, "mixed");
            return;
        }
        snprintf(out, size, "%s", one);
        first = false;
    }
}

static double turned(double deg)
{
    deg = fmod(deg, 360.0);
    return deg < 0.0 ? deg + 360.0 : deg;
}

/* A pad has four colours and an orb six: the next one this type launches. */
static launch_kind_t next_launch(const object_t *o, int dir)
{
    int k = (int)o->launch;

    for (int tries = 0; tries < LAUNCH_KIND_COUNT; tries++) {
        k = (k + dir + LAUNCH_KIND_COUNT) % LAUNCH_KIND_COUNT;
        if ((o->type == OBJ_PAD ? LAUNCHES[k].pad_v : LAUNCHES[k].orb_v)
            != 0.0)
            break;
    }
    return (launch_kind_t)k;
}

static void step_kind(ed_object_t *o, int dir)
{
    if (o->is_start)
        o->start.mode = (gamemode_t)((o->start.mode + dir + MODE_COUNT)
            % MODE_COUNT);
    else if (o->obj.type == OBJ_PORTAL)
        o->obj.portal_mode = (gamemode_t)((o->obj.portal_mode + dir
            + MODE_COUNT) % MODE_COUNT);
    else if (o->obj.type == OBJ_GRAVITY)
        o->obj.portal_gravity = -o->obj.portal_gravity;
    else
        o->obj.launch = next_launch(&o->obj, dir);
}

/* The speeds stop at their ends: nothing is slower than x0.5. */
static void step_speed(ed_object_t *o, int dir)
{
    int at = 1;

    for (int i = 0; i < NB_SPEEDS; i++)
        if (SPEEDS[i] == o->start.speed_mult)
            at = i;
    at += dir;
    at = at < 0 ? 0 : (at >= NB_SPEEDS ? NB_SPEEDS - 1 : at);
    o->start.speed_mult = SPEEDS[at];
}

/* A size never goes under one grid unit; the top left corner stays. */
static void step_one(ed_object_t *o, ed_prop_t p, int dir)
{
    if (p == ED_PROP_TURN)
        o->obj.rotation = turned(o->obj.rotation + dir * TURN_STEP);
    if (p == ED_PROP_W)
        o->obj.rect.w = fmax(UNIT, o->obj.rect.w + dir * UNIT);
    if (p == ED_PROP_H)
        o->obj.rect.h = fmax(UNIT, o->obj.rect.h + dir * UNIT);
    if (p == ED_PROP_KIND)
        step_kind(o, dir);
    if (p == ED_PROP_GRAVITY)
        o->start.gravity_dir = -o->start.gravity_dir;
    if (p == ED_PROP_SPEED)
        step_speed(o, dir);
    if (o->obj.type == OBJ_SAW && !o->is_start && p == ED_PROP_W)
        o->obj.rect.h = o->obj.rect.w;       /* a saw stays round: its hitbox */
    if (o->obj.type == OBJ_SAW && !o->is_start && p == ED_PROP_H)
        o->obj.rect.w = o->obj.rect.h;       /* is a disc (10.6) */
    ed_refresh(o);
}

static bool same(const ed_object_t *a, const ed_object_t *b)
{
    return a->obj.rotation == b->obj.rotation
        && a->obj.rect.x == b->obj.rect.x && a->obj.rect.y == b->obj.rect.y
        && a->obj.rect.w == b->obj.rect.w && a->obj.rect.h == b->obj.rect.h
        && a->obj.portal_mode == b->obj.portal_mode
        && a->obj.portal_gravity == b->obj.portal_gravity
        && a->obj.launch == b->obj.launch && a->start.mode == b->start.mode
        && a->start.gravity_dir == b->start.gravity_dir
        && a->start.speed_mult == b->start.speed_mult;
}

/* One command if any grabbed object is no longer what it was. */
static bool commit_if_changed(ed_level_t *lv, ed_history_t *h, ed_grab_t *g)
{
    bool changed = false;

    for (size_t i = 0; i < g->count; i++)
        changed = changed || !same(ed_find(lv, g->ids[i]), &g->before[i]);
    if (changed)
        ed_grab_commit(lv, h, g);
    else
        ed_grab_cancel(lv, g);
    return changed;
}

bool ed_prop_step(ed_level_t *lv, ed_history_t *h, ed_prop_t p, int dir)
{
    ed_grab_t g;

    ed_grab(lv, &g);
    for (size_t i = 0; i < g.count; i++) {
        ed_object_t *o = ed_find(lv, g.ids[i]);

        if (has(o, p))
            step_one(o, p, dir);
    }
    return commit_if_changed(lv, h, &g);
}

/* The middle of everything grabbed: what a piece turns around. */
static vec2_t middle(const ed_grab_t *g)
{
    double lo_x = INFINITY;
    double lo_y = INFINITY;
    double hi_x = -INFINITY;
    double hi_y = -INFINITY;

    for (size_t i = 0; i < g->count; i++) {
        rect_t r = g->before[i].obj.rect;

        lo_x = fmin(lo_x, r.x);
        lo_y = fmin(lo_y, r.y);
        hi_x = fmax(hi_x, r.x + r.w);
        hi_y = fmax(hi_y, r.y + r.h);
    }
    return (vec2_t){(lo_x + hi_x) / 2.0, (lo_y + hi_y) / 2.0};
}

/* Its center goes around `c`, clockwise on screen like `rot=` (PLAN 4.2). */
static void swing(ed_object_t *o, vec2_t c, double deg, double grid)
{
    rect_t *r = &o->obj.rect;
    double rad = deg * M_PI / 180.0;
    double dx = r->x + r->w / 2.0 - c.x;
    double dy = r->y + r->h / 2.0 - c.y;
    double x = c.x + dx * cos(rad) - dy * sin(rad) - r->w / 2.0;
    double y = c.y + dx * sin(rad) + dy * cos(rad) - r->h / 2.0;

    r->x = round(x / grid) * grid;
    r->y = round(y / grid) * grid;
}

bool ed_rotate(ed_level_t *lv, ed_history_t *h, double deg, double grid)
{
    ed_grab_t g;
    vec2_t c;

    ed_grab(lv, &g);
    c = middle(&g);
    for (size_t i = 0; i < g.count; i++) {
        ed_object_t *o = ed_find(lv, g.ids[i]);

        if (g.count > 1)
            swing(o, c, deg, grid);
        if (!o->is_start)
            o->obj.rotation = turned(o->obj.rotation + deg);
        ed_refresh(o);
    }
    return commit_if_changed(lv, h, &g);
}
