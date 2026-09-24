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

void free_level(level_t *level);
void print_level(gd_t *gd, level_t *level);
level_t *start_level(gd_t *gd);
void reset_attempt_display(level_t *level);
int check_end_screen_buttons(end_level_screen_t *end_screen, int mx, int my);


void keyboard_events_main_menu(main_m_t **menu, gd_t *gd);
void keyboard_events_option_menu(option_m_t **om, gd_t *gd);
void keyboard_events_editor_menu(editor_m_t **editor_m, gd_t *gd);
void keyboard_events_level_list(level_list_t **lvl_list, gd_t *gd);
void keyboard_events_playing(level_t **level, gd_t *gd);


int load_level_data(char *levelname, level_t *level, gd_t *gd);
void rewrite_level(level_t *level, gd_t *gd);

void free_block(block_t *block);
void free_spike(spike_t *spike);
void free_block_list(block_t **list);

void free_player(player_t *player);
player_t *create_player(gd_t *gd);


void apply_physics(level_t *level, object_list_t *obj);
void check_collisions(gd_t *gd, level_t *level, object_list_t *obj);

void load_portal(char **arr, level_t *level, gd_t *gd);

#endif
