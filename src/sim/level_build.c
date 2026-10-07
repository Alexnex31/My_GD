/*
** ALEXNEX PROJECT, 2026
** sim/level_build.c
** File description:
** turns parsed objects into level data, and loads a level file (4.1, 7.3)
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "sim/alloc.h"
#include "sim/level.h"
#include "sim/modes.h"
#include "sim/sim.h"

/* By hitbox left edge, then by source line: the same order on every libc. */
static int cmp_object(const void *a, const void *b)
{
    double xa = ((const object_t *)a)->hitbox.aabb.x;
    double xb = ((const object_t *)b)->hitbox.aabb.x;
    int la = ((const object_t *)a)->line;
    int lb = ((const object_t *)b)->line;

    if (xa != xb)
        return (xa > xb) - (xa < xb);
    return (la > lb) - (la < lb);
}

void level_finalize(level_data_t *lvl)
{
    double right = 0.0;
    double top = GROUND_Y - modes_tallest_corridor();   /* read, never assumed */

    qsort(lvl->objects, lvl->nb_objects, sizeof(object_t), cmp_object);
    lvl->reach = 0.0;
    for (size_t i = 0; i < lvl->nb_objects; i++) {
        rect_t box = lvl->objects[i].hitbox.aabb;

        lvl->reach = fmax(lvl->reach, box.w);
        right = fmax(right, box.x + box.w);
        top = fmin(top, box.y);
    }
    lvl->end_shift = 100.0;              /* an empty level is legal, and short */
    if (lvl->nb_objects > 0)
        lvl->end_shift = fmax(right + LEVEL_END_PADDING
            - lvl->hdr.start.pos.x, 100.0);   /* the run is measured from the spawn */
    lvl->kill_y = top - KILL_CEILING_MARGIN;
}

/* The defaults the start_ fields override (7.2); the name is the id. */
void level_header_defaults(level_header_t *hdr, const char *id)
{
    *hdr = (level_header_t){0};
    snprintf(hdr->name, sizeof(hdr->name), "%s", id);
    hdr->start = (level_start_t){
        .pos = {PLAYER_SPAWN_X, PLAYER_SPAWN_Y}, .mode = MODE_CUBE,
        .speed_mult = 1.0, .gravity_dir = 1};
}

/* The level is in s->lvl: sort it, measure it, and stand at its start. */
static void sim_ready(sim_t *s)
{
    level_finalize(&s->lvl);
    s->st.spent_words = (s->lvl.nb_objects + 63) / 64;
    s->st.spent = sim_xcalloc(s->st.spent_words + 1, sizeof(uint64_t));
    sim_reset(s);
}

int sim_load_mem(sim_t *s, const char *buf, size_t len, const char *id,
    sim_log_fn log)
{
    level_doc_t doc;

    memset(s, 0, sizeof(*s));
    snprintf(s->lvl.id, sizeof(s->lvl.id), "%s", id);
    level_header_defaults(&s->lvl.hdr, id);
    s->lvl.file_hash = fnv1a(buf, len);  /* the version, from the bytes read */
    s->lvl.skipped_lines = level_parse_doc(buf, len, id, &doc, &s->lvl.hdr,
        log);
    s->lvl.objects = doc.objs;           /* the sim keeps these two arrays */
    s->lvl.nb_objects = doc.count;
    s->lvl.starts = doc.starts;
    s->lvl.nb_starts = doc.nb_starts;
    doc.objs = NULL;
    doc.starts = NULL;
    level_doc_free(&doc);
    if (s->lvl.hdr.start.mini && log != NULL)
        log("start_size mini has no effect yet: the mini scale is FEATURES 10.4");
    sim_ready(s);
    return 0;
}

/*
** The same level from a document in memory: what the editor playtests and
** verifies is built exactly like what the game loads (FEATURES 11.1). The
** objects are numbered in the document's order, which decides ties in x.
*/
int sim_init(sim_t *s, const level_doc_t *doc, const level_header_t *hdr,
    const char *id)
{
    memset(s, 0, sizeof(*s));
    snprintf(s->lvl.id, sizeof(s->lvl.id), "%s", id);
    s->lvl.hdr = *hdr;
    s->lvl.objects = sim_xcalloc(doc->count + 1, sizeof(object_t));
    s->lvl.nb_objects = doc->count;
    for (size_t i = 0; i < doc->count; i++) {
        s->lvl.objects[i] = doc->objs[i];
        s->lvl.objects[i].line = (int)i + 1;
    }
    s->lvl.starts = sim_xcalloc(doc->nb_starts + 1, sizeof(level_start_t));
    s->lvl.nb_starts = doc->nb_starts;
    if (doc->nb_starts > 0)
        memcpy(s->lvl.starts, doc->starts,
            doc->nb_starts * sizeof(level_start_t));
    sim_ready(s);
    return 0;
}

static char *read_whole_file(const char *path, size_t *len);

int level_read_header(const char *path, level_header_t *hdr,
    uint64_t *file_hash)
{
    char id[LEVEL_ID_MAX + 1] = "";
    size_t len = 0;
    char *text = read_whole_file(path, &len);

    if (text == NULL)
        return -1;
    level_id_from_path(path, id, sizeof(id));
    level_header_defaults(hdr, id);
    if (file_hash != NULL)
        *file_hash = fnv1a(text, len);
    level_parse_mem(text, len, id, NULL, NULL, hdr, NULL);   /* no object */
    free(text);
    return 0;
}

/* "levels/10280.gd" -> "10280": the id is the file name, digits only (7.2). */
bool level_id_from_path(const char *path, char *id, size_t size)
{
    const char *slash = strrchr(path, '/');
    const char *dot;
    size_t n;

    if (slash != NULL)
        path = slash + 1;
    dot = strrchr(path, '.');
    if (dot == NULL || strcmp(dot, ".gd") != 0)
        return false;
    n = (size_t)(dot - path);
    if (n == 0 || n > LEVEL_ID_MAX || n >= size)
        return false;
    for (size_t i = 0; i < n; i++)
        if (path[i] < '0' || path[i] > '9')
            return false;
    memcpy(id, path, n);
    id[n] = '\0';
    return true;
}

/*
** Regular files only: fopen also opens a directory, and ftell on one can
** report LLONG_MAX, an allocation that ends the game.
*/
static char *read_whole_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    struct stat st;
    char *buf = NULL;

    if (f == NULL)
        return NULL;
    if (fstat(fileno(f), &st) != 0 || !S_ISREG(st.st_mode)) {
        fclose(f);
        return NULL;
    }
    buf = sim_xcalloc((size_t)st.st_size + 1, 1);
    *len = fread(buf, 1, (size_t)st.st_size, f);
    fclose(f);
    return buf;
}

int sim_load(sim_t *s, const char *path, sim_log_fn log)
{
    char id[24];
    size_t len = 0;
    char *buf = NULL;
    int ret;

    memset(s, 0, sizeof(*s));
    if (!level_id_from_path(path, id, sizeof(id))) {
        if (log != NULL)
            log("a level file is named <digits>.gd");
        return -1;
    }
    buf = read_whole_file(path, &len);
    if (buf == NULL) {
        if (log != NULL)
            log("level file cannot be read");
        return -1;
    }
    ret = sim_load_mem(s, buf, len, id, log);
    free(buf);
    return ret;
}

void sim_free(sim_t *s)
{
    free(s->lvl.objects);
    free(s->lvl.starts);
    free(s->st.spent);
    memset(s, 0, sizeof(*s));
}
