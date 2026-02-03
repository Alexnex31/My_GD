/*
** EPITECH PROJECT, 2025
** duck.h
** File description:
** header file to define duck struct
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

typedef struct object_list {
    int id;
} object_list_t;

typedef struct level {
    object_list_t *objects;
    float best;
    float percent;
    int lvl;
    int attempts;
} level_t;

typedef struct level_button {
    char *filename;
    float best;
    int attempts;
    button_t *play_level;
} level_button_t;

typedef struct level_list {
    sfSprite *background;
    char **names;
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
} textures_t;

typedef struct gd {
    textures_t *res;
    sfFont *main_font;
    sfRenderWindow *w;
    cursor_t *cursor;
    sfEvent *event;
    char menu;
} gd_t;

#endif
