/*
** ALEXNEX PROJECT, 2026
** editor/ed_level.c
** File description:
** the level the editor edits: objects with stable ids, open, save (FEATURES 11.1)
*/

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor/ed_level.h"
#include "sim/alloc.h"
#include "sim/constants.h"
#include "sim/hitbox.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "sim/speed.h"

void ed_level_new(ed_level_t *lv, const char *id)
{
    *lv = (ed_level_t){.next_id = 1, .layer = ED_LAYER_ALL};
    snprintf(lv->id, sizeof(lv->id), "%s", id);
    level_header_defaults(&lv->hdr, id);
    snprintf(lv->hdr.name, sizeof(lv->hdr.name), "LEVEL %s", id);
}

void ed_level_free(ed_level_t *lv)
{
    for (size_t i = 0; i < lv->count; i++)
        free(lv->objects[i].extra);
    for (size_t i = 0; i < lv->nb_kept; i++)
        free(lv->kept[i]);
    free(lv->kept);
    free(lv->objects);
    *lv = (ed_level_t){0};
}

static void make_room(ed_level_t *lv)
{
    ed_object_t *bigger;

    if (lv->count < lv->cap)
        return;
    lv->cap = lv->cap == 0 ? 64 : lv->cap * 2;
    bigger = sim_xcalloc(lv->cap, sizeof(ed_object_t));
    if (lv->count > 0)
        memcpy(bigger, lv->objects, lv->count * sizeof(ed_object_t));
    free(lv->objects);
    lv->objects = bigger;
}

/* Everything of an object but its id and whether it is selected. */
static void copy_into(ed_object_t *dst, const ed_object_t *src, int id)
{
    *dst = (ed_object_t){.id = id, .obj = src->obj,
        .extra = src->extra != NULL ? sim_xstrdup(src->extra) : NULL,
        .is_start = src->is_start, .start = src->start};
}

int ed_add_object(ed_level_t *lv, const ed_object_t *src)
{
    make_room(lv);
    copy_into(&lv->objects[lv->count], src, lv->next_id);
    lv->count += 1;
    lv->next_id += 1;
    lv->dirty = true;
    return lv->next_id - 1;
}

int ed_add(ed_level_t *lv, const object_t *obj, const char *extra)
{
    ed_object_t o = {.obj = *obj, .extra = (char *)extra};

    return ed_add_object(lv, &o);
}

void ed_refresh(ed_object_t *o)
{
    hitbox_for_object(&o->obj);
    if (o->is_start)
        o->start.pos = (vec2_t){o->obj.rect.x + o->obj.rect.w / 2.0,
            o->obj.rect.y + o->obj.rect.h / 2.0};
}

/* Shown and clicked as one cell of 100 px around where the player starts. */
void ed_start_object(ed_object_t *out, const level_start_t *st)
{
    *out = (ed_object_t){.is_start = true, .start = *st};
    out->obj = (object_t){.type = OBJ_BLOCK, .size = 2,
        .rect = {st->pos.x - UNIT, st->pos.y - UNIT, 2.0 * UNIT, 2.0 * UNIT}};
    hitbox_for_object(&out->obj);
}

size_t ed_starts(const ed_level_t *lv)
{
    size_t n = 0;

    for (size_t i = 0; i < lv->count; i++)
        n += lv->objects[i].is_start;
    return n;
}

void ed_insert(ed_level_t *lv, const ed_object_t *o, size_t at)
{
    make_room(lv);
    at = at > lv->count ? lv->count : at;
    memmove(&lv->objects[at + 1], &lv->objects[at],
        (lv->count - at) * sizeof(ed_object_t));
    copy_into(&lv->objects[at], o, o->id);
    lv->count += 1;
    lv->next_id = o->id >= lv->next_id ? o->id + 1 : lv->next_id;
    lv->dirty = true;
}

ed_object_t *ed_find(ed_level_t *lv, int id)
{
    for (size_t i = 0; i < lv->count; i++)
        if (lv->objects[i].id == id)
            return &lv->objects[i];
    return NULL;
}

/*
** From where the last one was found, then round: ids asked for in list order
** are found in one walk of the list, not one walk each.
*/
ed_object_t *ed_find_from(ed_level_t *lv, int id, size_t *hint)
{
    size_t from = *hint < lv->count ? *hint : 0;

    for (size_t k = 0; k < lv->count; k++) {
        size_t i = (from + k) % lv->count;

        if (lv->objects[i].id == id) {
            *hint = i + 1;
            return &lv->objects[i];
        }
    }
    return NULL;
}

