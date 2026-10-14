/*
** ALEXNEX PROJECT, 2026
** sim/sim.c
** File description:
** the simulation's run state: reset, tick, snapshots (3.3, 3.4)
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sim/alloc.h"
#include "sim/internal.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "sim/speed.h"

/* The player the level's start_ fields describe (7.2). */
static player_t start_player(const level_start_t *start)
{
    return (player_t){
        .pos = start->pos,
        .prev_pos = start->pos,
        .vx = speed_step(start->speed),
        .gravity_dir = start->gravity_dir,
        .mode = start->mode,
        .mini = start->mini,
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
    double feet = p->pos.y + player_mode(p)->half * p->gravity_dir;

    if (b->active)
        return feet == (p->gravity_dir > 0 ? b->bottom : b->top);
    return p->gravity_dir > 0 && feet == GROUND_Y;
}

/*
** A start position is somewhere along the level: the run is put at the tick
** a run from the level's own start reaches that x at, through the level's
** speed portals, rounded down, so the player stays on the tick grid and the
** music knows where it is (FEATURES 11.8, 10.3).
*/
static void place_along(sim_t *s, const level_start_t *start)
{
    const level_start_t *own = &s->lvl.hdr.start;
    run_state_t *st = &s->st;

    if (start->pos.x <= own->pos.x)
        return;
    st->tick = (long)floor(speed_map_ticks(&s->lvl.speeds, own,
        start->pos.x));
    st->distance = speed_map_x(&s->lvl.speeds, own, (double)st->tick)
        - own->pos.x;
    st->player.pos.x = own->pos.x + st->distance;
    st->player.prev_pos = st->player.pos;
}

/* What the run would have gone through before this start has acted. */
static void spend_what_is_behind(sim_t *s)
{
    const player_t *p = &s->st.player;
    double back = p->pos.x - player_mode(p)->half;

    for (size_t i = 0; i < s->lvl.nb_objects; i++) {
        const object_t *o = &s->lvl.objects[i];

        if (OBJ_CATEGORY[o->type] == CAT_INTERACTIVE
            && o->hitbox.aabb.x + o->hitbox.aabb.w < back)
            set_spent(&s->st, i);
    }
}

/*
** An attempt from `start`: the level's own, or one of its start positions.
** Every attempt starts here, the first one like every retry (3.4, 7.2).
*/
void sim_reset_at(sim_t *s, const level_start_t *start)
{
    run_state_t *st = &s->st;

    st->player = start_player(start);
    st->cam = (camera_t){{0.0, 0.0}};
    st->bounds = (ship_bounds_t){0};
    st->tick = 0;
    st->distance = 0.0;
    st->speed_mult = speed_px_per_s(start->speed) / SPEED_NORMAL;
    st->first_active = 0;
    st->complete = false;
    memset(st->spent, 0, st->spent_words * sizeof(uint64_t));
    if (start != &s->lvl.hdr.start) {
        place_along(s, start);
        spend_what_is_behind(s);
    }
    if (MODES[start->mode].corridor_height > 0.0)
        corridor_from_center(s, start->pos.y);
    st->player.grounded = starts_on_the_floor(s);
    st->player.can_jump = st->player.grounded;
    st->cam.pos.y = camera_rest_y(&st->player, &st->bounds);
}

void sim_reset(sim_t *s)
{
    sim_reset_at(s, &s->lvl.hdr.start);
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
    in = activate_orbs(s, in);               /*    an orb it is in takes the click */
    player_mode(p)->apply_input(p, in);       /* 1. the mode's impulses          */
    player_mode(p)->apply_forces(p);          /* 2. gravity and the fall cap     */
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
        player_mode(p)->update_rotation(p);   /* 8. the icon, cosmetic           */
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
