/*
** ALEXNEX PROJECT, 2026
** struct.h
** File description:
** header file to define structures
*/

#include "mygd.h"

#ifndef GD_STRUCT_H
    #define GD_STRUCT_H

typedef struct sound {
    sfSound *s;
    sfSoundBuffer *sb;
} sound_t;

typedef struct back_mus {
    sfSoundBuffer *sbuf;
    sfSound *sound;
} back_mus_t;

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

typedef struct spike {
    sfSprite *sprite;
    sfVector2f pos;
    int size;
} spike_t;

typedef struct block {
    sfSprite *sprite;
    sfVector2f pos;
    int size;
} block_t;

typedef struct portal {
    sfSprite *sprite;
    sfVector2f pos;
    char gamemode;
    int size;
} portal_t;

typedef struct object_list {
    spike_t **spikes;
    block_t **blocks;
    portal_t **portals;
    block_t **portal_blocks;
    block_t *ground;
    sfVector2f sprite_ground_pos;
    int nb_portals;
} object_list_t;

typedef struct player {
    sfSprite *sprite;
    sfVector2f pos;
    float vy;
    float size;
    float orientation;
    char gamemode;
    char state;
    char allow_jump;
} player_t;

typedef struct end_level_screen {
    sfSprite *background;
    sfText *title_text;
    sfText *attempts_text;
    sfText *percent_text;
    button_t *retry_button;
    button_t *quit_button;
} end_level_screen_t;

typedef struct level {
    sfSprite *background;
    object_list_t *objects;
    player_t *player;
    float speed;
    float shift;
    float yshift;
    float level_end;
    float best;
    float percent;
    int lvl;
    int attempts;
    int curr_attempts;
    sfText *attempt_text;
    sfText *percent_text;
    sfClock *attempt_display_clock;
    char show_attempt_text;
    char level_completed;
    end_level_screen_t *end_screen;
} level_t;

typedef struct level_button {
    char *filename;
    char *display_name;
    float best;
    int attempts;
    int level_num;
    button_t *play_button;
    sfText *name_text;
    sfText *attempts_text;
    sfText *best_text;
} level_button_t;

typedef struct level_list {
    sfSprite *background;
    char **names;
    level_button_t **level_buttons;
    int nb_levels;
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
    sfTexture *spike;
    sfTexture *block;
    sfTexture *player_icon;
    sfTexture *ship_icon;
    sfTexture *end_level_background;
    sfTexture *retry_button;
    sfTexture *quit_button;
} textures_t;

typedef struct music {
    sfMusic *main;
    sfMusic *param;
    sfMusic *editor;
    sfMusic *level1;
} music_t;

typedef struct gd {
    textures_t *res;
    music_t *musics;
    sfFont *main_font;
    sfRenderWindow *w;
    cursor_t *cursor;
    sfEvent *event;
    char menu;
    int selected_level;
} gd_t;

#endif
