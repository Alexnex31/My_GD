/*
** ALEXNEX PROJECT, 2026
** editor.h
** File description:
** header file for my_gd project
*/

#ifndef GD_EDITOR_H
    #define GD_EDITOR_H

    #include "mygd.h"
    #include "editor/ed_select.h"

/* The screen's layout, UI pixels (FEATURES 11.2). */
    #define ED_TOOLBAR_H 80.0f
    #define ED_PALETTE_W 220.0f
    #define ED_CANVAS_W (VIEW_W - ED_PALETTE_W)
    #define ED_CANVAS_H (VIEW_H - ED_TOOLBAR_H)
    #define ED_ZOOM_MIN 0.25f
    #define ED_ZOOM_MAX 4.0f
    #define ED_BIG_STEP 100.0     /* Z, Q, S, D: a whole block               */
    #define ED_REPEAT_DELAY_MS 350    /* a held key waits, then repeats      */
    #define ED_REPEAT_EVERY_MS 60
    #define ED_DRAG_PX 4          /* a shakier click than this is a drag     */

typedef enum ed_tool {
    ED_TOOL_PLACE,
    ED_TOOL_SELECT
} ed_tool_t;

/* What the mouse is in the middle of (FEATURES 11.5). */
typedef enum ed_mouse {
    ED_IDLE,
    ED_PANNING,
    ED_PAINTING,                  /* placing, the left button held           */
    ED_ERASING,                   /* the right one                           */
    ED_DRAG_PENDING,              /* pressed on an object, not moved yet     */
    ED_DRAGGING,
    ED_BOXING                     /* pulling a selection box                 */
} ed_mouse_t;

/*
** The editor on screen: the level it edits, where the canvas looks, and what
** the mouse is doing. Everything it decides about the level goes through
** editor/ed_level.c, which is tested; this is the window's side only.
*/
typedef struct editor {
    ed_level_t lv;
    ed_history_t hist;            /* every edit, to undo and redo            */
    ed_clip_t clip;
    ed_grab_t grab;               /* the selection, while it is dragged      */
    ed_tool_t tool;
    ed_mouse_t mouse;
    sfVector2i press_px;          /* where the left button went down         */
    vec2_t press_at;              /* the same, in the world                  */
    int press_id;                 /* the object it went down on              */
    sfKeyEvent repeat;            /* the held key that repeats, if any       */
    bool repeating;
    int64_t repeat_at_ms;         /* when it acts again                      */
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

/* editor_mouse.c: the tools */
void editor_mouse_press(editor_t *ed, gd_t *gd, const sfEvent *ev);
void editor_mouse_move(editor_t *ed, gd_t *gd, const sfEvent *ev);
void editor_mouse_release(editor_t *ed, gd_t *gd);
bool editor_mouse_cancel(editor_t *ed);      /* false: nothing to cancel */
rect_t editor_box(editor_t *ed, gd_t *gd);   /* the selection box, world */
void editor_notice(editor_t *ed, const char *what);

/* editor_canvas.c */
void editor_apply_camera(editor_t *ed, gd_t *gd);
vec2_t editor_mouse_world(editor_t *ed, gd_t *gd);
bool editor_mouse_on_canvas(gd_t *gd);
void editor_zoom_at(editor_t *ed, gd_t *gd, float factor);

#endif
