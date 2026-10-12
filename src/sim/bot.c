/*
** ALEXNEX PROJECT, 2026
** sim/bot.c
** File description:
** the bot: a depth-first search over inputs with the real engine (8.3)
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/alloc.h"
#include "sim/bot.h"
#include "sim/modes.h"
#include "sim/sim.h"

#define ORB_AHEAD_TICKS 12    /* how early an orb starts to matter: 50 ms */

typedef struct decision {
    uint64_t key;             /* sim_physics_hash when it was taken          */
    bool held;                /* the choice: the button until the next one   */
    bool was_held;            /* the button just before it: the press edge   */
    sim_snapshot_t snap;      /* the run state it was taken in               */
} decision_t;

typedef struct bot_search {
    sim_t *s;
    bot_result_t r;           /* so far: the attempts, the furthest run      */
    bool resume;              /* the next run starts on a decision made      */
    bool over;
    decision_t *path;         /* the current run's decisions, oldest first   */
    size_t depth;
    size_t cap;               /* decisions allocated, each with its snapshot */
    uint64_t *dead;           /* open addressing set of hashes, 0 is empty   */
    size_t dead_cap;          /* a power of two                              */
    size_t dead_count;
    bool held;                /* the button the next tick gets               */
    bool was_held;            /* the button the last tick got                */
} bot_search_t;

typedef bot_search_t bot_t;

/* The memo's slot for a hash: 0 marks an empty one, so 0 is stored as 1. */
static size_t dead_slot(const uint64_t *set, size_t cap, uint64_t *key)
{
    size_t i;

    if (*key == 0)
        *key = 1;
    i = *key & (cap - 1);
    while (set[i] != 0 && set[i] != *key)
        i = (i + 1) & (cap - 1);
    return i;
}

static bool dead_has(const bot_t *b, uint64_t key)
{
    return b->dead[dead_slot(b->dead, b->dead_cap, &key)] != 0;
}

static void dead_grow(bot_t *b)
{
    size_t cap = b->dead_cap * 2;
    uint64_t *set = sim_xcalloc(cap, sizeof(uint64_t));

    for (size_t i = 0; i < b->dead_cap; i++)
        if (b->dead[i] != 0)
            set[dead_slot(set, cap, &b->dead[i])] = b->dead[i];
    free(b->dead);
    b->dead = set;
    b->dead_cap = cap;
}

/* Both choices failed from this state: any later run reaching it stops. */
static void dead_add(bot_t *b, uint64_t key)
{
    size_t i;

    if ((b->dead_count + 1) * 2 > b->dead_cap)
        dead_grow(b);
    i = dead_slot(b->dead, b->dead_cap, &key);
    if (b->dead[i] == 0)
        b->dead_count += 1;
    b->dead[i] = key;
}

static void path_grow(bot_t *b)
{
    size_t cap = b->cap == 0 ? 1024 : b->cap * 2;
    decision_t *path = sim_xcalloc(cap, sizeof(decision_t));

    if (b->cap > 0)
        memcpy(path, b->path, b->cap * sizeof(decision_t));
    for (size_t i = b->cap; i < cap; i++)
        sim_snapshot_init(&path[i].snap, b->s);
    free(b->path);
    b->path = path;
    b->cap = cap;
}

/*
** A live orb the player is over, or reaches within ORB_AHEAD_TICKS: around
** one, every tick is a choice. An orb wants a fresh hold on the tick of the
** contact, so "release now, press on it" has to be a run the search can try,
** and a cube in the air has no other moment to choose in (FEATURES 10.2).
*/
static bool near_live_orb(const sim_t *s)
{
    const player_t *p = &s->st.player;
    double half = MODES[p->mode].half;
    double reach = p->pos.x + half + ORB_AHEAD_TICKS * p->vx;

    for (size_t i = s->st.first_active; i < s->lvl.nb_objects; i++) {
        const object_t *o = &s->lvl.objects[i];

        if (o->hitbox.aabb.x > reach)
            return false;                    /* sorted: nothing nearer follows */
        if (o->type == OBJ_ORB && !is_spent(&s->st, i)
            && o->hitbox.aabb.x + o->hitbox.aabb.w >= p->pos.x - half)
            return true;
    }
    return false;
}

/* The cube chooses when it can jump, flying modes every N ticks (6.1). */
static bool is_decision(const sim_t *s)
{
    const player_t *p = &s->st.player;
    int every = MODES[p->mode].bot_decision_ticks;

    if (near_live_orb(s))
        return true;
    if (every > 0)
        return s->st.tick % every == 0;
    return p->can_jump;
}

