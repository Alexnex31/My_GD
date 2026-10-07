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
    #include "sim/alloc.h"
    #include "struct.h"
    #include "level.h"
    #include "view.h"
    #include "ui/ui.h"
    #include "options.h"

void close_window(sfRenderWindow *window);
void destroy_all(sfRenderWindow *window);
/* Every scene's events go through it: a resize letterboxes (9.7). */
bool poll_event(gd_t *gd);

sfRenderWindow *create_window(const settings_t *set);
void window_apply_sync(sfRenderWindow *w, const settings_t *set);
int window_apply(gd_t *gd);


cursor_t *create_cursor(void);
void print_cursor(cursor_t *cursor, sfRenderWindow *window);
void free_cursor(cursor_t *cursor);


void free_button(button_t *b);
void print_button(button_t *b, sfRenderWindow *w);
button_t *create_button(float x, float y, int size, sfTexture *texture);

void free_main_menu(main_m_t *m);
void print_main_menu(main_m_t *m, sfRenderWindow *w);
main_m_t *create_main_menu(gd_t *gd);


void free_editor_menu(editor_m_t *om);
void print_editor_menu(editor_m_t *om, gd_t *gd);
editor_m_t *create_editor_menu(gd_t *gd);

void free_level_button(level_button_t *lb);
void free_level_list_menu(level_list_t *level_list);
void print_level_list(level_list_t *level_list, sfRenderWindow *w);
float level_list_max_scroll(const level_list_t *list);
void level_list_scroll(level_list_t *list, float by);
level_list_t *create_level_list(gd_t *gd);

int check_end_screen_buttons(end_level_screen_t *end_screen, int mx, int my);
end_level_screen_t *create_end_level_screen(level_t *level, gd_t *gd);
void print_end_level_screen(gd_t *gd, end_level_screen_t *end_screen);
void free_end_level_screen(end_level_screen_t *end_screen);


void keyboard_events_main_menu(main_m_t **menu, gd_t *gd);
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

/* The music manager (FEATURES 4.7): one sfMusic for everything. */
bool music_probe(const char *path, float *duration);
int music_load(music_manager_t *m, const char *file);
void music_play_from(music_manager_t *m, double seconds);
void music_stop(music_manager_t *m);
void music_set_loop(music_manager_t *m, bool loop);
void music_set_volume(music_manager_t *m, int volume);
void music_free(music_manager_t *m);
void music_check_sync(music_manager_t *m, long tick);
void music_menu(gd_t *gd);
void music_preview(gd_t *gd, const song_t *s);
void music_update(gd_t *gd);

/* The level list's song line and its picker (FEATURES 4.6). */
void song_line_refresh(gd_t *gd, level_button_t *lb);
void song_picker_open(level_list_t *list, gd_t *gd, int index);
void song_picker_event(level_list_t *list, gd_t *gd);
void song_picker_update(level_list_t *list, gd_t *gd);
void song_picker_finish(level_list_t *list, gd_t *gd);
void song_picker_draw(level_list_t *list, gd_t *gd);
void song_picker_discard(level_list_t *list);

/* The folder of the executable: res/, levels/ and music/ live there. */
int exe_dir(char *buf, size_t size);

/* The widget toolkit's window side (FEATURES 3): events in, widgets drawn. */
void ui_gfx_create(gd_t *gd);
void ui_gfx_free(gd_t *gd);
bool ui_from_sf(gd_t *gd, const sfEvent *ev, ui_event_t *out);
void ui_draw(gd_t *gd, const ui_screen_t *ui);
int64_t ui_now_ms(void);
const sfUint32 *utf8_to_utf32(const char *s);

/* The F3 overlay (9.6): shapes, hitboxes, the tick's events, the numbers. */
void render_debug_overlay(gd_t *gd, level_t *lv, vec2_t cam);

#endif
