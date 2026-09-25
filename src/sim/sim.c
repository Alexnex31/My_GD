/*
** ALEXNEX PROJECT, 2026
** sim/sim.c
** File description:
** the simulation's run state: reset, tick, snapshots (3.3, 3.4)
*/

#include <stdlib.h>
#include <string.h>

#include "sim/alloc.h"
#include "sim/internal.h"
#include "sim/modes.h"
#include "sim/sim.h"

/* Every attempt starts here, the first one like every retry (3.4). */
void sim_reset(sim_t *s)
{
    run_state_t *st = &s->st;

    st->player = (player_t){
        .pos = {PLAYER_SPAWN_X, PLAYER_SPAWN_Y},
        .prev_pos = {PLAYER_SPAWN_X, PLAYER_SPAWN_Y},
        .vx = PER_TICK(SCROLL_SPEED),
        .gravity_dir = 1,
        .mode = MODE_CUBE,
        .grounded = true,
        .can_jump = true,
        .hold = HOLD_FRESH,              /* a hold carried into the attempt is fresh */
        .support_normal = {0.0, -1.0},   /* standing on the ground */
        .alive = true,
    };
    st->cam = (camera_t){{0.0, 0.0}};
    st->bounds = (ship_bounds_t){0};
    st->tick = 0;
    st->distance = 0.0;
    st->speed_mult = 1.0;
    st->first_active = 0;
    st->complete = false;
    memset(st->spent, 0, st->spent_words * sizeof(uint64_t));
}

/* The tick, in the order of 3.4: forces, movement, effects, camera. */
void sim_tick(sim_t *s, input_t in)
{
    run_state_t *st = &s->st;
    player_t *p = &st->player;

    if (!p->alive || st->complete)
        return;
    p->prev_pos = p->pos;
    player_update_hold(p, in);               /* 0. fresh / used / none          */
    player_apply_input(p, in);               /* 1. the jump, the ship's thrust  */
    player_apply_gravity(p);                 /* 2. gravity and the fall cap     */
    move_and_collide(s);                     /* 3. legs, contacts, deaths (4.4) */
    if (p->alive)
        collide_kill_ceiling(p, &s->lvl);    /* 4. flipped gravity only (4.7)   */
    if (p->alive)                            /* 5. interactive objects: step 6  */
        update_can_jump(s);                  /*    the jump zone, last (4.3)    */
    camera_follow(&st->cam, p, &st->bounds); /* 6.                              */
    st->tick += 1;
    if (p->alive && st->distance >= s->lvl.end_shift)
        st->complete = true;                 /* 7.                              */
    if (p->alive)
        player_update_rotation(p);           /* 8. the icon, cosmetic           */
}

float sim_percent(const sim_t *s)
{
    double pct = s->st.distance / s->lvl.end_shift * 100.0;

    return pct > 100.0 ? 100.0f : (float)pct;
}

void sim_snapshot_init(sim_snapshot_t *snap, const sim_t *s)
{
    snap->st = s->st;
    snap->st.spent = sim_xcalloc(s->st.spent_words + 1, sizeof(uint64_t));
}

void sim_snapshot_save(sim_snapshot_t *snap, const sim_t *s)
{
    uint64_t *buf = snap->st.spent;

    snap->st = s->st;
    snap->st.spent = buf;
    memcpy(buf, s->st.spent, s->st.spent_words * sizeof(uint64_t));
}

void sim_snapshot_restore(sim_t *s, const sim_snapshot_t *snap)
{
    uint64_t *buf = s->st.spent;

    s->st = snap->st;
    s->st.spent = buf;
    memcpy(buf, snap->st.spent, s->st.spent_words * sizeof(uint64_t));
}

void sim_snapshot_free(sim_snapshot_t *snap)
{
    free(snap->st.spent);
    snap->st.spent = NULL;
}
