/*
** ALEXNEX PROJECT, 2026
** sim/progress.c
** File description:
** attempts and best per level, saved once per visit (6.2, 6.4)
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sim/alloc.h"
#include "sim/level.h"
#include "sim/progress.h"

#define LINE_MAX_LEN 1024

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

static bool parse_float(const char *s, float *out)
{
    char *end = NULL;
    float v = strtof(s, &end);

    if (end == s || *end != '\0')
        return false;
    *out = v;
    return true;
}

static bool parse_ull(const char *s, unsigned long long *out, int base)
{
    char *end = NULL;
    unsigned long long v = strtoull(s, &end, base);

    if (end == s || *end != '\0')
        return false;
    *out = v;
    return true;
}

/* One "key=value" of a line; anything unknown is kept for the next save. */
static void entry_field(progress_entry_t *e, char *field)
{
    char *value = strchr(field, '=');
    unsigned long long n = 0;
    float f = 0.0f;

    if (value == NULL || value == field)
        return;                              /* not a field at all: dropped */
    *value = '\0';
    value += 1;
    if (strcmp(field, "attempts") == 0 && parse_ull(value, &n, 10))
        e->attempts = (int)n;
    else if (strcmp(field, "best") == 0 && parse_float(value, &f))
        e->best = f;
    else if (strcmp(field, "practice_best") == 0 && parse_float(value, &f))
        e->practice_best = f;
    else if (strcmp(field, "hash") == 0 && parse_ull(value, &n, 16))
        e->level_hash = (uint64_t)n;
    else if (strcmp(field, "song") == 0)
        snprintf(e->song, sizeof(e->song), "%s", value);
    else {
        *(value - 1) = '=';
        keep_extra(e, field);
    }
}

static void parse_line(progress_t *p, char *line)
{
    char *save = NULL;
    char *tok = strtok_r(line, " \t", &save);
    progress_entry_t *e = NULL;

    if (tok == NULL)
        return;
    e = progress_get(p, tok);
    if (e == NULL)
        return;                              /* not a level id: line ignored */
    for (tok = strtok_r(NULL, " \t", &save); tok != NULL;
        tok = strtok_r(NULL, " \t", &save))
        entry_field(e, tok);
}

int progress_load(progress_t *p, const char *path)
{
    char line[LINE_MAX_LEN];
    FILE *f = NULL;

    *p = (progress_t){0};
    snprintf(p->path, sizeof(p->path), "%s", path);
    f = fopen(path, "r");
    if (f == NULL)
        return 0;                            /* no file yet: an empty store */
    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '\0' && line[0] != '#')
            parse_line(p, line);
    }
    fclose(f);
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

/* The store's own folder, whatever its path: "save" for SAVE_PATH. */
static int make_parent_dir(const char *path)
{
    char dir[sizeof(((progress_t *)0)->path)];
    char *slash = NULL;

    snprintf(dir, sizeof(dir), "%s", path);
    slash = strrchr(dir, '/');
    if (slash == NULL || slash == dir)
        return 0;                            /* the working directory, or / */
    *slash = '\0';
    return mkdir(dir, 0755) != 0 && errno != EEXIST ? -1 : 0;
}

/*
** Atomic: the new content is complete and on disk before the rename makes it
** visible, so a crash or a power cut leaves the old store intact (6.4).
*/
int progress_save(const progress_t *p)
{
    char tmp[sizeof(p->path) + 8];
    FILE *f = NULL;

    if (make_parent_dir(p->path) != 0)
        return -1;
    snprintf(tmp, sizeof(tmp), "%s.tmp", p->path);
    f = fopen(tmp, "w");
    if (f == NULL)
        return -1;
    for (size_t i = 0; i < p->count; i++)
        write_entry(f, &p->entries[i]);
    if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
        fclose(f);
        return (void)remove(tmp), -1;
    }
    if (fclose(f) != 0)                      /* write errors surface here */
        return (void)remove(tmp), -1;
    return rename(tmp, p->path) == 0 ? 0 : (remove(tmp), -1);
}

void progress_free(progress_t *p)
{
    for (size_t i = 0; i < p->count; i++)
        free(p->entries[i].extra);
    free(p->entries);
    *p = (progress_t){0};
}
