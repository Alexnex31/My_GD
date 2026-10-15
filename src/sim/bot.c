/*
** ALEXNEX PROJECT, 2026
** sim/bot.c
** File description:
** the bot: a depth-first search over the button with the real engine (8.3)
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/alloc.h"
#include "sim/bot.h"
#include "sim/level.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "sim/speed.h"

#define ORB_AHEAD_TICKS 12    /* how early an orb starts to matter: 50 ms   */
#define ROBOT_BOT_TICKS 4     /* a rising robot may let go every 4 ticks ...  */
#define ROBOT_LOW_TICKS 4     /* ... and on each of its first: the low hops */
#define CHECK_TICKS 12        /* a snapshot this close behind a decision ... */
#define CHECK_BYTES (64L << 20)   /* ... unless a run's would pass this    */
#define CELL_Y 2.0            /* px: closer than this, two states are one   */
#define CELL_V 0.25           /* px per tick, the same for speeds           */
#define SEEN_MAX (1L << 21)   /* states remembered at most: 64 MB           */

typedef struct decision {
    long tick;                /* when it was taken: ticks only go up        */
    bool held;                /* the choice, until the next decision        */
    bool was_held;            /* the button the tick before: the press edge */
    bool tap;                 /* the press lasts its own tick only          */
    bool tried;               /* the other choice has been taken too        */
} decision_t;

typedef struct checkpoint {
    size_t at;                /* the decision whose state it is             */
    sim_snapshot_t snap;
} checkpoint_t;

typedef struct state_key {    /* two hashes: a collision is 2^-128          */
    uint64_t a;
    uint64_t b;
} state_key_t;

struct bot_search {
    sim_t *s;
    bot_result_t r;
    bool over;
    bool replaying;           /* back from a checkpoint to the decision     */
    bool resume;              /* the run goes on from a decision taken      */
    size_t cur;               /* the decision in force while replaying      */
    bool was_held;            /* the button the last tick got               */
    decision_t *path;         /* the run's decisions, oldest first          */
    size_t depth;
    size_t cap;
    checkpoint_t *ckpt;       /* a few of them, their states                */
    size_t nb_ckpt;
    size_t made_ckpt;         /* slots whose snapshot is allocated          */
    size_t cap_ckpt;
    state_key_t *seen;        /* open addressing; {0, 0} is empty           */
    size_t seen_cap;          /* a power of two                             */
    size_t seen_count;
    long check_ticks;         /* between two checkpoints of a run           */
};

/* The button on tick `t` under decision `d`: a tap only on its own. */
static bool button(const decision_t *d, long t)
{
    return d->held && (!d->tap || t == d->tick);
}