/* The order of the others is kept: it decides what is drawn on top. */
static int by_id(const void *a, const void *b)
{
    int ia = *(const int *)a;
    int ib = *(const int *)b;

    return (ia > ib) - (ia < ib);
}

/* The ids sorted once, the list compacted once: a big paste's undo is
** n log n, not n times a search and a shift (11.7). */
size_t ed_remove_ids(ed_level_t *lv, const int *ids, size_t n)
{
    int *sorted = sim_xcalloc(n + 1, sizeof(int));
    size_t kept = 0;

    if (n > 0)
        memcpy(sorted, ids, n * sizeof(int));
    qsort(sorted, n, sizeof(int), by_id);
    for (size_t i = 0; i < lv->count; i++) {
        ed_object_t *o = &lv->objects[i];

        if (bsearch(&o->id, sorted, n, sizeof(int), by_id) != NULL)
            free(o->extra);
        else
            lv->objects[kept++] = *o;
    }
    free(sorted);
    n = lv->count - kept;
    lv->count = kept;
    lv->dirty = lv->dirty || n > 0;
    return n;
}

size_t ed_remove_selected(ed_level_t *lv)
{
    size_t kept = 0;
    size_t n = 0;

    for (size_t i = 0; i < lv->count; i++) {
        if (lv->objects[i].selected)
            free(lv->objects[i].extra);
        else
            lv->objects[kept++] = lv->objects[i];
    }
    n = lv->count - kept;
    lv->count = kept;
    lv->dirty = lv->dirty || n > 0;
    return n;
}

/* One merge: each object lands at its place, those between move once. */
void ed_insert_many(ed_level_t *lv, const ed_object_t *const *objs,
    const size_t *at, size_t n)
{
    size_t total = lv->count + n;
    ed_object_t *merged = sim_xcalloc(total + 1, sizeof(ed_object_t));
    size_t from = 0;
    size_t k = 0;

    for (size_t i = 0; i < total; i++) {
        if (k < n && (at[k] <= i || from == lv->count)) {
            copy_into(&merged[i], objs[k], objs[k]->id);
            lv->next_id = objs[k]->id >= lv->next_id ? objs[k]->id + 1
                : lv->next_id;
            k += 1;
        } else {
            merged[i] = lv->objects[from++];
        }
    }
    free(lv->objects);
    lv->objects = merged;
    lv->count = total;
    lv->cap = total + 1;
    lv->dirty = lv->dirty || n > 0;
}

bool ed_remove(ed_level_t *lv, int id)
{
    ed_object_t *o = ed_find(lv, id);
    size_t at;

    if (o == NULL)
        return false;
    at = (size_t)(o - lv->objects);
    free(o->extra);
    memmove(o, o + 1, (lv->count - at - 1) * sizeof(ed_object_t));
    lv->count -= 1;
    lv->dirty = true;
    return true;
}

void ed_level_from_text(ed_level_t *lv, const char *buf, size_t len,
    const char *id)
{
    ed_object_t start;
    level_doc_t doc;

    *lv = (ed_level_t){.next_id = 1, .layer = ED_LAYER_ALL};
    snprintf(lv->id, sizeof(lv->id), "%s", id);
    level_header_defaults(&lv->hdr, id);
    level_parse_doc(buf, len, id, &doc, &lv->hdr, NULL);
    for (size_t i = 0; i < doc.count; i++)
        ed_add(lv, &doc.objs[i], doc.extras[i]);
    for (size_t i = 0; i < doc.nb_starts; i++) {
        ed_start_object(&start, &doc.starts[i]);
        ed_add_object(lv, &start);
    }
    lv->kept = doc.kept;                     /* written back as they were */
    lv->nb_kept = doc.nb_kept;
    doc.kept = NULL;
    doc.nb_kept = 0;
    level_doc_free(&doc);
    lv->dirty = false;
}

int ed_level_open(ed_level_t *lv, const char *path)
{
    char id[LEVEL_ID_MAX + 1];
    size_t len = 0;
    char *buf;

    if (!level_id_from_path(path, id, sizeof(id)))
        return -1;
    buf = level_read_file(path, &len);
    if (buf == NULL)
        return -1;
    ed_level_from_text(lv, buf, len, id);
    free(buf);
    return 0;
}

