/*
** ALEXNEX PROJECT, 2026
** struct.h
** File description:
** header file to define structures
*/

#ifndef GD_STRUCT_H
    #define GD_STRUCT_H
    #include <SFML/Graphics.h>
    #include <SFML/Audio.h>

    #include <pthread.h>

    #include "sim/input_ticks.h"
    #include "sim/progress.h"
    #include "sim/settings.h"
    #include "music/library.h"
    #include "ui/ui.h"
    #include "level.h"

typedef struct cursor {
    sfSprite *cursor_s;
    sfTexture *cursor_t;
} cursor_t;

typedef struct button {
    sfSprite *sprite;
    sfVector2f pos;
    int size;
    char pressed;
} button_t;

typedef struct end_level_screen {
    sfSprite *background;
    sfText *title_text;
    sfText *attempts_text;
    sfText *percent_text;
    sfText *song_text;            /* "Song: Title — Artist (License)" (4.11) */
    button_t *retry_button;
    button_t *quit_button;
} end_level_screen_t;

typedef struct level_button {
    char *filename;
    char *display_name;           /* the header's name (7.2)                */
    char id[24];                  /* the file's digits                      */
    uint64_t file_hash;           /* the level's current version (6.4)      */
    bool edited;                  /* the best was set on another version    */
    float best;
    int attempts;
    button_t *play_button;
    sfText *name_text;
    sfText *attempts_text;
    sfText *best_text;
    level_header_t hdr;           /* its song and offset (4.4)              */
    sfText *song_text;            /* "Song: Title — Artist": opens the picker */
} level_button_t;

typedef struct level_list {
    sfSprite *background;
    char **names;
    level_button_t **level_buttons;
    int nb_levels;
    struct gd *gd;                /* for the picker's callbacks             */
    int picker_for;               /* the level whose song is chosen, -1 (4.6) */
    bool picker_closing;          /* closed by a callback: freed after events */
    ui_screen_t picker_root;      /* nothing of its own: dims, holds the modal */
    ui_screen_t picker;
    widget_t picker_list;
    int picker_row;               /* 0: the level's own song                */
    char **picker_rows;
    sfText *picker_title;
} level_list_t;

typedef struct editor_menu {
    sfSprite *background;
} editor_m_t;

typedef struct option_menu {
    sfSprite *background;
} option_m_t;

typedef struct main_menu {
    sfSprite *background;
    button_t *play;
    button_t *param;
    button_t *online;
    sfText *title;
    char *title_string;
} main_m_t;

typedef struct textures {
    sfTexture *main_background;
    sfTexture *opt_background;
    sfTexture *edi_background;
    sfTexture *list_background;
    sfTexture *level_background;
    sfTexture *play_button;
    sfTexture *opt_button;
    sfTexture *onli_button;
    sfTexture *ground;
    sfTexture *explosion;
    sfTexture *spike;
    sfTexture *block;
    sfTexture *cube_portal;
    sfTexture *player_icon;
    sfTexture *ship_icon;
    sfTexture *end_level_background;
    sfTexture *retry_button;
    sfTexture *quit_button;
} textures_t;

/* The one playing song (FEATURES 4.7): levels, menus and previews share it. */
typedef struct music_manager {
    sfMusic *current;
    char current_file[SONG_FILE_MAX];
    int volume;                   /* the setting's 0-100, curved when applied */
    double level_offset;          /* where the level's song starts            */
    double audio_offset;          /* seconds, the setting's (4.8)             */
    int64_t last_check_ms;        /* drift checks once a second               */
    char menu_file[SONG_FILE_MAX]; /* "" when the library is empty            */
    float menu_position;          /* where the menu song was left (4.10)      */
    int64_t preview_until_ms;     /* the picker's preview: 0 none, -1 over    */
} music_manager_t;

/* The polling thread writes the queue; the ticks read it (FEATURES 1). */
typedef struct input_poll {
    input_queue_t queue;
    input_reader_t reader;        /* the ticks' side: what's down, what's latched */
    bindings_t jump;              /* the settings' jump inputs, for the thread */
    pthread_t thread;
    atomic_bool running;
} input_poll_t;

typedef struct gd {
    textures_t *res;
    music_manager_t music;
    library_t library;            /* the songs in music/ (FEATURES 4.2)       */
    sfFont *main_font;
    sfRenderWindow *w;
    sfView *ui_view;              /* menus and the HUD, fixed 1920x1080 (9.1) */
    sfView *level_view;           /* the world, centered on the camera        */
    float viewport_px_w;          /* the letterboxed viewport, window pixels  */
    cursor_t *cursor;
    sfEvent *event;
    char menu;
    char selected_id[24];         /* the level file's digits (7.2)            */
    input_poll_t input;           /* the jump, polled by its thread (FEATURES 1) */
    bool debug_overlay;           /* F3 (9.6)                                 */
    int draw_calls;               /* this frame's, counted in draw.c (9.0)    */
    sfTexture *atlas;             /* every object image in one texture (9.2)  */
    sfFloatRect atlas_rect[OBJ_TYPE_COUNT];
    progress_t progress;          /* attempts and best, loaded once (6.4)     */
    settings_t settings;          /* save/settings.txt (FEATURES 2)           */
    sfRectangleShape *ui_shape;   /* the widgets', reused (FEATURES 3)        */
    sfText *ui_text;
} gd_t;

#endif
