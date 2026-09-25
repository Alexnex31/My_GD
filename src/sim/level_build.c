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

#include "sim/alloc.h"
#include "sim/level.h"
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
    double top = GROUND_Y - CORRIDOR_MAX_HEIGHT;

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
        lvl->end_shift = fmax(right + LEVEL_END_PADDING, 100.0);
    lvl->kill_y = top - KILL_CEILING_MARGIN;
}

int sim_load_mem(sim_t *s, const char *buf, size_t len, const char *id,
    sim_log_fn log)
{
    memset(s, 0, sizeof(*s));
    snprintf(s->lvl.id, sizeof(s->lvl.id), "%s", id);
    s->lvl.hdr.version = 2;                  /* the current format (7.2) */
    snprintf(s->lvl.hdr.name, sizeof(s->lvl.hdr.name), "%s", id);
    level_parse_mem(buf, len, id, &s->lvl.objects, &s->lvl.nb_objects,
        &s->lvl.hdr, log);
    level_finalize(&s->lvl);
    s->st.spent_words = (s->lvl.nb_objects + 63) / 64;
    s->st.spent = sim_xcalloc(s->st.spent_words + 1, sizeof(uint64_t));
    sim_reset(s);
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

static char *read_whole_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    char *buf = NULL;
    long size = 0;

    if (f == NULL)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = sim_xcalloc((size_t)size + 1, 1);
    *len = fread(buf, 1, (size_t)size, f);
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
    free(s->st.spent);
    memset(s, 0, sizeof(*s));
}
