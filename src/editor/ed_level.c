/*
** ALEXNEX PROJECT, 2026
** editor/ed_level.c
** File description:
** the level the editor edits: objects with stable ids, open, save (FEATURES 11.1)
*/

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

void ed_level_new(ed_level_t *lv, const char *id)
{
    *lv = (ed_level_t){.next_id = 1};
    snprintf(lv->id, sizeof(lv->id), "%s", id);
    level_header_defaults(&lv->hdr, id);
    snprintf(lv->hdr.name, sizeof(lv->hdr.name), "LEVEL %s", id);
}

void ed_level_free(ed_level_t *lv)
{
    for (size_t i = 0; i < lv->count; i++)
        free(lv->objects[i].extra);
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

/* The order of the others is kept: it decides what is drawn on top. */
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

    *lv = (ed_level_t){.next_id = 1};
    snprintf(lv->id, sizeof(lv->id), "%s", id);
    level_header_defaults(&lv->hdr, id);
    level_parse_doc(buf, len, id, &doc, &lv->hdr, NULL);
    for (size_t i = 0; i < doc.count; i++)
        ed_add(lv, &doc.objs[i], doc.extras[i]);
    for (size_t i = 0; i < doc.nb_starts; i++) {
        ed_start_object(&start, &doc.starts[i]);
        ed_add_object(lv, &start);
    }
    level_doc_free(&doc);
    lv->dirty = false;
}

static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    long size;
    char *buf;

    if (f == NULL)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0
        || size > 64L * 1024 * 1024) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = sim_xcalloc((size_t)size + 1, 1);
    *len = fread(buf, 1, (size_t)size, f);
    fclose(f);
    return buf;
}

int ed_level_open(ed_level_t *lv, const char *path)
{
    char id[LEVEL_ID_MAX + 1];
    size_t len = 0;
    char *buf;

    if (!level_id_from_path(path, id, sizeof(id)))
        return -1;
    buf = read_file(path, &len);
    if (buf == NULL)
        return -1;
    ed_level_from_text(lv, buf, len, id);
    free(buf);
    return 0;
}

/*
** Arrays of its own, pointing at nothing of the level's but the extras. The
** start positions leave the objects here: the game never meets one (11.8).
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
}

void ed_doc_free(level_doc_t *doc)
{
    free(doc->objs);
    free(doc->extras);                       /* the strings are the level's */
    free(doc->starts);
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
    double rad = -o->rotation * M_PI / 180.0;
    double x = c.x + (at.x - c.x) * cos(rad) - (at.y - c.y) * sin(rad);
    double y = c.y + (at.x - c.x) * sin(rad) + (at.y - c.y) * cos(rad);

    return x >= o->rect.x && x < o->rect.x + o->rect.w
        && y >= o->rect.y && y < o->rect.y + o->rect.h;
}

/*
** Drawn later means on top: by layer (blocks, hazards, interactive objects,
** which is the category's own order), then by x, then by place in the list.
*/
static bool drawn_over(const object_t *a, const object_t *b)
{
    if (OBJ_CATEGORY[a->type] != OBJ_CATEGORY[b->type])
        return OBJ_CATEGORY[a->type] > OBJ_CATEGORY[b->type];
    return a->rect.x >= b->rect.x;
}

int ed_hit(const ed_level_t *lv, vec2_t at)
{
    const ed_object_t *top = NULL;

    for (size_t i = 0; i < lv->count; i++) {
        const ed_object_t *o = &lv->objects[i];

        if (!rect_holds(&o->obj, at))
            continue;
        if (top == NULL || (o->is_start && !top->is_start)
            || (o->is_start == top->is_start
            && drawn_over(&o->obj, &top->obj)))
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
    entry(out, &n, max, OBJ_SAW, NULL);
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

static bool same_object(const ed_object_t *ea, const ed_object_t *eb)
{
    const object_t *a = &ea->obj;
    const object_t *b = &eb->obj;

    return ea->is_start == eb->is_start && a->type == b->type && a->rect.x == b->rect.x
        && a->rect.y == b->rect.y && a->rect.w == b->rect.w
        && a->rect.h == b->rect.h && a->rotation == b->rotation
        && a->portal_mode == b->portal_mode
        && a->portal_gravity == b->portal_gravity && a->launch == b->launch;
}

/* The object is made by the loader from its line: what is placed is what
** the game will read back. */
bool ed_place_object(const ed_level_t *lv, const ed_entry_t *e, vec2_t at,
    double grid, ed_object_t *out)
{
    level_start_t st = {.speed_mult = 1.0, .gravity_dir = 1,
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
    for (size_t i = 0; i < lv->count; i++)
        if (same_object(&lv->objects[i], out))
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

void ed_next_id(const char *const *ids, size_t count, char *out, size_t size)
{
    long long highest = 0;

    for (size_t i = 0; i < count; i++) {
        long long v = strtoll(ids[i], NULL, 10);

        highest = v > highest ? v : highest;
    }
    snprintf(out, size, "%lld", highest + 1);
}