static uint64_t mix(uint64_t z)
{
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static int64_t cell(double v, double size)
{
    return (int64_t)floor(v / size);
}

static int64_t bits(double v)
{
    int64_t out;

    if (v == 0.0)
        v = 0.0;                             /* -0.0 is 0.0 */
    memcpy(&out, &v, sizeof out);
    return out;
}

/*
** What decides the rest of the run, in cells. The height is counted from
** the floor's side (y grows down), so a level and its mirror merge the same
** states and search the same way.
*/
static state_key_t state_key(const struct bot_search *b)
{
    const run_state_t *st = &b->s->st;
    const player_t *p = &st->player;
    int64_t f[] = {st->tick, cell(p->pos.x, CELL_Y),
        cell(-p->pos.y * p->gravity_dir, CELL_Y), cell(p->vy, CELL_V),
        cell(p->surface_rise, CELL_V), bits(p->vx), bits(st->speed_mult),
        p->gravity_dir, p->mode, p->hold, p->boost_ticks, p->mini,
        p->grounded, p->can_jump, b->was_held, st->bounds.active,
        st->bounds.active ? bits(st->bounds.top) : 0,
        st->bounds.active ? bits(st->bounds.bottom) : 0,
        (int64_t)st->spent_hash};
    state_key_t k = {fnv1a(f, sizeof f), 0};

    for (size_t i = 0; i < sizeof f / sizeof f[0]; i++)
        k.b = mix(k.b ^ (uint64_t)f[i]) + i;
    if (k.a == 0 && k.b == 0)
        k.b = 1;
    return k;
}

static size_t seen_slot(const state_key_t *set, size_t cap, state_key_t k)
{
    size_t i = k.a & (cap - 1);

    while ((set[i].a != 0 || set[i].b != 0)
        && (set[i].a != k.a || set[i].b != k.b))
        i = (i + 1) & (cap - 1);
    return i;
}

static void seen_grow(struct bot_search *b)
{
    size_t cap = b->seen_cap * 2;
    state_key_t *set = sim_xcalloc(cap, sizeof(state_key_t));

    for (size_t i = 0; i < b->seen_cap; i++)
        if (b->seen[i].a != 0 || b->seen[i].b != 0)
            set[seen_slot(set, cap, b->seen[i])] = b->seen[i];
    free(b->seen);
    b->seen = set;
    b->seen_cap = cap;
}

/*
** False when a run already came through this cell: it failed from there,
** or it would have ended the search. Past SEEN_MAX nothing more is
** remembered; the search stays right, only slower.
*/
static bool remember(struct bot_search *b, state_key_t k)
{
    size_t i = seen_slot(b->seen, b->seen_cap, k);

    if (b->seen[i].a != 0 || b->seen[i].b != 0)
        return false;
    if (b->seen_count >= (size_t)SEEN_MAX)
        return true;
    b->seen[i] = k;
    b->seen_count += 1;
    if (b->seen_count * 2 > b->seen_cap)
        seen_grow(b);
    return true;
}

/*
** A live orb this mode can use, that the player is over or reaches within
** ORB_AHEAD_TICKS, across and up or down: around one every tick is a
** choice. An orb wants a fresh hold on the tick of the contact, so "release
** now, press on it" has to be a run the search can try (FEATURES 10.2).
*/
static bool near_live_orb(const sim_t *s)
{
    const player_t *p = &s->st.player;
    const mode_ops_t *m = player_mode(p);
    double speed = fmax(fmax(fabs(p->vy), m->max_fall),
        fmax(fabs(m->jump_v), 2.0 * p->vx));
    double up_down = m->half + ORB_AHEAD_TICKS * speed
        + m->gravity * ORB_AHEAD_TICKS * ORB_AHEAD_TICKS / 2.0;
    double reach = p->pos.x + m->half + ORB_AHEAD_TICKS * p->vx;

    for (size_t i = s->st.first_active; i < s->lvl.nb_objects; i++) {
        const object_t *o = &s->lvl.objects[i];
        const rect_t *box = &o->hitbox.aabb;

        if (box->x > reach)
            return false;                    /* sorted: nothing nearer follows */
        if (o->type == OBJ_ORB && !is_spent(&s->st, i)
            && (!m->orbs_flip_only || o->launch == LAUNCH_BLUE)
            && box->x + box->w >= p->pos.x - m->half
            && box->y <= p->pos.y + up_down
            && box->y + box->h >= p->pos.y - up_down)
            return true;
    }
    return false;
}

/*
** The cube chooses when it can jump, the flying modes every N ticks (6.1),
** a rising robot on each of its first ticks (the low hops) then every few.
*/
static bool is_decision(const sim_t *s)
{
    const player_t *p = &s->st.player;
    const mode_ops_t *m = player_mode(p);
    int risen = m->rise_ticks - p->boost_ticks;

    if (near_live_orb(s))
        return true;
    if (m->bot_decision_ticks > 0)
        return s->st.tick % m->bot_decision_ticks == 0;
    if (p->boost_ticks > 0)
        return risen <= ROBOT_LOW_TICKS || risen % ROBOT_BOT_TICKS == 0;
    return p->can_jump;
}

static void path_grow(struct bot_search *b)
{
    size_t cap = b->cap == 0 ? 256 : b->cap * 2;
    decision_t *path = sim_xcalloc(cap, sizeof(decision_t));

    if (b->depth > 0)
        memcpy(path, b->path, b->depth * sizeof(decision_t));
    free(b->path);
    b->path = path;
    b->cap = cap;
}

/*
** The decision just pushed gets a snapshot when the last one is
** check_ticks old: memory goes with the level's length, not with its
** decisions, and a backtrack replays about that many ticks.
*/
static void maybe_checkpoint(struct bot_search *b)
{
    checkpoint_t *bigger = NULL;
    long tick = b->s->st.tick;

    if (b->nb_ckpt > 0
        && tick - b->path[b->ckpt[b->nb_ckpt - 1].at].tick < b->check_ticks)
        return;
    if (b->nb_ckpt == b->cap_ckpt) {
        b->cap_ckpt = b->cap_ckpt == 0 ? 16 : b->cap_ckpt * 2;
        bigger = sim_xcalloc(b->cap_ckpt, sizeof(checkpoint_t));
        if (b->made_ckpt > 0)
            memcpy(bigger, b->ckpt, b->made_ckpt * sizeof(checkpoint_t));
        free(b->ckpt);
        b->ckpt = bigger;
    }
    if (b->nb_ckpt == b->made_ckpt) {
        sim_snapshot_init(&b->ckpt[b->nb_ckpt].snap, b->s);
        b->made_ckpt += 1;
    }
    b->ckpt[b->nb_ckpt].at = b->depth - 1;
    sim_snapshot_save(&b->ckpt[b->nb_ckpt].snap, b->s);
    b->nb_ckpt += 1;
}

/*
** The button as it is first: a held ship goes on holding, a cube that
** jumped goes on jumping. A tap mode's first choice is no press, and right
** after a tap it has no other. False when this state's cell was seen.
*/
static bool decide(struct bot_search *b)
{
    bool tap = player_mode(&b->s->st.player)->bot_taps;

    if (!remember(b, state_key(b)))
        return false;
    if (b->depth == b->cap)
        path_grow(b);
    b->path[b->depth] = (decision_t){b->s->st.tick, tap ? false : b->was_held,
        b->was_held, tap, tap && b->was_held};
    b->depth += 1;
    maybe_checkpoint(b);
    return true;
}

static void play_tick(struct bot_search *b, bool held)
{
    sim_tick(b->s, (input_t){held, held && !b->was_held});
    b->was_held = held;
    b->r.work += 1;
}

/* The run as it is now, tick by tick: each decision holds until the next. */
static void record_inputs(struct bot_search *b)
{
    bot_result_t *r = &b->r;
    size_t i = 0;

    free(r->inputs);
    r->ticks = b->s->st.tick;
    r->inputs = sim_xcalloc((size_t)r->ticks + 1, sizeof(bool));
    for (long t = 0; t < r->ticks && b->depth > 0; t++) {
        while (i + 1 < b->depth && b->path[i + 1].tick <= t)
            i += 1;
        r->inputs[t] = t >= b->path[i].tick && button(&b->path[i], t);
    }
}

/*
** Back to the most recent decision with a choice left, the other choice
** taken. Its state comes from the checkpoint before it and a replay.
*/
static bool backtrack(struct bot_search *b)
{
    decision_t *d;
    const checkpoint_t *c;

    while (b->depth > 0 && b->path[b->depth - 1].tried)
        b->depth -= 1;
    if (b->depth == 0)
        return false;
    d = &b->path[b->depth - 1];
    d->tried = true;
    d->held = !d->held;
    while (b->ckpt[b->nb_ckpt - 1].at >= b->depth)
        b->nb_ckpt -= 1;                     /* the first decision keeps one */
    c = &b->ckpt[b->nb_ckpt - 1];
    sim_snapshot_restore(b->s, &c->snap);
    b->was_held = b->path[c->at].was_held;
    b->cur = c->at;
    b->replaying = true;
    return true;
}

/*
** A run is over. The furthest so far is kept tick by tick: with no path,
** it is the one worth looking at.
*/
static void end_run(struct bot_search *b, bool done)
{
    float percent = sim_percent(b->s);

    b->r.attempts += 1;
    if (done || b->r.inputs == NULL || percent > b->r.furthest) {
        b->r.furthest = percent;
        record_inputs(b);
    }
    if (done)
        b->r.verdict = BOT_FOUND;
    else if (!backtrack(b))
        b->r.verdict = BOT_NO_PATH;
    b->over = b->r.verdict != BOT_SEARCHING;
}

static void replay_tick(struct bot_search *b)
{
    long t = b->s->st.tick;

    if (t == b->path[b->depth - 1].tick) {
        b->replaying = false;
        b->resume = true;                    /* decided already */
        return;
    }
    while (b->cur + 1 < b->depth && b->path[b->cur + 1].tick <= t)
        b->cur += 1;
    play_tick(b, button(&b->path[b->cur], t));
}

static void run_tick(struct bot_search *b)
{
    sim_t *s = b->s;

    if (!b->resume && is_decision(s) && !decide(b))
        return end_run(b, false);
    b->resume = false;
    play_tick(b, b->depth > 0 && button(&b->path[b->depth - 1], s->st.tick));
    if (!s->st.player.alive)
        end_run(b, false);
    else if (s->st.complete)
        end_run(b, true);
}

/*
** Checkpoints CHECK_TICKS apart, or further when a run's snapshots would
** pass CHECK_BYTES: a level of a million objects stays in memory.
*/
static long check_ticks(const sim_t *s)
{
    const level_start_t *start = &s->lvl.hdr.start;
    double ticks = speed_map_ticks(&s->lvl.speeds, start,
        start->pos.x + s->lvl.end_shift);
    double bytes = (double)sizeof(sim_snapshot_t)
        + (double)s->st.spent_words * sizeof(uint64_t);
    double spacing = ceil(ticks * bytes / (double)CHECK_BYTES);

    return spacing > CHECK_TICKS ? (long)spacing : CHECK_TICKS;
}

bot_search_t *bot_begin(sim_t *s)
{
    bot_search_t *b = sim_xcalloc(1, sizeof(bot_search_t));

    b->s = s;
    b->check_ticks = check_ticks(s);
    b->seen_cap = 4096;
    b->seen = sim_xcalloc(b->seen_cap, sizeof(state_key_t));
    b->r.verdict = BOT_SEARCHING;
    sim_reset(s);
    return b;
}

/*
** A tick at a time, so a window can stop it anywhere: a run, a replay to a
** decision, all the same to the budget.
*/
bool bot_step(bot_search_t *b, long ticks, long max_ticks)
{
    long until = b->r.work + ticks;

    while (!b->over && b->r.work < until) {
        if (b->r.work >= max_ticks) {
            b->r.verdict = BOT_GAVE_UP;
            b->over = true;
        } else if (b->replaying)
            replay_tick(b);
        else
            run_tick(b);
    }
    if (!b->over && b->r.work >= max_ticks) {
        b->r.verdict = BOT_GAVE_UP;
        b->over = true;
    }
    return b->over;
}

const bot_result_t *bot_progress(const bot_search_t *b)
{
    return &b->r;
}

/* Stopped before its end, a search is undecided. */
bot_result_t bot_end(bot_search_t *b)
{
    bot_result_t r = b->r;

    for (size_t i = 0; i < b->made_ckpt; i++)
        sim_snapshot_free(&b->ckpt[i].snap);
    free(b->ckpt);
    free(b->path);
    free(b->seen);
    sim_reset(b->s);
    free(b);
    if (r.verdict == BOT_SEARCHING)
        r.verdict = BOT_GAVE_UP;
    return r;
}

bot_result_t bot_solve(sim_t *s, long max_ticks)
{
    bot_search_t *b = bot_begin(s);

    bot_step(b, max_ticks, max_ticks);
    return bot_end(b);
}

void bot_result_free(bot_result_t *r)
{
    free(r->inputs);
    r->inputs = NULL;
}

input_t bot_input_at(const bot_result_t *r, long t)
{
    bool held = r->inputs[t];

    return (input_t){held, held && (t == 0 || !r->inputs[t - 1])};
}

bool bot_replays(sim_t *s, const bot_result_t *r)
{
    sim_reset(s);
    for (long t = 0; t < r->ticks; t++) {
        if (!s->st.player.alive || s->st.complete)
            return false;
        sim_tick(s, bot_input_at(r, t));
    }
    return s->st.player.alive && s->st.complete;
}

void bot_describe(const bot_result_t *r, char *buf, size_t size)
{
    if (r->verdict == BOT_FOUND)
        snprintf(buf, size, "found a path (%ld attempts)", r->attempts);
    else if (r->verdict == BOT_NO_PATH)
        snprintf(buf, size, "no path found, furthest %.1f%% (%ld attempts;"
            " it can miss one)", r->furthest, r->attempts);
    else
        snprintf(buf, size, "undecided: gave up after %ld attempts, furthest"
            " %.1f%%", r->attempts, r->furthest);
}