/*
** Arrays of its own, pointing at nothing of the level's but the extras and
** the kept lines. The start positions leave the objects here: the game never
** meets one (11.8).
*/
void ed_level_doc(const ed_level_t *lv, level_doc_t *doc)
{
    *doc = (level_doc_t){0};
    doc->objs = sim_xcalloc(lv->count + 1, sizeof(object_t));
    doc->extras = sim_xcalloc(lv->count + 1, sizeof(char *));
    doc->starts = sim_xcalloc(lv->count + 1, sizeof(level_start_t));
    for (size_t i = 0; i < lv->count; i++) {
        const ed_object_t *o = &lv->objects[i];

        if (o->is_start) {
            doc->starts[doc->nb_starts] = o->start;
            doc->nb_starts += 1;
            continue;
        }
        doc->objs[doc->count] = o->obj;
        doc->extras[doc->count] = o->extra;
        doc->count += 1;
    }
    doc->kept = lv->kept;
    doc->nb_kept = lv->nb_kept;
}

void ed_doc_free(level_doc_t *doc)
{
    free(doc->objs);
    free(doc->extras);                       /* the strings are the level's */
    free(doc->starts);                       /* and so are the kept lines */
    *doc = (level_doc_t){0};
}

char *ed_level_text(const ed_level_t *lv)
{
    level_doc_t doc;
    char *text;

    ed_level_doc(lv, &doc);
    text = level_write_mem(&doc, &lv->hdr);
    ed_doc_free(&doc);
    return text;
}

int ed_level_save(ed_level_t *lv, const char *path)
{
    level_doc_t doc;
    int ret;

    ed_level_doc(lv, &doc);
    ret = level_write(path, &doc, &lv->hdr);
    ed_doc_free(&doc);
    if (ret == 0)
        lv->dirty = false;
    return ret;
}

void ed_level_measure(const ed_level_t *lv, double *end_x, double *kill_y)
{
    level_doc_t doc;
    sim_t *s = sim_xcalloc(1, sizeof(sim_t));

    ed_level_doc(lv, &doc);
    sim_init(s, &doc, &lv->hdr, lv->id);
    *end_x = lv->hdr.start.pos.x + s->lvl.end_shift;
    *kill_y = s->lvl.kill_y;
    sim_free(s);
    free(s);
    ed_doc_free(&doc);
}

double ed_snap(double v, double grid)
{
    return floor(v / grid) * grid;
}

/* The point in the object's own frame: its rect is then a plain rect (11.5). */
static bool rect_holds(const object_t *o, vec2_t at)
{
    vec2_t c = {o->rect.x + o->rect.w / 2.0, o->rect.y + o->rect.h / 2.0};
    double cs = 0.0;
    double sn = 0.0;
    double x;
    double y;

    hitbox_rotation(-o->rotation, &cs, &sn);
    x = c.x + (at.x - c.x) * cs - (at.y - c.y) * sn;
    y = c.y + (at.x - c.x) * sn + (at.y - c.y) * cs;

    return x >= o->rect.x && x < o->rect.x + o->rect.w
        && y >= o->rect.y && y < o->rect.y + o->rect.h;
}

bool ed_in_layer(const ed_level_t *lv, const ed_object_t *o)
{
    return lv->layer == ED_LAYER_ALL || o->is_start
        || o->obj.editor_layer == lv->layer;
}

static const ed_level_t *g_drawn;

/* The list's order is the order they were placed in (11.15). */
static int by_drawn(const void *a, const void *b)
{
    size_t i = *(const size_t *)a;
    size_t j = *(const size_t *)b;

    return object_draw_cmp(&g_drawn->objects[i].obj, (long)i,
        &g_drawn->objects[j].obj, (long)j);
}

void ed_sort_drawn(const ed_level_t *lv, size_t *idx, size_t n)
{
    g_drawn = lv;
    qsort(idx, n, sizeof(size_t), by_drawn);
}

/*
** The one drawn on top: start positions over the level's objects, which
** are drawn by Z layer, then Z order, then the later placed (11.15). The
** list is walked in order, so a tie goes to `o`, the later one.
*/
int ed_hit(const ed_level_t *lv, vec2_t at)
{
    const ed_object_t *top = NULL;

    for (size_t i = 0; i < lv->count; i++) {
        const ed_object_t *o = &lv->objects[i];

        if (!ed_in_layer(lv, o) || !rect_holds(&o->obj, at))
            continue;
        if (top == NULL || (o->is_start && !top->is_start)
            || (o->is_start == top->is_start && object_draw_cmp(&o->obj,
            (long)i, &top->obj, (long)(top - lv->objects)) > 0))
            top = o;
    }
    return top != NULL ? top->id : -1;
}

static void entry(ed_entry_t *out, int *n, int max, obj_type_t type,
    const char *word)
{
    if (*n >= max)
        return;
    out[*n] = (ed_entry_t){.type = type, .word = word};
    snprintf(out[*n].label, sizeof(out[*n].label), "%s%s%s",
        obj_type_name(type), word != NULL ? " " : "",
        word != NULL ? word : "");
    *n += 1;
}

