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
    #include "music/library.h"
    #include "fx/trail.h"
    #include "ui/ui.h"

typedef struct gd gd_t;

/* One slice of the level's static geometry, built once at load (9.2). */
typedef struct render_chunk {
    float left;                   /* the leftmost drawn x of its objects  */
    float right;                  /* the rightmost: a sprite can stick out */
    sfVertexBuffer *layer[LAYER_COUNT];
    sfVertexArray *array[LAYER_COUNT];   /* the fallback, same vertices    */
} render_chunk_t;

    #define PAUSE_ITEMS 4

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
    int64_t last_frame_us;        /* input_now_us at the last frame's ticks   */
    level_stats_t stats;
    render_chunk_t *chunks;       /* the level's static geometry (9.2)        */
    size_t nb_chunks;
    sfVertexArray *spinning;      /* the saws in view, turned each frame      */
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
    song_choice_t song;           /* what plays, from where (FEATURES 4.4)    */
    sfText *notice_text;          /* "Song missing", for a few seconds        */
    float notice_left;
    int start_at;                 /* 0: the level's start; k: its k-th start
                                     position, an attempt that doesn't count
                                     (FEATURES 11.8)                           */
    bool playtest;                /* run by the editor: nothing is saved (11.9) */
    bool playtest_over;           /* died or finished: back to the editor      */
    vec2_t *path;                 /* playtest: where the player went           */
    size_t path_len;
    size_t path_cap;
    trail_t trail;                /* the wave's path, corners only (FEATURES 8.5) */
    sfVertexArray *trail_va;      /* its strip, built again each frame         */
    bool practice;                /* practice mode (PLAN 13.1)                 */
    sim_snapshot_t *checkpoints;  /* its checkpoints, the last one respawns    */
    size_t nb_checkpoints;
    size_t cap_checkpoints;
    sfRectangleShape *diamond;    /* a checkpoint in the level                 */
    sfRectangleShape *bar_tick;   /* and on the progress bar                   */
    bool paused;                  /* the pause menu is open: no tick (PLAN 10.4) */
    ui_screen_t pause_ui;
    widget_t pause_widgets[PAUSE_ITEMS];
    int pause_action;             /* a button's, done once the events are read */
} level_t;

/* The scene (3.6): events, then whole ticks, then one render. */
void handle_playing(gd_t *gd, level_t **level);
void level_step(level_t *lv, gd_t *gd, input_t in);
level_t *level_start(gd_t *gd, const char *id);
/* The editor's playtest: a level that is no file, from start `start_at`. */
level_t *level_start_doc(gd_t *gd, const level_doc_t *doc,
    const level_header_t *hdr, const char *id, int start_at);
void level_frame(gd_t *gd, level_t *lv);     /* the frame's ticks, then render */
/* A (-1) and E (+1): the next start position, and an attempt from it. */
void level_change_start(level_t *lv, gd_t *gd, int dir);
const level_start_t *level_current_start(const level_t *lv);
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

/*
** The jump (3.6, FEATURES 1): a thread polls the buttons every millisecond and
** stamps each change; a tick gets the presses that happened inside its own
** 1/240 s. Ticks run up to input_now_us, the time the input is known until.
*/
void input_init_threads(void);               /* first thing in main */
int64_t input_clock_us(void);
int64_t input_now_us(gd_t *gd);
input_t input_for_tick(gd_t *gd, int64_t start, int64_t end);
bool input_event_is(const sfEvent *ev, binding_t b);

/* Per level. The click that started it isn't a jump: ignored until it's up. */
void input_start(gd_t *gd);
void input_resume(gd_t *gd);                 /* out of the pause menu */
void input_stop(gd_t *gd);

/*
** The pause menu (PLAN 10.4): Escape or a lost focus opens it, the level
** stops ticking and its song waits. Resume, Restart, Quit.
*/
void level_pause(level_t *lv, gd_t *gd);
void pause_event(level_t *lv, gd_t *gd);     /* one event, while it's open */
void pause_act(level_t **lv, gd_t *gd);      /* what its buttons asked */
void pause_draw(gd_t *gd, level_t *lv);

/*
** Practice mode (PLAN 13.1): the checkpoint keys place and remove run-state
** snapshots, a death or a restart goes back to the last one, and the run's
** percentage is practice's only.
*/
void practice_create(level_t *lv);
void practice_free(level_t *lv);
void practice_set(level_t *lv, gd_t *gd, bool on);
void practice_place(level_t *lv);
void practice_remove(level_t *lv);
void practice_clear(level_t *lv);
bool practice_restore(level_t *lv);          /* false: no checkpoint */
void practice_draw_world(gd_t *gd, level_t *lv);
void practice_draw_bar(gd_t *gd, level_t *lv, sfVector2f bar, sfVector2f size);

/* The static geometry: built once per level, drawn a few calls per frame. */
void level_build_chunks(level_t *lv, gd_t *gd);
void level_free_chunks(level_t *lv);
void render_objects(gd_t *gd, level_t *lv, float cam_x);
void object_vertices(gd_t *gd, const object_t *o, sfVertex *v);   /* 6 of them */
int object_layer(const object_t *o);

/* The camera the renderer uses: the player's x, the sim's y, pixel snapped. */
vec2_t level_camera(gd_t *gd, const level_t *lv);
void render_trail(gd_t *gd, level_t *lv, vec2_t head, float cam_x);

/* Seconds from the completion until the player reaches the end wall (9.8). */
float level_end_flight(const level_t *lv);

#endif
