/*
** ALEXNEX PROJECT, 2026
** sim/sim.h
** File description:
** header file for my_gd project
*/

#ifndef MYGD_SIM_H
    #define MYGD_SIM_H
    #include "sim/sim_types.h"

int sim_load(sim_t *s, const char *path, sim_log_fn log);   /* levels/<id>.gd */
int sim_load_mem(sim_t *s, const char *buf, size_t len,
    const char *id, sim_log_fn log);                        /* same, from memory */
void sim_free(sim_t *s);
void sim_reset(sim_t *s);                      /* back to spawn, every object live again */
void sim_tick(sim_t *s, input_t in);           /* advance one fixed tick */
float sim_percent(const sim_t *s);

/* Run-state snapshots: practice checkpoints, the bot, determinism tests */
void sim_snapshot_init(sim_snapshot_t *snap, const sim_t *s);      /* allocates once */
void sim_snapshot_save(sim_snapshot_t *snap, const sim_t *s);
void sim_snapshot_restore(sim_t *s, const sim_snapshot_t *snap);
void sim_snapshot_free(sim_snapshot_t *snap);
uint64_t sim_state_hash(const sim_t *s);       /* hash of the whole run state */
uint64_t sim_physics_hash(const sim_t *s);     /* the same without the camera (bot memo, 3.3) */

#endif
