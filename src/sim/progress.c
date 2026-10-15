/*
** ALEXNEX PROJECT, 2026
** sim/progress.c
** File description:
** attempts and best per level, saved once per visit (6.2, 6.4)
*/

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/alloc.h"
#include "sim/level.h"
#include "sim/progress.h"
#include "sim/save_file.h"

bool progress_valid_id(const char *id)
{
    size_t n = 0;

    if (id == NULL)
        return false;
    for (n = 0; id[n] != '\0'; n++)
        if (id[n] < '0' || id[n] > '9')
            return false;
    return n > 0 && n <= LEVEL_ID_MAX;
}

static progress_entry_t *entry_append(progress_t *p, const char *id)
{
    progress_entry_t *e = NULL;

    if (p->count == p->cap) {
        p->cap = p->cap == 0 ? 8 : p->cap * 2;
        e = sim_xcalloc(p->cap, sizeof(*e));
        if (p->entries != NULL)
            memcpy(e, p->entries, p->count * sizeof(*e));
        free(p->entries);
        p->entries = e;
    }
    e = &p->entries[p->count];
    *e = (progress_entry_t){0};
    snprintf(e->id, sizeof(e->id), "%s", id);
    p->count += 1;
    return e;
}

progress_entry_t *progress_find(const progress_t *p, const char *id)
{
    if (!progress_valid_id(id))
        return NULL;
    for (size_t i = 0; i < p->count; i++)
        if (strcmp(p->entries[i].id, id) == 0)
            return &p->entries[i];
    return NULL;
}

progress_entry_t *progress_get(progress_t *p, const char *id)
{
    progress_entry_t *e = progress_find(p, id);

    if (e != NULL || !progress_valid_id(id))
        return e;
    return entry_append(p, id);
}

float progress_printable(float pct)
{
    return pct < 100.0f && pct > 99.99f ? 99.99f : pct;
}

/* An unknown field is kept verbatim, space separated, for the next save. */
static void keep_extra(progress_entry_t *e, const char *field)
{
    size_t had = e->extra == NULL ? 0 : strlen(e->extra);
    char *grown = sim_xcalloc(had + strlen(field) + 2, 1);

    if (had > 0)
        snprintf(grown, had + strlen(field) + 2, "%s %s", e->extra, field);
    else
        snprintf(grown, strlen(field) + 1, "%s", field);
    free(e->extra);
    e->extra = grown;
}

/* A percentage: 0 to 100, nothing else (no nan, it could never be beaten). */
static bool parse_percent(const char *s, float *out)
{
    char *end = NULL;
    float v = strtof(s, &end);

    if (end == s || *end != '\0' || !(v >= 0.0f && v <= 100.0f))
        return false;
    *out = v;
    return true;
}

/* Digits only, no sign, no space, at most `max`. */
static bool parse_count(const char *s, int base, unsigned long long max,
    unsigned long long *out)
{
    char *end = NULL;
    unsigned long long v = 0;

    if (!isxdigit((unsigned char)s[0]) || (base == 10 && !isdigit(
        (unsigned char)s[0])))
        return false;
    errno = 0;
    v = strtoull(s, &end, base);
    if (*end != '\0' || errno != 0 || v > max)
        return false;
    *out = v;
    return true;
}

/*
** One field of a line. A known key with a bad value is dropped, its default
** stays; anything unknown is kept for the next save.
*/
static void entry_field(progress_entry_t *e, char *field)
{
    char *value = strchr(field, '=');
    unsigned long long n = 0;

    if (value == NULL)
        return keep_extra(e, field);
    *value = '\0';
    value += 1;
    if (strcmp(field, "attempts") == 0) {
        if (parse_count(value, 10, INT_MAX, &n))
            e->attempts = (int)n;
    } else if (strcmp(field, "best") == 0)
        parse_percent(value, &e->best);
    else if (strcmp(field, "practice_best") == 0)
        parse_percent(value, &e->practice_best);
    else if (strcmp(field, "hash") == 0) {
        if (parse_count(value, 16, UINT64_MAX, &n))
            e->level_hash = (uint64_t)n;
    } else if (strcmp(field, "song") == 0)
        snprintf(e->song, sizeof(e->song), "%s", value);
    else {
        *(value - 1) = '=';
        keep_extra(e, field);
    }
}

static void keep_line(progress_t *p, const char *line)
{
    char **grown = sim_xcalloc(p->nb_kept + 1, sizeof(char *));

    if (p->kept != NULL)
        memcpy(grown, p->kept, p->nb_kept * sizeof(char *));
    free(p->kept);
    p->kept = grown;
    p->kept[p->nb_kept] = sim_xstrdup(line);
    p->nb_kept += 1;
}