/* A new mode or colour in the tables is in the palette with no line here. */
int ed_entries(ed_entry_t *out, int max)
{
    int n = 0;

    entry(out, &n, max, OBJ_BLOCK, NULL);
    entry(out, &n, max, OBJ_SPIKE, NULL);
    entry(out, &n, max, OBJ_SLOPE, NULL);
    for (int m = 0; m < MODE_COUNT; m++)
        entry(out, &n, max, OBJ_PORTAL, MODES[m].name);
    entry(out, &n, max, OBJ_GRAVITY, "up");
    entry(out, &n, max, OBJ_GRAVITY, "down");
    entry(out, &n, max, OBJ_MINI, "on");
    entry(out, &n, max, OBJ_MINI, "off");
    entry(out, &n, max, OBJ_SAW, NULL);
    for (int k = 0; k < SPEED_COUNT; k++)
        entry(out, &n, max, OBJ_SPEED, speed_word(SPEED_NAMES[k]));
    for (int k = 0; k < LAUNCH_KIND_COUNT; k++)
        if (LAUNCHES[k].pad_v != 0.0)
            entry(out, &n, max, OBJ_PAD, LAUNCHES[k].name);
    for (int k = 0; k < LAUNCH_KIND_COUNT; k++)
        if (LAUNCHES[k].orb_v != 0.0)
            entry(out, &n, max, OBJ_ORB, LAUNCHES[k].name);
    if (n < max) {
        out[n] = (ed_entry_t){.start = true, .label = "start position"};
        n += 1;
    }
    return n;
}

/*
** The placement's "the very same object" is the level check's "twice in the
** same place" (object_same), and a property step's "nothing changed" is
** the same with the editor layer too.
*/
bool ed_same_object(const ed_object_t *a, const ed_object_t *b,
    bool layer_too)
{
    const level_start_t *sa = &a->start;
    const level_start_t *sb = &b->start;

    if (a->is_start != b->is_start || !object_same(&a->obj, &b->obj))
        return false;
    if (layer_too && a->obj.editor_layer != b->obj.editor_layer)
        return false;
    return !a->is_start || (sa->mode == sb->mode
        && sa->gravity_dir == sb->gravity_dir && sa->speed == sb->speed
        && sa->mini == sb->mini);
}

/* The object is made by the loader from its line: what is placed is what
** the game will read back. */
bool ed_place_object(const ed_level_t *lv, const ed_entry_t *e, vec2_t at,
    double grid, ed_object_t *out)
{
    level_start_t st = {.speed = 1.0, .gravity_dir = 1,
        .pos = {ed_snap(at.x, grid) + UNIT, ed_snap(at.y, grid) + UNIT}};
    char line[128];

    *out = (ed_object_t){0};
    if (e->start)
        ed_start_object(out, &st);
    snprintf(line, sizeof(line), "%s %.10g %.10g 2%s%s",
        obj_type_name(e->type), ed_snap(at.x, grid), ed_snap(at.y, grid),
        e->word != NULL ? " " : "", e->word != NULL ? e->word : "");
    if (!e->start && level_object_from_line(line, &out->obj) != 0)
        return false;
    out->obj.editor_layer = lv->layer == ED_LAYER_ALL ? 0 : lv->layer;
    for (size_t i = 0; i < lv->count; i++)
        if (ed_same_object(&lv->objects[i], out, false))
            return false;
    return true;
}

bool ed_place(ed_level_t *lv, const ed_entry_t *e, vec2_t at, double grid)
{
    ed_object_t o;

    if (!ed_place_object(lv, e, at, grid, &o))
        return false;
    ed_add_object(lv, &o);
    return true;
}

static bool taken(const char *const *ids, size_t count, long long n)
{
    for (size_t i = 0; i < count; i++)
        if (strtoll(ids[i], NULL, 10) == n)
            return true;
    return false;
}

/*
** One past the highest. When that doesn't fit `size` (an id is LEVEL_ID_MAX
** digits at most), the smallest number no level has: a cut id could be one
** that exists, and a save would write over it.
*/
void ed_next_id(const char *const *ids, size_t count, char *out, size_t size)
{
    long long highest = 0;

    for (size_t i = 0; i < count; i++) {
        long long v = strtoll(ids[i], NULL, 10);

        highest = v > highest ? v : highest;
    }
    if (highest < LLONG_MAX
        && snprintf(NULL, 0, "%lld", highest + 1) < (int)size) {
        snprintf(out, size, "%lld", highest + 1);
        return;
    }
    for (long long n = 1; ; n++)
        if (!taken(ids, count, n)) {
            snprintf(out, size, "%lld", n);
            return;
        }
}
