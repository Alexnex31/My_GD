/*
** ALEXNEX PROJECT, 2026
** tests/test_mirror.c
** File description:
** a section turned upside down, played flipped, is the same run (FEATURES 9.6)
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sim/bot.h"
#include "sim/hitbox.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

/*
** The mirror line is y = 0 on purpose: negating a double is exact, so the
** mirrored run does the very same arithmetic with the signs turned, and the
** two can be compared bit for bit. Around any other line the coordinates
** round differently, and a fall that ends exactly on a floor (a ship coming
** back to the height it left) lands one tick apart in the two runs.
**
** Floors and ceilings, both kinds of slope, spikes both ways, a tilted
** block, gravity portals, pads on a floor and under a ceiling, orbs, and a
** mode portal on the line. Every corridor
** these modes open around y = 0 ends above the ground, so the ground plays
** no part and the mirror has nothing it lacks.
*/
#define SECTION \
    "start_y 0\n" \
    "block 1200 400 2\n" \
    "block 1800 -500 2 h=6\n" \
    "slope 2600 400 2\n" \
    "block 2700 400 2 w=6\n" \
    "pad 2800 300 2 pink\n" \
    "orb 3200 -50 2 red\n" \
    "orb 4200 100 2 green\n" \
    "spike 3400 400 2\n" \
    "spike 3900 -500 2 rot=180\n" \
    "block 4500 -50 2 rot=30\n" \
    "gravity 5200 -50 2 up\n" \
    "block 5800 100 2 w=8 h=1\n" \
    "slope 6400 -500 2 rot=90 w=4 h=4\n" \
    "spike 7900 0 2\n" \
    "gravity 8400 -50 2 down\n" \
    "block 9000 300 2 h=4\n" \
    "pad 9000 200 2 blue\n" \
    "orb 9400 -150 2 black\n" \
    "pad 9650 -300 2 yellow rot=180\n" \
    "block 9600 -500 2 h=4\n" \
    "slope 10200 350 2 w=6 h=3\n" \
    "block 11500 -50 2\n"