/*
** "<id> key=value ...". A line that doesn't start with a level id of ours,
** a comment among them, is kept as it is: an older build never destroys a
** newer one's lines (6.4).
*/
static void parse_line(progress_t *p, char *line)
{
    size_t at = strspn(line, " \t");
    size_t n = strcspn(line + at, " \t");
    char id[sizeof(p->entries->id)];
    char *save = NULL;
    progress_entry_t *e = NULL;

    if (n == 0)
        return;                              /* a blank line */
    snprintf(id, sizeof(id), "%.*s", (int)(n < sizeof(id) ? n : 0),
        line + at);
    if (!progress_valid_id(id))
        return keep_line(p, line);
    e = progress_get(p, id);
    for (char *tok = strtok_r(line + at + n, " \t", &save); tok != NULL;
        tok = strtok_r(NULL, " \t", &save))
        entry_field(e, tok);
}

int progress_load(progress_t *p, const char *path)
{
    char *text = NULL;
    char *cursor = NULL;
    char *line = NULL;

    *p = (progress_t){0};
    snprintf(p->path, sizeof(p->path), "%s", path);
    if (save_read(path, &text) == SAVE_UNREADABLE) {
        p->unreadable = true;
        return -1;
    }
    cursor = text;                           /* NULL: no file, an empty store */
    while ((line = save_next_line(&cursor)) != NULL)
        parse_line(p, line);
    free(text);
    return 0;
}

/* Nothing but its id: nothing to keep (an older build's lookups made those). */
static bool entry_empty(const progress_entry_t *e)
{
    return e->attempts == 0 && e->best == 0.0f && e->practice_best == 0.0f
        && e->level_hash == 0 && e->song[0] == '\0' && e->extra == NULL;
}

static void write_entry(FILE *f, const progress_entry_t *e)
{
    if (entry_empty(e))
        return;
    fprintf(f, "%s attempts=%d best=%.2f", e->id, e->attempts,
        progress_printable(e->best));
    if (e->practice_best > 0.0f)
        fprintf(f, " practice_best=%.2f",
            progress_printable(e->practice_best));
    if (e->level_hash != 0)
        fprintf(f, " hash=%016llx", (unsigned long long)e->level_hash);
    if (e->song[0] != '\0')
        fprintf(f, " song=%s", e->song);
    if (e->extra != NULL)
        fprintf(f, " %s", e->extra);
    fprintf(f, "\n");
}

static void write_store(FILE *f, const void *data)
{
    const progress_t *p = data;

    for (size_t i = 0; i < p->nb_kept; i++)
        fprintf(f, "%s\n", p->kept[i]);
    for (size_t i = 0; i < p->count; i++)
        write_entry(f, &p->entries[i]);
}

/* Atomic: a crash or a power cut leaves the old store intact (6.4). */
int progress_save(const progress_t *p)
{
    if (p->unreadable)
        return -1;
    return save_atomic(p->path, write_store, p);
}

static void write_text(FILE *f, const void *data)
{
    fputs(data, f);
}

/*
** The store's file as it is now, to path.bak. No file, or nothing in it,
** has nothing to keep: the last .bak stays, so a second reset never
** replaces the real progress with an empty file.
*/
static int backup(const progress_t *p)
{
    char bak[sizeof(p->path) + 8];
    char *text = NULL;
    save_read_t got = save_read(p->path, &text);
    int ok = 0;

    if (got == SAVE_UNREADABLE)
        return -1;
    if (got == SAVE_READ && text[strspn(text, " \t\r\n")] != '\0') {
        snprintf(bak, sizeof(bak), "%s.bak", p->path);
        ok = save_atomic(bak, write_text, text);
    }
    free(text);
    return ok;
}

/*
** Every attempt, best and song choice gone, the old file kept as .bak
** (FEATURES 5.6). The empty store is saved before the old one is let go:
** whatever fails, the store is still what the file holds.
*/
int progress_reset(progress_t *p)
{
    progress_t empty = {0};

    if (p->unreadable || backup(p) != 0)
        return -1;
    snprintf(empty.path, sizeof(empty.path), "%s", p->path);
    if (progress_save(&empty) != 0)
        return -1;
    progress_free(p);
    *p = empty;
    return 0;
}

void progress_free(progress_t *p)
{
    for (size_t i = 0; i < p->count; i++)
        free(p->entries[i].extra);
    free(p->entries);
    for (size_t i = 0; i < p->nb_kept; i++)
        free(p->kept[i]);
    free(p->kept);
    *p = (progress_t){0};
}
