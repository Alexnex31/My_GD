/*
** ALEXNEX PROJECT, 2026
** editor/ed_check.c
** File description:
** what looks wrong in the level being edited (FEATURES 11.13)
*/

#include <stdlib.h>

#include "editor/ed_check.h"
#include "sim/alloc.h"
#include "sim/constants.h"

    #define INSIDE_PX 1.0     /* a spike this deep in a block isn't just on it */

static const ed_level_t *g_sorting;

bool ed_issue_is_hint(ed_issue_kind_t kind)
{
    return kind == ED_HINT_SPIKE_IN_BLOCK || kind == ED_HINT_ENDS_IN_MODE;
}

const char *ed_issue_text(ed_issue_kind_t kind)
{
    static const char *const TEXT[ED_ISSUE_KIND_COUNT] = {
        "the same object twice in the same place",
        "below the ground: never reached",
        "behind the start: never reached",
        "a face steeper than 50 degrees looks up: a wall, not a slope",
        "a spike inside a block",
        "the last portal isn't a cube portal",
    };

    return TEXT[kind];
}

/* By the left of the hitbox, then by place in the list: a fixed order. */
static int by_left(const void *a, const void *b)
{
    size_t i = *(const size_t *)a;
    size_t j = *(const size_t *)b;
    double xi = g_sorting->objects[i].obj.hitbox.aabb.x;
    double xj = g_sorting->objects[j].obj.hitbox.aabb.x;

    if (xi != xj)
        return xi < xj ? -1 : 1;
    return i < j ? -1 : (i > j);
}

/* How far `b` reaches inside `a` along a's axes, at the shallowest of them. */
static double depth_along(const hitbox_t *a, const hitbox_t *b)
{
    double least = 1e300;

    for (int k = 0; k < a->naxes; k++) {
        double lo = 1e300;
        double hi = -1e300;

        for (int v = 0; v < b->nverts; v++) {
            double d = a->axes[k].x * b->verts[v].x
                + a->axes[k].y * b->verts[v].y;

            lo = d < lo ? d : lo;
            hi = d > hi ? d : hi;
        }
        hi = (hi < a->axis_hi[k] ? hi : a->axis_hi[k]);
        lo = (lo > a->axis_lo[k] ? lo : a->axis_lo[k]);
        least = hi - lo < least ? hi - lo : least;
    }
    return least;
}

/* The two shapes share more than an edge: one is in the other (G.2). */
static bool overlap(const hitbox_t *a, const hitbox_t *b)
{
    if (a->kind != SHAPE_POLY || b->kind != SHAPE_POLY)
        return false;
    return depth_along(a, b) > INSIDE_PX && depth_along(b, a) > INSIDE_PX;
}

static bool steep(const object_t *o)
{
    if (OBJ_CATEGORY[o->type] != CAT_NEUTRAL)
        return false;
    for (int f = 0; f < o->hitbox.nverts; f++) {
        double up = -o->hitbox.face_n[f].y;

        if (o->hitbox.face_kind[f] == FACE_TILTED && up > 0.0
            && up < FLOOR_MIN_DOT)
            return true;
    }
    return false;
}

/* What one object is alone, whatever stands around it. */
static unsigned alone(const ed_level_t *lv, const object_t *o)
{
    const rect_t *a = &o->hitbox.aabb;
    unsigned found = 0;

    if (a->y >= GROUND_Y)
        found |= 1u << ED_ISSUE_UNDERGROUND;
    if (a->x + a->w < lv->hdr.start.pos.x - PLAYER_HALF)
        found |= 1u << ED_ISSUE_BEHIND_START;
    if (steep(o))
        found |= 1u << ED_ISSUE_STEEP;
    return found;
}

/* Two objects whose hitboxes share some x: `a` is the one on the left. */
static void together(const object_t *a, const object_t *b, unsigned *fa,
    unsigned *fb)
{
    bool a_spike = a->type == OBJ_SPIKE && OBJ_CATEGORY[b->type] == CAT_NEUTRAL;
    bool b_spike = b->type == OBJ_SPIKE && OBJ_CATEGORY[a->type] == CAT_NEUTRAL;

    if (object_same(a, b))
        *fb |= 1u << ED_ISSUE_TWICE;
    if ((a_spike || b_spike) && overlap(&a->hitbox, &b->hitbox))
        *(a_spike ? fa : fb) |= 1u << ED_HINT_SPIKE_IN_BLOCK;
}

/* Sorted by x, an object only meets those that start before it ends. */
static void pairs(const ed_level_t *lv, const size_t *order, size_t n,
    unsigned *flags)
{
    for (size_t i = 0; i < n; i++) {
        const object_t *a = &lv->objects[order[i]].obj;
        double right = a->hitbox.aabb.x + a->hitbox.aabb.w;

        for (size_t j = i + 1; j < n; j++) {
            const object_t *b = &lv->objects[order[j]].obj;

            if (b->hitbox.aabb.x > right)
                break;
            together(a, b, &flags[i], &flags[j]);
        }
    }
}

/* A level that ends as a ship is often one that forgot its last portal. */
static void last_portal(const ed_level_t *lv, const size_t *order, size_t n,
    unsigned *flags)
{
    for (size_t i = n; i > 0; i--) {
        const object_t *o = &lv->objects[order[i - 1]].obj;

        if (o->type != OBJ_PORTAL)
            continue;
        if (o->portal_mode != MODE_CUBE)
            flags[i - 1] |= 1u << ED_HINT_ENDS_IN_MODE;
        return;
    }
}

static void add(ed_report_t *r, ed_issue_kind_t kind, int id)
{
    r->issues[r->count] = (ed_issue_t){kind, id};
    r->count += 1;
    if (ed_issue_is_hint(kind))
        r->hints += 1;
    else
        r->warnings += 1;
}

void ed_report_free(ed_report_t *r)
{
    free(r->issues);
    *r = (ed_report_t){0};
}

/* Start positions aren't objects of the level: nothing is said about them. */
void ed_check(const ed_level_t *lv, ed_report_t *out)
{
    size_t *order = sim_xcalloc(lv->count + 1, sizeof(size_t));
    unsigned *flags = sim_xcalloc(lv->count + 1, sizeof(unsigned));
    size_t n = 0;

    ed_report_free(out);
    for (size_t i = 0; i < lv->count; i++)
        if (!lv->objects[i].is_start)
            order[n++] = i;
    g_sorting = lv;
    qsort(order, n, sizeof(size_t), by_left);
    for (size_t i = 0; i < n; i++)
        flags[i] = alone(lv, &lv->objects[order[i]].obj);
    pairs(lv, order, n, flags);
    last_portal(lv, order, n, flags);
    out->issues = sim_xcalloc(n * ED_ISSUE_KIND_COUNT + 1, sizeof(ed_issue_t));
    for (size_t i = 0; i < n; i++)
        for (int k = 0; k < ED_ISSUE_KIND_COUNT; k++)
            if (flags[i] & (1u << k))
                add(out, (ed_issue_kind_t)k, lv->objects[order[i]].id);
    free(order);
    free(flags);
}
