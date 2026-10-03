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

/* One slice of the level's static geometry, built once at load (9.2). */
typedef struct render_chunk {
    float left;                   /* the leftmost drawn x of its objects  */
    float right;                  /* the rightmost: a sprite can stick out */
    sfVertexBuffer *layer[LAYER_COUNT];
    sfVertexArray *array[LAYER_COUNT];   /* the fallback, same vertices    */
} render_chunk_t;

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
    render_chunk_t *chunks;       /* the level's static geometry (9.2)        */
    size_t nb_chunks;
    sfSprite *player_sprite;
    sfSprite *ground_sprite;
    sfSprite *background_sprite;  /* parallax, in the UI view (9.3)           */
    sfSprite *strip_sprite;       /* the corridor's floor and ceiling (5.3)   */
    sfSprite *explosion_sprite;   /* the death animation (6.1)                */
    ship_bounds_t drawn_bounds;   /* what the strips showed last frame        */
    ship_bounds_t fading_bounds;  /* a corridor that just went away (5.3)     */
    float fade_left;              /* seconds of fade still to draw            */
    sfInt64 frame_us;             /* the frame the renderer is drawing        */
    sfText *hud_text;             /* the percentage: set only when it changes */
    sfText *debug_text;           /* F3's numbers, its own: never the HUD's   */
    sfText *attempt_text;         /* drawn in the world, it scrolls away (9.5) */
    sfRectangleShape *bar_back;   /* the progress bar, created once            */
    sfRectangleShape *bar_fill;
    sfRectangleShape *flash;      /* the white flash at the end (9.8)          */
    char shown_percent[16];       /* the string on screen: only set when it changes */
    float end_time;               /* seconds since the level was completed (9.8) */
    uint64_t *seen_spent;         /* the spent bits the last frame drew (9.2)  */
    sfSprite *flash_sprite;       /* an object that just fired, fading out     */
    size_t flash_index[MAX_FLASHES];
    float flash_left[MAX_FLASHES];
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

/* The click that started the level isn't a jump: ignored until it's up. */
void input_level_started(gd_t *gd);

/* The static geometry: built once per level, drawn a few calls per frame. */
void level_build_chunks(level_t *lv, gd_t *gd);
void level_free_chunks(level_t *lv);
void render_objects(gd_t *gd, level_t *lv, float cam_x);

/* The camera the renderer uses: the player's x, the sim's y, pixel snapped. */
vec2_t level_camera(gd_t *gd, const level_t *lv);

/* Seconds from the completion until the player reaches the end wall (9.8). */
float level_end_flight(const level_t *lv);

#endif