/* "Don't press" first. False when this state is already known to fail. */
static bool decide(bot_t *b)
{
    uint64_t key = sim_physics_hash(b->s);
    decision_t *d;

    if (dead_has(b, key))
        return false;
    if (b->depth == b->cap)
        path_grow(b);
    d = &b->path[b->depth];
    d->key = key;
    d->held = false;
    d->was_held = b->was_held;
    sim_snapshot_save(&d->snap, b->s);
    b->depth += 1;
    b->held = false;
    return true;
}

/*
** Ticks until the end, a death or a known dead state. A resumed run starts
** on a decision backtrack() already made, so it doesn't take it again.
*/
static bool run(bot_t *b, bool resume)
{
    sim_t *s = b->s;

    for (;;) {
        if (!resume && is_decision(s) && !decide(b))
            return false;
        resume = false;
        sim_tick(s, (input_t){b->held, b->held && !b->was_held});
        b->was_held = b->held;
        if (!s->st.player.alive)
            return false;
        if (s->st.complete)
            return true;
    }
}

/*
** Back to the most recent decision that hasn't tried "press", and press.
** The ones passed on the way failed both ways: their states are dead.
*/
static bool backtrack(bot_t *b)
{
    decision_t *d;

    while (b->depth > 0 && b->path[b->depth - 1].held) {
        dead_add(b, b->path[b->depth - 1].key);
        b->depth -= 1;
    }
    if (b->depth == 0)
        return false;
    d = &b->path[b->depth - 1];
    d->held = true;
    sim_snapshot_restore(b->s, &d->snap);
    b->was_held = d->was_held;
    b->held = true;
    return true;
}

/* The run that just ended, tick by tick: each decision holds until the next. */
static void record_inputs(const bot_t *b, bot_result_t *r)
{
    free(r->inputs);
    r->ticks = b->s->st.tick;
    r->inputs = sim_xcalloc(r->ticks + 1, sizeof(bool));
    for (size_t i = 0; i < b->depth; i++) {
        long from = b->path[i].snap.st.tick;
        long to = i + 1 < b->depth ? b->path[i + 1].snap.st.tick : r->ticks;

        for (long t = from; t < to; t++)
            r->inputs[t] = b->path[i].held;
    }
}

bot_search_t *bot_begin(sim_t *s)
{
    bot_t *b = sim_xcalloc(1, sizeof(bot_t));

    b->s = s;
    b->dead_cap = 1024;
    b->dead = sim_xcalloc(b->dead_cap, sizeof(uint64_t));
    b->r.verdict = BOT_GAVE_UP;
    b->r.furthest = -1.0f;
    sim_reset(s);
    return b;
}

/*
** One run, then back to the next decision to try. The furthest run so far
** is kept tick by tick: with no path, it is the one worth looking at.
*/
static void one_attempt(bot_t *b, long max_attempts)
{
    bool done = run(b, b->resume);
    float percent = sim_percent(b->s);

    b->r.attempts += 1;
    if (done || percent > b->r.furthest) {
        b->r.furthest = percent;
        record_inputs(b, &b->r);
    }
    b->resume = true;
    if (done)
        b->r.verdict = BOT_FOUND;
    else if (!backtrack(b))
        b->r.verdict = BOT_NO_PATH;
    b->over = done || b->r.verdict == BOT_NO_PATH
        || b->r.attempts >= max_attempts;
}

bool bot_step(bot_search_t *b, long attempts, long max_attempts)
{
    for (long i = 0; i < attempts && !b->over; i++)
        one_attempt(b, max_attempts);
    return b->over;
}

const bot_result_t *bot_progress(const bot_search_t *b)
{
    return &b->r;
}

bot_result_t bot_end(bot_search_t *b)
{
    bot_result_t r = b->r;

    for (size_t i = 0; i < b->cap; i++)
        sim_snapshot_free(&b->path[i].snap);
    free(b->path);
    free(b->dead);
    sim_reset(b->s);
    free(b);
    r.furthest = fmaxf(r.furthest, 0.0f);
    return r;
}

bot_result_t bot_solve(sim_t *s, long max_attempts)
{
    bot_search_t *b = bot_begin(s);

    bot_step(b, max_attempts, max_attempts);
    return bot_end(b);
}

void bot_result_free(bot_result_t *r)
{
    free(r->inputs);
    r->inputs = NULL;
}

void bot_describe(const bot_result_t *r, char *buf, size_t size)
{
    if (r->verdict == BOT_FOUND)
        snprintf(buf, size, "found a path (%ld attempts)", r->attempts);
    else if (r->verdict == BOT_NO_PATH)
        snprintf(buf, size, "no path found, furthest %.1f%% (%ld attempts)",
            r->furthest, r->attempts);
    else
        snprintf(buf, size, "gave up after %ld attempts, furthest %.1f%%",
            r->attempts, r->furthest);
}
