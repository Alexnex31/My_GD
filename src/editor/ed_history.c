/*
** ALEXNEX PROJECT, 2026
** editor/ed_history.c
** File description:
** undo and redo: every edit is a command that can be taken back (FEATURES 11.7)
*/

#include <stdlib.h>
#include <string.h>

#include "editor/ed_history.h"
#include "sim/alloc.h"

static void cmd_free(ed_cmd_t *c)
{
    if (c->objs != NULL)
        for (size_t i = 0; i < c->count; i++)
            free(c->objs[i].extra);
    free(c->objs);
    free(c->at);
    free(c->ids);
    free(c->before);
    free(c->after);
    *c = (ed_cmd_t){0};
}

static void clear(ed_cmd_t *stack, size_t *count)
{
    for (size_t i = 0; i < *count; i++)
        cmd_free(&stack[i]);
    *count = 0;
}

void ed_history_free(ed_history_t *h)
{
    clear(h->undo, &h->nb_undo);
    clear(h->redo, &h->nb_redo);
}

void ed_gesture(ed_history_t *h)
{
    h->gesture += 1;
}

/* A full stack forgets its oldest command. */
static ed_cmd_t *push(ed_cmd_t *stack, size_t *count)
{
    if (*count == ED_HISTORY_MAX) {
        cmd_free(&stack[0]);
        memmove(&stack[0], &stack[1], (ED_HISTORY_MAX - 1) * sizeof(ed_cmd_t));
        *count -= 1;
    }
    *count += 1;
    stack[*count - 1] = (ed_cmd_t){0};
    return &stack[*count - 1];
}

/*
** The command a new edit is written in: the last one when it is the same
** kind of edit in the same gesture, so a stroke is one step. A new edit
** makes what was undone impossible to redo.
*/
static ed_cmd_t *record(ed_history_t *h, ed_cmd_kind_t kind)
{
    ed_cmd_t *top = h->nb_undo > 0 ? &h->undo[h->nb_undo - 1] : NULL;

    clear(h->redo, &h->nb_redo);
    if (top != NULL && kind != ED_CHANGE && top->kind == kind
        && top->gesture == h->gesture)
        return top;
    top = push(h->undo, &h->nb_undo);
    top->kind = kind;
    top->gesture = h->gesture;
    return top;
}

static void *grown(void *old, size_t count, size_t cap, size_t size)
{
    void *bigger = sim_xcalloc(cap, size);

    if (count > 0)
        memcpy(bigger, old, count * size);
    free(old);
    return bigger;
}

/* Its own copy of the object, with its id, and where it is in the list. */
static void keep_object(ed_cmd_t *c, const ed_object_t *o, size_t at)
{
    if (c->count == c->cap) {
        c->cap = c->cap == 0 ? 8 : c->cap * 2;
        c->objs = grown(c->objs, c->count, c->cap, sizeof(ed_object_t));
        c->at = grown(c->at, c->count, c->cap, sizeof(size_t));
    }
    c->objs[c->count] = (ed_object_t){.id = o->id, .obj = o->obj,
        .extra = o->extra != NULL ? sim_xstrdup(o->extra) : NULL};
    c->at[c->count] = at;
    c->count += 1;
}

int ed_do_add(ed_level_t *lv, ed_history_t *h, const object_t *obj,
    const char *extra)
{
    int id = ed_add(lv, obj, extra);

    keep_object(record(h, ED_ADD), &lv->objects[lv->count - 1], lv->count - 1);
    return id;
}

bool ed_do_remove(ed_level_t *lv, ed_history_t *h, int id)
{
    ed_object_t *o = ed_find(lv, id);

    if (o == NULL)
        return false;
    keep_object(record(h, ED_REMOVE), o, (size_t)(o - lv->objects));
    return ed_remove(lv, id);
}

bool ed_do_place(ed_level_t *lv, ed_history_t *h, const ed_entry_t *e,
    vec2_t at, double grid)
{
    object_t o;

    if (!ed_place_object(lv, e, at, grid, &o))
        return false;
    ed_do_add(lv, h, &o, NULL);
    return true;
}

void ed_do_change(ed_level_t *lv, ed_history_t *h, const int *ids,
    const object_t *before, size_t count)
{
    ed_cmd_t *c = record(h, ED_CHANGE);

    c->count = count;
    c->ids = sim_xcalloc(count + 1, sizeof(int));
    c->before = sim_xcalloc(count + 1, sizeof(object_t));
    c->after = sim_xcalloc(count + 1, sizeof(object_t));
    for (size_t i = 0; i < count; i++) {
        c->ids[i] = ids[i];
        c->before[i] = before[i];
        c->after[i] = ed_find(lv, ids[i])->obj;
    }
    lv->dirty = true;
}

/*
** Removed objects went one after the other, each from the place it then
** had: they come back last first, each to that place, and the list is the
** one it was, order included (the order is what is drawn on top).
*/
static void put_back(ed_level_t *lv, const ed_cmd_t *c)
{
    for (size_t i = c->count; i > 0; i--)
        ed_insert(lv, &c->objs[i - 1], c->at[i - 1]);
}

static void take_out(ed_level_t *lv, const ed_cmd_t *c)
{
    for (size_t i = 0; i < c->count; i++)
        ed_remove(lv, c->objs[i].id);
}

static void apply(ed_level_t *lv, const ed_cmd_t *c, bool forward)
{
    if (c->kind == ED_ADD && forward)
        for (size_t i = 0; i < c->count; i++)
            ed_insert(lv, &c->objs[i], c->at[i]);
    if (c->kind == ED_ADD && !forward)
        take_out(lv, c);
    if (c->kind == ED_REMOVE && forward)
        take_out(lv, c);
    if (c->kind == ED_REMOVE && !forward)
        put_back(lv, c);
    if (c->kind != ED_CHANGE)
        return;
    for (size_t i = 0; i < c->count; i++)
        ed_find(lv, c->ids[i])->obj = forward ? c->after[i] : c->before[i];
    lv->dirty = true;
}

/* The command changes stack as it is: its arrays go with it. */
static bool step(ed_level_t *lv, ed_cmd_t *from, size_t *nb_from, ed_cmd_t *to,
    size_t *nb_to, bool forward)
{
    if (*nb_from == 0)
        return false;
    apply(lv, &from[*nb_from - 1], forward);
    *push(to, nb_to) = from[*nb_from - 1];
    *nb_from -= 1;
    return true;
}

bool ed_undo(ed_level_t *lv, ed_history_t *h)
{
    return step(lv, h->undo, &h->nb_undo, h->redo, &h->nb_redo, false);
}

bool ed_redo(ed_level_t *lv, ed_history_t *h)
{
    return step(lv, h->redo, &h->nb_redo, h->undo, &h->nb_undo, true);
}
