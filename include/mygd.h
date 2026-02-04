/*
** ALEXNEX PROJECT, 2026
** my_gd.h
** File description:
** header file for my_gd project
*/

#ifndef MY_GD
    #define MY_GD

    #include <SFML/Graphics/RenderWindow.h>
    #include <SFML/Graphics/Color.h>
    #include <SFML/Graphics/RenderTexture.h>
    #include <SFML/Graphics/Texture.h>
    #include <SFML/Graphics/Sprite.h>
    #include <SFML/Graphics.h>
    #include <SFML/System/Export.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <SFML/System.h>
    #include <SFML/Config.h>
    #include <stdlib.h>
    #include <SFML/Window/Mouse.h>
    #include <SFML/Audio.h>
    #include <SFML/Audio/Types.h>
    #include <SFML/Audio/Export.h>
    #include <SFML/Audio/Sound.h>
    #include <SFML/Audio/SoundBuffer.h>
    #include <SFML/System/InputStream.h>
    #include <SFML/System/Time.h>
    #include <SFML/Audio/SoundStatus.h>
    #include <stdio.h>
    #include <math.h>
    #include <string.h>
    #include <time.h>
    #include <dirent.h>
    #include "struct.h"

void my_putchar(char c);
int my_putstr(char const *str);
int my_put_nbr(int nb);
void free_arr(char **ar);
int set_power(int i);
int len_int(int nb);
char *my_strcpy(char *dest, char const *src);
int my_stricpy(char *dest, char const *src, int i);

char **my_str_to_word_array(char *str);
char *int_to_str(int nb);
char *float_to_str(float nb);

sfVector2u create_vector(int x, int y);
sfVector2f create_vector_f(float x, float y);

void window_disp_clear(sfRenderWindow *window);
void close_window(sfRenderWindow *window);
void destroy_all(sfRenderWindow *window);
sfRenderWindow *create_window(unsigned int width,
    unsigned int height);

back_mus_t *play_background_music(char *filepath);
void free_music_back(back_mus_t *m);
sound_t play_sound(char *filepath);

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


void keyboard_events_main_menu(main_m_t **menu, gd_t *gd);
void keyboard_events_option_menu(option_m_t **om, gd_t *gd);
void keyboard_events_editor_menu(editor_m_t **editor_m, gd_t *gd);
void keyboard_events_level_list(level_list_t **lvl_list, gd_t *gd);
void keyboard_events_playing(level_t **level, gd_t *gd);


void load_level_data(char *levelname, level_t *level, gd_t *gd);


void free_player(player_t *player);
player_t *create_player(gd_t *gd);


void apply_physics(gd_t *gd, level_t *level, object_list_t *obj);
void check_collisions(gd_t *gd, level_t *level, object_list_t *obj);

#endif
