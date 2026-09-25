/*
** ALEXNEX PROJECT, 2026
** sim/hash.c
** File description:
** FNV-1a over the run state: determinism tests and the bot's memo (G.11)
*/

#include <string.h>

#include "sim/sim.h"

#define FNV_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

static void hash_bytes(uint64_t *h, const void *p, size_t n)
{
    const uint8_t *b = p;

    for (size_t i = 0; i < n; i++) {
        *h ^= b[i];
        *h *= FNV_PRIME;
    }
}

static void hash_double(uint64_t *h, double v)
{
    if (v == 0.0)
        v = 0.0;                             /* -0.0 is the same state as 0.0 */
    hash_bytes(h, &v, sizeof v);
}

static void hash_long(uint64_t *h, long v)
{
    hash_bytes(h, &v, sizeof v);
}

static void hash_player(uint64_t *h, const player_t *p)
{
    hash_double(h, p->pos.x);
    hash_double(h, p->pos.y);
    hash_double(h, p->vx);
    hash_double(h, p->vy);
    hash_double(h, p->surface_rise);
    hash_double(h, p->support_normal.x);
    hash_double(h, p->support_normal.y);
    hash_long(h, p->gravity_dir);
    hash_long(h, p->mode);
    hash_long(h, p->hold);
    hash_long(h, p->grounded);
    hash_long(h, p->can_jump);
    hash_long(h, p->alive);
}

/* Everything that decides what happens next; the icon's rotation never does. */
static uint64_t hash_state(const sim_t *s, bool with_camera)
{
    const run_state_t *st = &s->st;
    uint64_t h = FNV_BASIS;

    hash_long(&h, st->tick);
    hash_double(&h, st->distance);
    hash_double(&h, st->speed_mult);
    hash_player(&h, &st->player);
    hash_long(&h, st->bounds.active);
    hash_double(&h, st->bounds.top);
    hash_double(&h, st->bounds.bottom);
    hash_long(&h, (long)st->first_active);
    hash_long(&h, st->complete);
    hash_bytes(&h, st->spent, st->spent_words * sizeof(uint64_t));
    if (with_camera) {
        hash_double(&h, st->cam.pos.x);
        hash_double(&h, st->cam.pos.y);
    }
    return h;
}

uint64_t sim_state_hash(const sim_t *s)
{
    return hash_state(s, true);
}

uint64_t sim_physics_hash(const sim_t *s)
{
    return hash_state(s, false);             /* the camera decides nothing (3.5) */
}
