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

/* The player the level's start_ fields describe (7.2). */
static player_t start_player(const level_start_t *start)
{
    return (player_t){
        .pos = start->pos,
        .prev_pos = start->pos,
        .vx = PER_TICK(SCROLL_SPEED) * start->speed_mult,
        .gravity_dir = start->gravity_dir,
        .mode = start->mode,
        .hold = HOLD_FRESH,              /* a hold carried into the attempt is fresh */
        .support_normal = {0.0, (double)-start->gravity_dir},
        .alive = true,
    };
}

/*
** Standing on a floor at the first tick: the ground, or the corridor's floor
** when the attempt starts inside one. Anywhere else the attempt starts in the
** air, and the jump zone of the first tick decides from there (4.3).
*/
static bool starts_on_the_floor(const sim_t *s)
{
    const player_t *p = &s->st.player;
    const ship_bounds_t *b = &s->st.bounds;
    double feet = p->pos.y + MODES[p->mode].half * p->gravity_dir;

    if (b->active)
        return feet == (p->gravity_dir > 0 ? b->bottom : b->top);
    return p->gravity_dir > 0 && feet == GROUND_Y;
}

/* Every attempt starts here, the first one like every retry (3.4, 7.2). */
void sim_reset(sim_t *s)
{
    run_state_t *st = &s->st;
    const level_start_t *start = &s->lvl.hdr.start;

    st->player = start_player(start);
    st->cam = (camera_t){{0.0, 0.0}};
    st->bounds = (ship_bounds_t){0};
    st->tick = 0;
    st->distance = 0.0;
    st->speed_mult = start->speed_mult;
    st->first_active = 0;
    st->complete = false;
    memset(st->spent, 0, st->spent_words * sizeof(uint64_t));
    if (MODES[start->mode].corridor_height > 0.0)
        corridor_from_center(s, start->pos.y);
    st->player.grounded = starts_on_the_floor(s);
    st->player.can_jump = st->player.grounded;
    st->cam.pos.y = camera_rest_y(&st->player, &st->bounds);
}

/*
** The tick, in the order of 3.4: forces, movement, effects, camera. The mode
** is read again at each step: step 5 can change it (a portal), and that must
** only reach the next tick's physics (FEATURES 6.3).
*/
void sim_tick(sim_t *s, input_t in)
{
    run_state_t *st = &s->st;
    player_t *p = &st->player;

    if (!p->alive || st->complete)
        return;
    p->prev_pos = p->pos;
    player_update_hold(p, in);               /* 0. fresh / used / none          */
    MODES[p->mode].apply_input(p, in);       /* 1. the mode's impulses          */
    MODES[p->mode].apply_forces(p);          /* 2. gravity and the fall cap     */
    move_and_collide(s);                     /* 3. legs, contacts, deaths (4.4) */
    if (p->alive)
        collide_kill_ceiling(p, &s->lvl);    /* 4. flipped gravity only (4.7)   */
    if (p->alive)
        apply_interactive(s);                /* 5. portals, in contact order    */
    if (p->alive)
        update_can_jump(s);                  /*    the jump zone, last (4.3)    */
    camera_follow(&st->cam, p, &st->bounds); /* 6.                              */
    st->tick += 1;
    if (p->alive && st->distance >= s->lvl.end_shift)
        st->complete = true;                 /* 7.                              */
    if (p->alive)
        MODES[p->mode].update_rotation(p);   /* 8. the icon, cosmetic           */
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
