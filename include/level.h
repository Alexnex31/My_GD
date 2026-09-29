/*
** ALEXNEX PROJECT, 2026
** level.h
** File description:
** header file for my_gd project
*/

#ifndef GD_LEVEL_H
    #define GD_LEVEL_H

    #include <SFML/Graphics.h>
    #include <SFML/Audio.h>

    #include "sim/level.h"
    #include "sim/sim.h"
    #include "view.h"

typedef struct gd gd_t;

typedef enum level_state {
    LEVEL_PLAYING,
    LEVEL_DYING,                  /* the explosion, then a respawn (6.1)    */
    LEVEL_COMPLETE
} level_state_t;

/* This session's numbers. The file is written when the level is left (6.2). */
typedef struct level_stats {
    int attempts;
    float best;
    float practice_best;
    bool dirty;
} level_stats_t;

typedef struct level {
    sim_t sim;                    /* the whole simulation: level data + run state */
    char id[24];                  /* the file's digits (7.2)                 */
    uint64_t file_hash;           /* which version of the level this is (6.4) */
    level_state_t state;
    int death_ticks;              /* counts down through LEVEL_DYING          */
    vec2_t death_pos;             /* where to draw the explosion              */
    sfInt64 accumulator;          /* microseconds x TICK_RATE (3.6)           */
    sfClock *clock;
    level_stats_t stats;
    sfSprite *object_sprite;      /* one sprite, reused per object (7.3)      */
    sfSprite *player_sprite;
    sfSprite *ground_sprite;
    sfText *hud_text;
    struct end_level_screen *end_screen;
} level_t;

/* The scene (3.6): events, then whole ticks, then one render. */
void handle_playing(gd_t *gd, level_t **level);
void level_step(level_t *lv, gd_t *gd, input_t in);
level_t *level_start(gd_t *gd, const char *id);
void level_free(level_t *lv, gd_t *gd);
void level_render(gd_t *gd, level_t *lv);

/* Death, respawn and the session's numbers (6.1, 6.2, 6.5). */
void level_on_death(level_t *lv, gd_t *gd);
void level_on_complete(level_t *lv, gd_t *gd);
void level_respawn(level_t *lv, gd_t *gd);
void level_restart(level_t *lv, gd_t *gd);
void level_count_attempt(level_t *lv);
void level_record_best(level_t *lv);
void level_flush_stats(level_t *lv, gd_t *gd);

/* One tick's input (3.6). Polled per tick, so `pressed` lasts exactly one. */
input_t input_for_tick(gd_t *gd);

/* The camera the renderer uses: the player's x, the sim's y, pixel snapped. */
vec2_t level_camera(gd_t *gd, const level_t *lv);

#endif
