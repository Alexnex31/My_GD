/*
** ALEXNEX PROJECT, 2026
** sim/level_write.c
** File description:
** a level document back to its text: what the editor saves (FEATURES 11.12)
*/

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/alloc.h"
#include "sim/level.h"
#include "sim/modes.h"

typedef struct text_buf {
    char *text;
    size_t len;
    size_t cap;
} text_buf_t;

static void put(text_buf_t *b, const char *fmt, ...)
{
    char line[1200];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    n = n < (int)sizeof(line) ? n : (int)sizeof(line) - 1;
    if (b->len + (size_t)n + 1 > b->cap) {
        char *bigger = sim_xcalloc((b->cap + (size_t)n) * 2 + 64, 1);

        if (b->len > 0)
            memcpy(bigger, b->text, b->len);
        free(b->text);
        b->text = bigger;
        b->cap = (b->cap + (size_t)n) * 2 + 64;
    }
    memcpy(b->text + b->len, line, (size_t)n + 1);
    b->len += (size_t)n;
}

/* Only what differs from the defaults a file without it gets (7.2). */
static void put_header(text_buf_t *b, const level_header_t *h)
{
    const level_start_t *st = &h->start;

    if (h->name[0] != '\0')
        put(b, "name %s\n", h->name);
    if (h->author[0] != '\0')
        put(b, "author %s\n", h->author);
    if (h->music[0] != '\0')
        put(b, "music %s\n", h->music);
    if (h->music_offset != 0.0)
        put(b, "music_offset %.10g\n", h->music_offset);
    if (h->bpm != 0.0)
        put(b, "bpm %.10g\n", h->bpm);
    if (h->first_beat != 0.0)
        put(b, "first_beat %.10g\n", h->first_beat);
    if (st->pos.x != PLAYER_SPAWN_X)
        put(b, "start_x %.10g\n", st->pos.x);
    if (st->pos.y != PLAYER_SPAWN_Y)
        put(b, "start_y %.10g\n", st->pos.y);
    if (st->mode != MODE_CUBE)
        put(b, "start_gamemode %s\n", MODES[st->mode].name);
    if (st->gravity_dir < 0)
        put(b, "start_gravity flipped\n");
    if (st->speed_mult != 1.0)
        put(b, "start_speed %.10g\n", st->speed_mult);
    if (st->mini)
        put(b, "start_size mini\n");
}

/* A portal's rect is derived from its cell when the line gives no w or h. */
static bool is_portal_shaped(const object_t *o)
{
    return o->type == OBJ_PORTAL || o->type == OBJ_GRAVITY;
}

/* The width or height a line without w= or h= gives this object (4.2). */
static double derived_w(const object_t *o)
{
    return o->size * UNIT * (is_portal_shaped(o) ? PORTAL_BOX_W : 1.0);
}

static double derived_h(const object_t *o)
{
    return o->size * UNIT * (is_portal_shaped(o) ? PORTAL_BOX_H : 1.0);
}

/*
** The x and y the line was written with. A portal's derived axis is centered
** on its cell, so the cell's corner is found back from the rect's center.
*/
static vec2_t written_pos(const object_t *o)
{
    double cell = o->size * UNIT;
    vec2_t at = {o->rect.x, o->rect.y};

    if (is_portal_shaped(o) && o->rect.w == derived_w(o))
        at.x = o->rect.x + o->rect.w / 2.0 - cell / 2.0;
    if (is_portal_shaped(o) && o->rect.h == derived_h(o))
        at.y = o->rect.y + o->rect.h / 2.0 - cell / 2.0;
    return at;
}

/* The word after the size: what kind of portal, pad or orb it is. */
static const char *object_word(const object_t *o)
{
    if (o->type == OBJ_PORTAL)
        return MODES[o->portal_mode].name;
    if (o->type == OBJ_GRAVITY)
        return o->portal_gravity < 0 ? "up" : "down";
    if (o->type == OBJ_PAD || o->type == OBJ_ORB)
        return LAUNCHES[o->launch].name;
    return NULL;
}

static void put_object(text_buf_t *b, const object_t *o, const char *extra)
{
    vec2_t at = written_pos(o);
    const char *word = object_word(o);

    put(b, "%s %.10g %.10g %d", obj_type_name(o->type), at.x, at.y, o->size);
    if (word != NULL)
        put(b, " %s", word);
    if (o->rotation != 0.0)
        put(b, " rot=%.10g", o->rotation);
    if (o->rect.w != derived_w(o))
        put(b, " w=%.10g", o->rect.w / UNIT);
    if (o->rect.h != derived_h(o))
        put(b, " h=%.10g", o->rect.h / UNIT);
    put(b, "%s\n", extra != NULL ? extra : "");
}

static const level_doc_t *sorting;

/* By x, then y, then the order they had: readable files, small diffs. */
static int by_position(const void *a, const void *b)
{
    size_t ia = *(const size_t *)a;
    size_t ib = *(const size_t *)b;
    vec2_t pa = written_pos(&sorting->objs[ia]);
    vec2_t pb = written_pos(&sorting->objs[ib]);

    if (pa.x != pb.x)
        return (pa.x > pb.x) - (pa.x < pb.x);
    if (pa.y != pb.y)
        return (pa.y > pb.y) - (pa.y < pb.y);
    return (ia > ib) - (ia < ib);
}

char *level_write_mem(const level_doc_t *doc, const level_header_t *hdr)
{
    text_buf_t b = {sim_xcalloc(64, 1), 0, 64};
    size_t *order = sim_xcalloc(doc->count + 1, sizeof(size_t));

    put_header(&b, hdr);
    if (b.len > 0 && doc->count > 0)
        put(&b, "\n");
    for (size_t i = 0; i < doc->count; i++)
        order[i] = i;
    sorting = doc;
    qsort(order, doc->count, sizeof(size_t), by_position);
    for (size_t i = 0; i < doc->count; i++)
        put_object(&b, &doc->objs[order[i]],
            doc->extras != NULL ? doc->extras[order[i]] : NULL);
    free(order);
    return b.text;
}

int level_write(const char *path, const level_doc_t *doc,
    const level_header_t *hdr)
{
    char tmp[512];
    char *text = level_write_mem(doc, hdr);
    size_t len = strlen(text);
    FILE *f;
    bool ok;

    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(tmp, "wb");
    if (f == NULL) {
        free(text);
        return -1;
    }
    ok = fwrite(text, 1, len, f) == len;
    ok = fclose(f) == 0 && ok;
    free(text);
    if (!ok || rename(tmp, path) != 0) {
        remove(tmp);
        return -1;
    }
    return 0;
}
