/*
** ALEXNEX PROJECT, 2026
** my_gd.h
** File description:
** header file for my_gd project
*/

#ifndef MY_GD
    #define MY_GD

    #include <SFML/Graphics.h>
    #include <SFML/Audio.h>
    #include <SFML/System.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include <unistd.h>
    #include <math.h>
    #include <dirent.h>
    #include "struct.h"
    #include "level.h"
    #include "view.h"

void my_putchar(char c);
int my_putstr(char const *str);
int my_put_nbr(int nb);
void free_arr(char **ar);
char *my_strcpy(char *dest, char const *src);

char **my_str_to_word_array(char *str);
char **my_str_word_array_delim(char *str, char *delim);
char *int_to_str(int nb);
char *float_to_str(float nb);
void *xcalloc(size_t n, size_t size);

sfVector2f create_vector_f(float x, float y);

void close_window(sfRenderWindow *window);
void destroy_all(sfRenderWindow *window);
sfRenderWindow *create_window(unsigned int width,
    unsigned int height);


cursor_t *create_cursor(void);
void print_cursor(cursor_t *cursor, sfRenderWindow *window);
void free_cursor(cursor_t *cursor);


void free_button(button_t *b);
void print_button(button_t *b, sfRenderWindow *w);
button_t *create_button(float x, float y, int size, sfTexture *texture);

void free_main_menu(main_m_t *m);
void print_main_menu(main_m_t *m, sfRenderWindow *w);
main_m_t *create_main_menu(gd_t *gd);

void free_option_menu(option_m_t *om);
void print_option_menu(option_m_t *om, sfRenderWindow *w);
option_m_t *create_option_menu(gd_t *gd);

void free_editor_menu(editor_m_t *om);
void print_editor_menu(editor_m_t *om, sfRenderWindow *w);
editor_m_t *create_editor_menu(gd_t *gd);

void free_level_button(level_button_t *lb);
void free_level_list_menu(level_list_t *level_list);
void print_level_list(level_list_t *level_list, sfRenderWindow *w);
level_list_t *create_level_list(gd_t *gd);

int check_end_screen_buttons(end_level_screen_t *end_screen, int mx, int my);
end_level_screen_t *create_end_level_screen(level_t *level, gd_t *gd);
void print_end_level_screen(gd_t *gd, end_level_screen_t *end_screen);
void free_end_level_screen(end_level_screen_t *end_screen);


void keyboard_events_main_menu(main_m_t **menu, gd_t *gd);
void keyboard_events_option_menu(option_m_t **om, gd_t *gd);
void keyboard_events_editor_menu(editor_m_t **editor_m, gd_t *gd);
void keyboard_events_level_list(level_list_t **lvl_list, gd_t *gd);
void keyboard_events_playing(level_t **level, gd_t *gd);


/* ./my_gd --check <level>: load it with the sim only and report (7.4). */
int level_check(const char *path);

/* Every draw goes through these, so the overlay can count them (9.0). */
void draw_sprite(gd_t *gd, const sfSprite *sprite, const sfRenderStates *rs);
void draw_text(gd_t *gd, const sfText *text);
void draw_rect(gd_t *gd, const sfRectangleShape *shape);
void draw_vertex_buffer(gd_t *gd, const sfVertexBuffer *buf,
    const sfRenderStates *rs);
void draw_vertex_array(gd_t *gd, const sfVertexArray *array,
    const sfRenderStates *rs);

/* One texture for every object image, built at startup (9.2). */
void atlas_build(gd_t *gd);

/* The letterbox of 9.7, applied to both views on every resize. */
void apply_letterbox(gd_t *gd, unsigned int w, unsigned int h);

/* The F3 overlay (9.6): shapes, hitboxes, the tick's events, the numbers. */
void render_debug_overlay(gd_t *gd, level_t *lv, vec2_t cam);

#endif
