/*
** ALEXNEX PROJECT, 2026
** editor/ed_select.c
** File description:
** the selection: choosing it, moving it, copying it (FEATURES 11.5, 11.6)
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "editor/ed_select.h"
#include "sim/alloc.h"
#include "sim/geom.h"
#include "sim/hitbox.h"

size_t ed_selected(const ed_level_t *lv)
{
    size_t n = 0;

    for (size_t i = 0; i < lv->count; i++)
        n += lv->objects[i].selected;
    return n;
}

static void select_every(ed_level_t *lv, bool on)
{
    for (size_t i = 0; i < lv->count; i++)
        lv->objects[i].selected = on;
}

void ed_select_none(ed_level_t *lv)
{
    select_every(lv, false);
}

void ed_select_all(ed_level_t *lv)
{
    select_every(lv, true);
}

void ed_select_click(ed_level_t *lv, int id, bool shift)
{
    ed_object_t *o = ed_find(lv, id);

    if (shift) {
        if (o != NULL)
            o->selected = !o->selected;
        return;
    }
    select_every(lv, false);
    if (o != NULL)
        o->selected = true;
}

void ed_select_box(ed_level_t *lv, rect_t box, bool add)
{
    for (size_t i = 0; i < lv->count; i++) {
        ed_object_t *o = &lv->objects[i];
        bool in = rect_overlap(object_drawn_bounds(&o->obj), box);

        o->selected = in || (add && o->selected);
    }
}

/* From the end, so removing one doesn't move those still to look at. */
bool ed_delete_selected(ed_level_t *lv, ed_history_t *h)
{
    bool any = false;

    for (size_t i = lv->count; i > 0; i--)
        if (lv->objects[i - 1].selected)
            any = ed_do_remove(lv, h, lv->objects[i - 1].id) || any;
    return any;
}

void ed_grab(ed_level_t *lv, ed_grab_t *g)
{
    size_t n = ed_selected(lv);

    *g = (ed_grab_t){.ids = sim_xcalloc(n + 1, sizeof(int)),
        .before = sim_xcalloc(n + 1, sizeof(object_t))};
    for (size_t i = 0; i < lv->count; i++)
        if (lv->objects[i].selected) {
            g->ids[g->count] = lv->objects[i].id;
            g->before[g->count] = lv->objects[i].obj;
            g->count += 1;
        }
}

static void release(ed_grab_t *g)
{
    free(g->ids);
    free(g->before);
    *g = (ed_grab_t){0};
}

/* The hitbox is rebuilt: the canvas, a click and a playtest see one shape. */
void ed_grab_move(ed_level_t *lv, const ed_grab_t *g, double dx, double dy)
{
    for (size_t i = 0; i < g->count; i++) {
        ed_object_t *o = ed_find(lv, g->ids[i]);

        if (o == NULL)
            continue;
        o->obj = g->before[i];
        o->obj.rect.x += dx;
        o->obj.rect.y += dy;
        hitbox_for_object(&o->obj);
    }
}

void ed_grab_cancel(ed_level_t *lv, ed_grab_t *g)
{
    ed_grab_move(lv, g, 0.0, 0.0);
    release(g);
}

bool ed_grab_drop(ed_level_t *lv, ed_history_t *h, ed_grab_t *g)
{
    const ed_object_t *first = g->count > 0 ? ed_find(lv, g->ids[0]) : NULL;
    bool moved = first != NULL && (first->obj.rect.x != g->before[0].rect.x
        || first->obj.rect.y != g->before[0].rect.y);

    if (moved)
        ed_do_change(lv, h, g->ids, g->before, g->count);
    release(g);
    return moved;
}

bool ed_nudge(ed_level_t *lv, ed_history_t *h, double dx, double dy)
{
    ed_grab_t g;

    ed_grab(lv, &g);
    ed_grab_move(lv, &g, dx, dy);
    return ed_grab_drop(lv, h, &g);
}

double ed_snap_delta(double travel, double grid)
{
    return round(travel / grid) * grid;
}

void ed_clip_free(ed_clip_t *clip)
{
    for (size_t i = 0; i < clip->count; i++)
        free(clip->extras[i]);
    free(clip->objs);
    free(clip->extras);
    *clip = (ed_clip_t){0};
}

/* With nothing selected the clipboard keeps what it had. */
bool ed_copy(const ed_level_t *lv, ed_clip_t *clip)
{
    size_t n = ed_selected(lv);

    if (n == 0)
        return false;
    ed_clip_free(clip);
    clip->objs = sim_xcalloc(n, sizeof(object_t));
    clip->extras = sim_xcalloc(n, sizeof(char *));
    clip->corner = (vec2_t){INFINITY, INFINITY};
    for (size_t i = 0; i < lv->count; i++) {
        const ed_object_t *o = &lv->objects[i];

        if (!o->selected)
            continue;
        clip->objs[clip->count] = o->obj;
        clip->extras[clip->count] = o->extra != NULL ? sim_xstrdup(o->extra)
            : NULL;
        clip->corner.x = fmin(clip->corner.x, o->obj.rect.x);
        clip->corner.y = fmin(clip->corner.y, o->obj.rect.y);
        clip->count += 1;
    }
    return true;
}

static bool paste_moved(ed_level_t *lv, ed_history_t *h, const ed_clip_t *clip,
    double dx, double dy)
{
    select_every(lv, false);
    for (size_t i = 0; i < clip->count; i++) {
        object_t o = clip->objs[i];

        o.rect.x += dx;
        o.rect.y += dy;
        hitbox_for_object(&o);
        ed_do_add(lv, h, &o, clip->extras[i]);
        lv->objects[lv->count - 1].selected = true;
    }
    return clip->count > 0;
}

/* Cell to cell, so copies of objects off the grid keep their offset. */
bool ed_paste(ed_level_t *lv, ed_history_t *h, const ed_clip_t *clip,
    vec2_t at, double grid)
{
    if (clip->count == 0)
        return false;
    return paste_moved(lv, h, clip,
        ed_snap(at.x, grid) - ed_snap(clip->corner.x, grid),
        ed_snap(at.y, grid) - ed_snap(clip->corner.y, grid));
}

bool ed_duplicate(ed_level_t *lv, ed_history_t *h, double grid)
{
    ed_clip_t clip = {0};
    bool done = ed_copy(lv, &clip) && paste_moved(lv, h, &clip, grid, 0.0);

    ed_clip_free(&clip);
    return done;
}
