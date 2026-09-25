/*
** ALEXNEX PROJECT, 2026
** sim/sim.c
** File description:
** the simulation's run state: reset, tick, snapshots (3.3, 3.4)
*/

#include <string.h>

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
