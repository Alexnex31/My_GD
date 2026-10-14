/*
** ALEXNEX PROJECT, 2026
** sim/speed.c
** File description:
** GD's five speeds, and how long a level takes when it changes speed (FEATURES 10.3)
*/

#include <stdlib.h>

#include "sim/alloc.h"
#include "sim/constants.h"
#include "sim/speed.h"

const double SPEED_NAMES[SPEED_COUNT] = {0.5, 1.0, 2.0, 3.0, 4.0};

static const double SPEED_PX_S[SPEED_COUNT] = {SPEED_SLOW, SPEED_NORMAL,
    SPEED_FAST, SPEED_VFAST, SPEED_XFAST};

bool speed_is_known(double name)
{
    for (int i = 0; i < SPEED_COUNT; i++)
        if (SPEED_NAMES[i] == name)
            return true;
    return false;
}

double speed_px_per_s(double name)
{
    for (int i = 0; i < SPEED_COUNT; i++)
        if (SPEED_NAMES[i] == name)
            return SPEED_PX_S[i];
    return SPEED_NORMAL;
}

const char *speed_word(double name)
{
    static const char *const WORDS[SPEED_COUNT] = {"0.5", "1", "2", "3", "4"};

    for (int i = 0; i < SPEED_COUNT; i++)
        if (SPEED_NAMES[i] == name)
            return WORDS[i];
    return "1";
}

double speed_step(double name)
{
    return PER_TICK(speed_px_per_s(name));
}

void speed_map_add(speed_map_t *m, const object_t *o)
{
    speed_change_t *bigger;

    if (o->type != OBJ_SPEED)
        return;
    if (m->n == m->cap) {
        m->cap = m->cap == 0 ? 8 : m->cap * 2;
        bigger = sim_xcalloc(m->cap, sizeof(speed_change_t));
        for (size_t i = 0; i < m->n; i++)
            bigger[i] = m->at[i];
        free(m->at);
        m->at = bigger;
    }
    m->at[m->n] = (speed_change_t){o->hitbox.aabb.x - PLAYER_HALF,
        o->hitbox.aabb.x + o->hitbox.aabb.w, speed_step(o->speed), m->n};
    m->n += 1;
}

/*
** By x, then in the order they were added: the caller adds them in the
** level's object order, so of two portals at the same x the later one acts
** last, as the run touches them (4.6). Never qsort's own order.
*/
static int by_x(const void *a, const void *b)
{
    const speed_change_t *ca = a;
    const speed_change_t *cb = b;

    if (ca->x != cb->x)
        return (ca->x > cb->x) - (ca->x < cb->x);
    return (ca->order > cb->order) - (ca->order < cb->order);
}

void speed_map_sort(speed_map_t *m)
{
    if (m->n > 1)
        qsort(m->at, m->n, sizeof(speed_change_t), by_x);
}

void speed_map_free(speed_map_t *m)
{
    free(m->at);
    *m = (speed_map_t){0};
}

/*
** The changes a run meets, in order: `k` is the next one to look at. A
** portal wholly behind the start is never met; one the player starts in
** acts at once, so the speed is its own from the start.
*/
static bool next_change(const speed_map_t *m, const level_start_t *start,
    size_t *k, double *step, const speed_change_t **out)
{
    while (*k < m->n) {
        const speed_change_t *c = &m->at[*k];

        *k += 1;
        if (c->right < start->pos.x - PLAYER_HALF)
            continue;
        if (c->x <= start->pos.x) {
            *step = c->step;
            continue;
        }
        *out = c;
        return true;
    }
    return false;
}

double speed_map_ticks(const speed_map_t *m, const level_start_t *start,
    double x)
{
    const speed_change_t *c = NULL;
    double step = speed_step(start->speed);
    double from = start->pos.x;
    double ticks = 0.0;
    size_t k = 0;

    while (next_change(m, start, &k, &step, &c) && c->x < x) {
        ticks += (c->x - from) / step;
        from = c->x;
        step = c->step;
    }
    return ticks + (x - from) / step;
}

double speed_map_x(const speed_map_t *m, const level_start_t *start,
    double ticks)
{
    const speed_change_t *c = NULL;
    double step = speed_step(start->speed);
    double from = start->pos.x;
    size_t k = 0;

    while (next_change(m, start, &k, &step, &c)
        && (c->x - from) / step < ticks) {
        ticks -= (c->x - from) / step;
        from = c->x;
        step = c->step;
    }
    return from + ticks * step;
}
