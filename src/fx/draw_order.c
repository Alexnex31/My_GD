/*
** ALEXNEX PROJECT, 2026
** fx/draw_order.c
** File description:
** which objects are drawn, in what order, and which flash (9.2, 11.15)
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "fx/draw_order.h"
#include "sim/alloc.h"
#include "sim/hitbox.h"

static const object_t *g_sorting;

static int by_draw_order(const void *a, const void *b)
{
    const object_t *oa = &g_sorting[*(const size_t *)a];
    const object_t *ob = &g_sorting[*(const size_t *)b];

    return object_draw_cmp(oa, oa->line, ob, ob->line);
}

static int by_rank(const void *a, const void *b)
{
    size_t ra = *(const size_t *)a;
    size_t rb = *(const size_t *)b;

    return (ra > rb) - (ra < rb);
}

static size_t chunk_index(const object_t *o)
{
    rect_t d = object_drawn_bounds(o);
    double x = d.x < 0.0 ? 0.0 : d.x;

    return (size_t)(x / DRAW_CHUNK_W);
}

/* Each chunk sized first, so its list is allocated once; filled by rank,
** so it is already in the draw order. */
static void build_chunks(draw_index_t *d, const object_t *objs, size_t n)
{
    size_t *filled = sim_xcalloc(d->nb_chunks, sizeof(size_t));

    for (size_t i = 0; i < n; i++)
        d->chunks[chunk_index(&objs[i])].count += 1;
    for (size_t c = 0; c < d->nb_chunks; c++)
        d->chunks[c].ranks = sim_xcalloc(d->chunks[c].count + 1,
            sizeof(size_t));
    for (size_t k = 0; k < n; k++) {
        const object_t *o = &objs[d->by_rank[k]];
        draw_chunk_t *ch = &d->chunks[chunk_index(o)];
        rect_t b = object_drawn_bounds(o);

        ch->ranks[filled[ch - d->chunks]++] = k;
        ch->left = fmin(ch->left, b.x);
        ch->right = fmax(ch->right, b.x + b.w);
    }
    free(filled);
}

/*
** Objects never move (Principle 1): their place in the draw order is found
** once per level, and so are the chunks.
*/
void draw_index_build(draw_index_t *d, const object_t *objs, size_t n)
{
    double right = 0.0;

    *d = (draw_index_t){0};
    d->by_rank = sim_xcalloc(n + 1, sizeof(size_t));
    d->visible = sim_xcalloc(n + 1, sizeof(size_t));
    for (size_t i = 0; i < n; i++) {
        rect_t b = object_drawn_bounds(&objs[i]);

        d->by_rank[i] = i;
        right = fmax(right, b.x + b.w);
    }
    g_sorting = objs;
    qsort(d->by_rank, n, sizeof(size_t), by_draw_order);
    d->nb_chunks = (size_t)(right / DRAW_CHUNK_W) + 1;
    d->chunks = sim_xcalloc(d->nb_chunks, sizeof(draw_chunk_t));
    for (size_t c = 0; c < d->nb_chunks; c++)
        d->chunks[c] = (draw_chunk_t){INFINITY, -INFINITY, NULL, 0};
    build_chunks(d, objs, n);
}

void draw_index_gather(draw_index_t *d, const object_t *objs, double left,
    double right)
{
    d->nb_visible = 0;
    for (size_t c = 0; c < d->nb_chunks; c++) {
        const draw_chunk_t *ch = &d->chunks[c];

        if (ch->right < left || ch->left > right)
            continue;
        memcpy(d->visible + d->nb_visible, ch->ranks,
            ch->count * sizeof(size_t));
        d->nb_visible += ch->count;
    }
    qsort(d->visible, d->nb_visible, sizeof(size_t), by_rank);
    d->nb_behind = 0;
    while (d->nb_behind < d->nb_visible
        && objs[d->by_rank[d->visible[d->nb_behind]]].z_layer < Z_T1)
        d->nb_behind += 1;
}

void draw_index_free(draw_index_t *d)
{
    for (size_t c = 0; c < d->nb_chunks; c++)
        free(d->chunks[c].ranks);
    free(d->chunks);
    free(d->by_rank);
    free(d->visible);
    *d = (draw_index_t){0};
}

void flash_forget(uint64_t *seen, const run_state_t *st)
{
    if (st->spent_words > 0)
        memcpy(seen, st->spent, st->spent_words * sizeof(uint64_t));
}

/* The bitsets compared word by word: nothing per object. */
size_t flash_fresh(uint64_t *seen, const run_state_t *st, size_t *out,
    size_t max)
{
    size_t n = 0;

    for (size_t w = 0; w < st->spent_words; w++) {
        uint64_t fresh = st->spent[w] & ~seen[w];

        for (int b = 0; fresh != 0 && b < 64; b++)
            if (fresh & ((uint64_t)1 << b)) {
                if (n < max)
                    out[n++] = w * 64 + (size_t)b;
                fresh &= ~((uint64_t)1 << b);
            }
        seen[w] = st->spent[w];
    }
    return n;
}
