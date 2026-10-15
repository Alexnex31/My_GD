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
#include "sim/hitbox.h"
#include "sim/modes.h"
#include "sim/speed.h"

    #define TURN_STEP 15.0

/* A start position's speed, or a speed portal's: the same five names. */
static double *speed_of(ed_object_t *o)
{
    return o->is_start ? &o->start.speed : &o->obj.speed;
}

const char *ed_prop_name(ed_prop_t p)
{
    static const char *const NAMES[ED_PROP_COUNT] = {"Turn", "Width",
        "Height", "Kind", "Gravity", "Speed", "Size"};

    return NAMES[p];
}

static bool has_kind(const ed_object_t *o)
{
    return o->is_start || o->obj.type == OBJ_PORTAL
        || o->obj.type == OBJ_GRAVITY || o->obj.type == OBJ_PAD
        || o->obj.type == OBJ_ORB || o->obj.type == OBJ_MINI;
}

static bool has(const ed_object_t *o, ed_prop_t p)
{
    if (p == ED_PROP_KIND)
        return has_kind(o);
    if (p == ED_PROP_SPEED)
        return o->is_start || o->obj.type == OBJ_SPEED;
    if (p == ED_PROP_GRAVITY || p == ED_PROP_SIZE)
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
    if (o->obj.type == OBJ_MINI)
        return o->obj.portal_mini ? "on" : "off";
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
        snprintf(out, size, "x%s", speed_word(*speed_of((ed_object_t *)o)));
    if (p == ED_PROP_SIZE)
        snprintf(out, size, "%s", o->start.mini ? "mini" : "normal");
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
    else if (o->obj.type == OBJ_MINI)
        o->obj.portal_mini = !o->obj.portal_mini;
    else
        o->obj.launch = next_launch(&o->obj, dir);
}

/* The speeds stop at their ends: nothing is slower than x0.5. */
static void step_speed(ed_object_t *o, int dir)
{
    int at = 1;

    for (int i = 0; i < SPEED_COUNT; i++)
        if (SPEED_NAMES[i] == *speed_of(o))
            at = i;
    at += dir;
    at = at < 0 ? 0 : (at >= SPEED_COUNT ? SPEED_COUNT - 1 : at);
    *speed_of(o) = SPEED_NAMES[at];
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
    if (p == ED_PROP_SIZE)
        o->start.mini = !o->start.mini;
    if (o->obj.type == OBJ_SAW && !o->is_start && p == ED_PROP_W)
        o->obj.rect.h = o->obj.rect.w;       /* a saw stays round: its hitbox */
    if (o->obj.type == OBJ_SAW && !o->is_start && p == ED_PROP_H)
        o->obj.rect.w = o->obj.rect.h;       /* is a disc (10.6) */
    ed_refresh(o);
}

/* One command if any grabbed object is no longer what it was. */
static bool commit_if_changed(ed_level_t *lv, ed_history_t *h, ed_grab_t *g)
{
    bool changed = false;
    size_t hint = 0;

    for (size_t i = 0; i < g->count && !changed; i++) {
        const ed_object_t *o = ed_find_from(lv, g->ids[i], &hint);

        changed = o != NULL && !ed_same_object(o, &g->before[i]);
    }
    if (changed)
        ed_grab_commit(lv, h, g);
    else
        ed_grab_cancel(lv, g);
    return changed;
}

bool ed_prop_step(ed_level_t *lv, ed_history_t *h, ed_prop_t p, int dir)
{
    ed_grab_t g;
    size_t hint = 0;

    ed_grab(lv, &g);
    for (size_t i = 0; i < g.count; i++) {
        ed_object_t *o = ed_find_from(lv, g.ids[i], &hint);

        if (o != NULL && has(o, p))
            step_one(o, p, dir);
    }
    return commit_if_changed(lv, h, &g);
}

/* The middle of everything grabbed, each as it is drawn: turned. */
static vec2_t middle(const ed_grab_t *g)
{
    double lo_x = INFINITY;
    double lo_y = INFINITY;
    double hi_x = -INFINITY;
    double hi_y = -INFINITY;

    for (size_t i = 0; i < g->count; i++) {
        rect_t r = object_drawn_bounds(&g->before[i].obj);

        lo_x = fmin(lo_x, r.x);
        lo_y = fmin(lo_y, r.y);
        hi_x = fmax(hi_x, r.x + r.w);
        hi_y = fmax(hi_y, r.y + r.h);
    }
    return (vec2_t){(lo_x + hi_x) / 2.0, (lo_y + hi_y) / 2.0};
}

/*
** The grid corner or cell center nearest the middle. A quarter turn around
** one takes the grid onto itself: in half cells they are the (a, b) with
** a + b even. Half a turn around the middle itself is already on the grid.
*/
static vec2_t pivot(vec2_t c, double deg, double grid)
{
    double half = grid / 2.0;
    double a = round(c.x / half);
    double b = round(c.y / half);
    double turns = fmod(fmod(deg, 360.0) + 360.0, 360.0);
    double fx = c.x / half - a;
    double fy = c.y / half - b;

    if (turns != 90.0 && turns != 270.0)
        return c;
    if (fmod(fabs(a + b), 2.0) == 0.0)
        return (vec2_t){a * half, b * half};
    if (fabs(fx) >= fabs(fy))
        return (vec2_t){(a + (fx < 0.0 ? -1.0 : 1.0)) * half, b * half};
    return (vec2_t){a * half, (b + (fy < 0.0 ? -1.0 : 1.0)) * half};
}

/* Its center goes around `p`, clockwise on screen like `rot=` (PLAN 4.2). */
static void swing(ed_object_t *o, vec2_t p, double cs, double sn)
{
    rect_t *r = &o->obj.rect;
    double dx = r->x + r->w / 2.0 - p.x;
    double dy = r->y + r->h / 2.0 - p.y;

    r->x = p.x + dx * cs - dy * sn - r->w / 2.0 + 0.0;   /* never -0 */
    r->y = p.y + dx * sn + dy * cs - r->h / 2.0 + 0.0;
}

/* The same piece again, nothing changed since the last turn: its pivot. */
static vec2_t turn_around(const ed_history_t *h, const ed_grab_t *g,
    double deg, double grid)
{
    vec2_t c = middle(g);

    if (h->pivot_edits != 0 && h->pivot_edits == h->edits
        && c.x == h->pivot_middle.x && c.y == h->pivot_middle.y)
        return h->pivot;
    return pivot(c, deg, grid);
}

bool ed_rotate(ed_level_t *lv, ed_history_t *h, double deg, double grid)
{
    ed_grab_t g;
    vec2_t p;
    double cs = 0.0;
    double sn = 0.0;
    size_t hint = 0;
    bool done;

    ed_grab(lv, &g);
    p = turn_around(h, &g, deg, grid);
    hitbox_rotation(deg, &cs, &sn);
    for (size_t i = 0; i < g.count; i++) {
        ed_object_t *o = ed_find_from(lv, g.ids[i], &hint);

        if (o == NULL)
            continue;
        if (g.count > 1)
            swing(o, p, cs, sn);
        if (!o->is_start)
            o->obj.rotation = turned(o->obj.rotation + deg);
        ed_refresh(o);
    }
    done = commit_if_changed(lv, h, &g);
    if (done) {
        ed_grab(lv, &g);
        h->pivot = p;
        h->pivot_middle = middle(&g);
        h->pivot_edits = h->edits;
        ed_grab_cancel(lv, &g);
    }
    return done;
}
