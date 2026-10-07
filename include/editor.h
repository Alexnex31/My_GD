/*
** ALEXNEX PROJECT, 2026
** editor.h
** File description:
** header file for my_gd project
*/

#ifndef GD_EDITOR_H
    #define GD_EDITOR_H

    #include "mygd.h"
    #include "editor/ed_level.h"

/* The screen's layout, UI pixels (FEATURES 11.2). */
    #define ED_TOOLBAR_H 80.0f
    #define ED_PALETTE_W 220.0f
    #define ED_CANVAS_W (VIEW_W - ED_PALETTE_W)
    #define ED_CANVAS_H (VIEW_H - ED_TOOLBAR_H)
    #define ED_ZOOM_MIN 0.25f
    #define ED_ZOOM_MAX 4.0f

/*
** The editor on screen: the level it edits, where the canvas looks, and what
** the mouse is doing. Everything it decides about the level goes through
** editor/ed_level.c, which is tested; this is the window's side only.
*/
typedef struct editor {
    ed_level_t lv;
    char path[64];                /* levels/<id>.gd                          */
    vec2_t cam;                   /* the canvas's center, world coordinates  */
    float zoom;                   /* world px per UI px: 2 is zoomed out     */
    double grid;                  /* 50 or 25                                */
    sfView *view;                 /* the canvas                              */
    ed_entry_t entries[ED_MAX_ENTRIES];
    int nb_entries;
    int entry;                    /* what a left click places                */
    sfVertexArray *objects[LAYER_COUNT];
    sfVertexArray *lines;         /* the grid and the level's limits         */
    sfRectangleShape *box;
    sfText *text;
    sfSprite *marker;             /* where attempts start                    */
    double end_x;                 /* measured after each edit: the level's end */
    double kill_y;                /* and its kill ceiling                    */
    bool measured;
    bool panning;
    bool placing;                 /* the left button is down on the canvas   */
    bool erasing;                 /* the right one is                        */
    sfVector2i pan_from;
    bool leave;                   /* back to the list, after the events      */
    bool warned;                  /* told once that changes aren't saved     */
    char notice[96];
    int64_t notice_until_ms;      /* when the notice goes away               */
} editor_t;

editor_t *editor_open(gd_t *gd, const char *id, const char *new_name);
void editor_free(editor_t *ed);
void editor_events(editor_t *ed, gd_t *gd);
void editor_draw(editor_t *ed, gd_t *gd);

/* editor_canvas.c */
void editor_apply_camera(editor_t *ed, gd_t *gd);
vec2_t editor_mouse_world(editor_t *ed, gd_t *gd);
bool editor_mouse_on_canvas(gd_t *gd);
void editor_zoom_at(editor_t *ed, gd_t *gd, float factor);

#endif
