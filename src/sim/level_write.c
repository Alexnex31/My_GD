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
#include <unistd.h>

#include "sim/alloc.h"
#include "sim/level.h"
#include "sim/modes.h"
#include "sim/speed.h"

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

/*
** The shortest text that reads back as exactly v: a hand-tuned 1000.123456789
** stays itself, and 0.1 is still written 0.1 (7.2).
*/
static const char *num(char *out, double v)
{
    for (int digits = 10; digits <= 17; digits++) {
        snprintf(out, 32, "%.*g", digits, v);
        if (strtod(out, NULL) == v)
            break;
    }
    return out;
}

/*
** A width or height in grid units that reads back as exactly px: the reader
** multiplies by 50, so the units written are the double whose product is px,
** found next to px / 50 when that one rounds the other way.
*/
static const char *units(char *out, double px)
{
    double u = px / UNIT;
    double down = u;
    double up = u;

    for (int i = 0; i < 8 && u * UNIT != px; i++) {
        down = nextafter(down, -INFINITY);
        up = nextafter(up, INFINITY);
        u = down * UNIT == px ? down : up * UNIT == px ? up : u;
    }
    return num(out, u);
}

static bool is_trim(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/*
** A text field as the reader will take it back: up to a '#', which starts a
** comment (7.2), without the blanks around it, \r included (written last on
** its line, it would be read as the line's end). Empty: nothing to write.
*/
void level_clean_text(char *text)
{
    size_t len;
    size_t lead = 0;
    char *hash = strchr(text, '#');

    if (hash != NULL)
        *hash = '\0';
    while (is_trim(text[lead]))
        lead += 1;
    len = strlen(text + lead);
    memmove(text, text + lead, len + 1);
    while (len > 0 && is_trim(text[len - 1]))
        len -= 1;
    text[len] = '\0';
}

static void put_text(text_buf_t *b, const char *key, const char *value)
{
    char clean[256];

    snprintf(clean, sizeof(clean), "%s", value);
    level_clean_text(clean);
    if (clean[0] != '\0')
        put(b, "%s %s\n", key, clean);
}

/* Only what differs from the defaults a file without it gets (7.2). */
static void put_header(text_buf_t *b, const level_header_t *h)
{
    const level_start_t *st = &h->start;
    char n[32];

    put_text(b, "name", h->name);
    put_text(b, "author", h->author);
    put_text(b, "music", h->music);
    if (h->music_offset != 0.0)
        put(b, "music_offset %s\n", num(n, h->music_offset));
    if (h->bpm != 0.0)
        put(b, "bpm %s\n", num(n, h->bpm));
    if (h->first_beat != 0.0)
        put(b, "first_beat %s\n", num(n, h->first_beat));
    if (st->pos.x != PLAYER_SPAWN_X)
        put(b, "start_x %s\n", num(n, st->pos.x));
    if (st->pos.y != PLAYER_SPAWN_Y)
        put(b, "start_y %s\n", num(n, st->pos.y));
    if (st->mode != MODE_CUBE)
        put(b, "start_gamemode %s\n", MODES[st->mode].name);
    if (st->gravity_dir < 0)
        put(b, "start_gravity flipped\n");
    if (st->speed != 1.0)
        put(b, "start_speed %s\n", speed_word(st->speed));
    if (st->mini)
        put(b, "start_size mini\n");
}

/* What the reader couldn't use, as it was, after what it could (7.2). */
static void put_kept(text_buf_t *b, const level_doc_t *doc)
{
    for (size_t i = 0; doc->kept != NULL && i < doc->nb_kept; i++) {
        put(b, "%s", doc->kept[i]);
        put(b, "\n");
    }
}

static void put_starts(text_buf_t *b, const level_doc_t *doc)
{
    char x[32];
    char y[32];

    for (size_t i = 0; i < doc->nb_starts; i++) {
        const level_start_t *st = &doc->starts[i];

        put(b, "start %s %s %s%s", num(x, st->pos.x), num(y, st->pos.y),
            MODES[st->mode].name, st->gravity_dir < 0 ? " up" : "");
        if (st->speed != 1.0)
            put(b, " %s", speed_word(st->speed));
        if (st->mini)
            put(b, " mini");
        put(b, "\n");
    }
}

/* A portal's rect is derived from its cell when the line gives no w or h. */
static bool is_portal_shaped(const object_t *o)
{
    return o->type == OBJ_PORTAL || o->type == OBJ_GRAVITY
        || o->type == OBJ_SPEED || o->type == OBJ_MINI;
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
    if (o->type == OBJ_SPEED)
        return speed_word(o->speed);
    if (o->type == OBJ_MINI)
        return o->portal_mini ? "on" : "off";
    return NULL;
}

static void put_object(text_buf_t *b, const object_t *o, const char *extra)
{
    vec2_t at = written_pos(o);
    const char *word = object_word(o);
    char x[32];
    char y[32];

    put(b, "%s %s %s %d", obj_type_name(o->type), num(x, at.x), num(y, at.y),
        o->size);
    if (word != NULL)
        put(b, " %s", word);
    if (o->rotation != 0.0)
        put(b, " rot=%s", num(x, o->rotation));
    if (o->rect.w != derived_w(o))
        put(b, " w=%s", units(x, o->rect.w));
    if (o->rect.h != derived_h(o))
        put(b, " h=%s", units(x, o->rect.h));
    put(b, "%s\n", extra != NULL ? extra : "");
}

typedef struct placed {
    vec2_t at;
    size_t index;
} placed_t;

/* By x, then y, then the order they had: readable files, small diffs. */
static int by_position(const void *a, const void *b)
{
    const placed_t *pa = a;
    const placed_t *pb = b;

    if (pa->at.x != pb->at.x)
        return (pa->at.x > pb->at.x) - (pa->at.x < pb->at.x);
    if (pa->at.y != pb->at.y)
        return (pa->at.y > pb->at.y) - (pa->at.y < pb->at.y);
    return (pa->index > pb->index) - (pa->index < pb->index);
}

void level_write_order(const level_doc_t *doc, size_t *order)
{
    placed_t *p = sim_xcalloc(doc->count + 1, sizeof(placed_t));

    for (size_t i = 0; i < doc->count; i++)
        p[i] = (placed_t){written_pos(&doc->objs[i]), i};
    qsort(p, doc->count, sizeof(placed_t), by_position);
    for (size_t i = 0; i < doc->count; i++)
        order[i] = p[i].index;
    free(p);
}

char *level_write_mem(const level_doc_t *doc, const level_header_t *hdr)
{
    text_buf_t b = {sim_xcalloc(64, 1), 0, 64};
    size_t *order = sim_xcalloc(doc->count + 1, sizeof(size_t));

    put_header(&b, hdr);
    put_kept(&b, doc);
    put_starts(&b, doc);
    if (b.len > 0 && doc->count > 0)
        put(&b, "\n");
    level_write_order(doc, order);
    for (size_t i = 0; i < doc->count; i++)
        put_object(&b, &doc->objs[order[i]],
            doc->extras != NULL ? doc->extras[order[i]] : NULL);
    free(order);
    return b.text;
}

/*
** The text on disk before the name points at it: written, flushed and
** synced, so the rename never publishes a file the disk doesn't hold yet.
*/
static bool write_all(const char *tmp, const char *text, size_t len)
{
    FILE *f = fopen(tmp, "wb");
    bool ok;

    if (f == NULL)
        return false;
    ok = fwrite(text, 1, len, f) == len;
    ok = fflush(f) == 0 && ok;
    ok = fsync(fileno(f)) == 0 && ok;
    return fclose(f) == 0 && ok;
}

int level_write(const char *path, const level_doc_t *doc,
    const level_header_t *hdr)
{
    char *tmp = sim_xcalloc(strlen(path) + 5, 1);
    char *text = level_write_mem(doc, hdr);
    bool ok;

    memcpy(tmp, path, strlen(path));
    memcpy(tmp + strlen(path), ".tmp", 4);     /* never cut: its own size */
    ok = write_all(tmp, text, strlen(text));
    free(text);
    if (!ok || rename(tmp, path) != 0) {
        remove(tmp);
        free(tmp);
        return -1;
    }
    free(tmp);
    return 0;
}