static void load(sim_t *s, const char *mode, const char *next)
{
    char text[1024];

    snprintf(text, sizeof(text), "start_gamemode %s\n" SECTION
        "portal 7000 -50 2 %s\n", mode, next);
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

/*
** Upside down around y = 0, at the hitbox level: every vertex, so it works
** for a slope too, which no rotation can mirror (a reflection isn't one).
** Rebuilding from the mirrored vertices recomputes order, faces and axes.
*/
static void mirror_object(object_t *o)
{
    vec2_t v[HB_MAX_VERTS];
    int n = o->hitbox.nverts;

    for (int i = 0; i < n; i++)
        v[i] = (vec2_t){o->hitbox.verts[i].x, -o->hitbox.verts[i].y};
    o->rect.y = -o->rect.y - o->rect.h;
    hitbox_build_poly(&o->hitbox, v, n, o->rect, 0.0);
    o->portal_gravity = -o->portal_gravity;
}

static void mirror(sim_t *s)
{
    for (size_t i = 0; i < s->lvl.nb_objects; i++)
        mirror_object(&s->lvl.objects[i]);
    s->lvl.hdr.start.pos.y = -s->lvl.hdr.start.pos.y;
    s->lvl.hdr.start.gravity_dir = -s->lvl.hdr.start.gravity_dir;
    s->lvl.kill_y = -1e9;     /* inside a corridor nothing reaches it (4.7) */
    sim_reset(s);
}

/* Bit for bit: nothing here is a tolerance. */
static bool same_run(const sim_t *a, const sim_t *b)
{
    const player_t *p = &a->st.player;
    const player_t *q = &b->st.player;

    return p->alive == q->alive && p->mode == q->mode
        && p->gravity_dir == -q->gravity_dir
        && p->grounded == q->grounded && p->can_jump == q->can_jump
        && p->hold == q->hold && a->st.complete == b->st.complete
        && p->pos.x == q->pos.x && p->pos.y == -q->pos.y
        && p->vy == q->vy && p->surface_rise == q->surface_rise
        && a->st.bounds.active == b->st.bounds.active
        && a->st.bounds.top == -b->st.bounds.bottom
        && a->st.bounds.bottom == -b->st.bounds.top;
}

static long pads_used;
static long orbs_used;

/* The runs must meet what the section holds, or agreeing on it says nothing. */
static void count_launches(const sim_t *s)
{
    for (size_t i = 0; i < s->lvl.nb_objects; i++) {
        if (!is_spent(&s->st, i))
            continue;
        pads_used += s->lvl.objects[i].type == OBJ_PAD;
        orbs_used += s->lvl.objects[i].type == OBJ_ORB;
    }
}

/* The button changes every 4 to 67 ticks, from a seed: the same for both. */
static bool button(unsigned *rng, int *left, bool down)
{
    if (*left > 0) {
        *left -= 1;
        return down;
    }
    *rng = *rng * 1664525u + 1013904223u;
    *left = 4 + (int)((*rng >> 16) % 64);
    return !down;
}

/* One seed: both runs tick for tick, until they end. The ticks they lasted. */
static long play_both(sim_t *a, sim_t *b, unsigned seed)
{
    unsigned rng = seed;
    int left = 0;
    bool down = (seed & 1u) != 0;
    bool prev = false;

    sim_reset(a);
    sim_reset(b);
    while (a->st.player.alive && !a->st.complete && a->st.tick < 5000) {
        down = button(&rng, &left, down);
        sim_tick(a, (input_t){down, down && !prev});
        sim_tick(b, (input_t){down, down && !prev});
        prev = down;
        if (!same_run(a, b)) {
            fprintf(stderr, "mirror: seed %u differs at tick %ld\n", seed,
                a->st.tick);
            return -1;
        }
    }
    return a->st.tick;
}

/* The mirror itself: a floor block is a ceiling block, a start is a start. */
static void test_the_mirror(void)
{
    sim_t a;
    sim_t b;

    load(&a, "ship", "ufo");
    load(&b, "ship", "ufo");
    mirror(&b);
    CHECK(a.lvl.nb_objects == 23 && b.lvl.nb_objects == 23);
    CHECK(b.lvl.objects[0].rect.y == -500.0 && a.lvl.objects[0].rect.y == 400.0);
    CHECK(b.lvl.objects[0].hitbox.aabb.y == -500.0);
    CHECK(b.lvl.objects[0].hitbox.aabb.h == 100.0);
    CHECK(b.st.player.gravity_dir == -1 && b.st.player.pos.y == 0.0);
    CHECK(a.st.bounds.top == -500.0 && a.st.bounds.bottom == 500.0);
    CHECK(b.st.bounds.top == -500.0 && b.st.bounds.bottom == 500.0);
    for (size_t i = 0; i < a.lvl.nb_objects; i++) {
        CHECK(a.lvl.objects[i].hitbox.aabb.x == b.lvl.objects[i].hitbox.aabb.x);
        CHECK(a.lvl.objects[i].hitbox.nverts == b.lvl.objects[i].hitbox.nverts);
        CHECK(a.lvl.objects[i].hitbox.naxes == b.lvl.objects[i].hitbox.naxes);
    }
    sim_free(&a);
    sim_free(&b);
}

/*
** Every mode with a corridor, 200 seeds each: the two runs agree on every
** tick, deaths included. The seeds must also show something: some die early,
** some get past the portals.
*/
static void test_mode(const char *mode, const char *next)
{
    sim_t a;
    sim_t b;
    long shortest = 100000;
    long longest = 0;
    long ticks;

    load(&a, mode, next);
    load(&b, mode, next);
    mirror(&b);
    for (unsigned seed = 1; seed <= 200; seed++) {
        ticks = play_both(&a, &b, seed);
        CHECK(ticks > 0);
        count_launches(&a);
        if (ticks < 0)
            break;
        shortest = ticks < shortest ? ticks : shortest;
        longest = ticks > longest ? ticks : longest;
    }
    CHECK(shortest < longest && longest > 800);
    sim_free(&a);
    sim_free(&b);
}

/* The bot sees the same level: same verdict after the same search. */
static void test_bot_agrees(const char *mode, const char *next)
{
    sim_t a;
    sim_t b;
    bot_result_t ra;
    bot_result_t rb;

    load(&a, mode, next);
    load(&b, mode, next);
    mirror(&b);
    ra = bot_solve(&a, 20000);
    rb = bot_solve(&b, 20000);
    CHECK(ra.verdict == rb.verdict && ra.attempts == rb.attempts);
    CHECK(ra.ticks == rb.ticks && ra.furthest == rb.furthest);
    bot_result_free(&ra);
    bot_result_free(&rb);
    sim_free(&a);
    sim_free(&b);
}

void test_mirror(void)
{
    test_the_mirror();
    test_mode("ship", "ufo");
    test_mode("ufo", "ball");
    test_mode("ball", "wave");
    test_mode("wave", "ship");
    CHECK(pads_used > 20 && orbs_used > 20);
    test_bot_agrees("ship", "ufo");
    test_bot_agrees("ball", "wave");
}
