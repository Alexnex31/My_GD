/*
** ALEXNEX PROJECT, 2026
** editor.h
** File description:
** header file for my_gd project
*/

#ifndef GD_EDITOR_H
    #define GD_EDITOR_H

    #include "mygd.h"
    #include "editor/ed_props.h"
    #include "editor/ed_music.h"
    #include "editor/ed_check.h"
    #include "editor/ed_verify.h"
    #include "editor/ed_autosave.h"

/* The screen's layout, UI pixels (FEATURES 11.2). */
    #define ED_TOOLBAR_H 80.0f
    #define ED_PALETTE_W 220.0f
    #define ED_CANVAS_W (VIEW_W - ED_PALETTE_W)
    #define ED_CANVAS_H (VIEW_H - ED_TOOLBAR_H)
    #define ED_PALETTE_ROW 30.0f  /* one entry: they all have to fit         */
    #define ED_STATUS_H 40.0f     /* the strip along the canvas's bottom     */
    #define ED_STATUS_Y (VIEW_H - ED_STATUS_H)
    #define ED_VERIFY_FRAME_MS 10 /* the bot's share of a frame (11.10)      */
    #define ED_VERIFY_SLICE 1000  /* ticks between two looks at the clock    */
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

/* The question on top of the editor, if any (FEATURES 11.12). */
typedef enum ed_dialog {
    ED_DIALOG_NONE,
    ED_DIALOG_LEAVE,              /* Save, Discard, Cancel                   */
    ED_DIALOG_RESTORE             /* an autosave newer than the level's file */
} ed_dialog_t;

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
    bool level_panel;             /* L: the level's song and beat (11.11)    */
    bool song_on;                 /* P: its song plays from the canvas       */
    level_t *play;                /* the playtest, while it runs (11.9)      */
    vec2_t *trace;                /* where the last playtest went            */
    size_t trace_len;
    unsigned int trace_edits;     /* the level's edit count it was made at   */
    bool trace_bot;               /* it is the bot's run, not a playtest's   */
    bool trace_stopped;           /* and it ends where the bot got no further */
    ed_verify_t verify;           /* the bot, while it searches (11.10)      */
    char verdict[160];            /* what it last said, until the next edit  */
    ed_report_t report;           /* what looks wrong (11.13)                */
    int issue_at;                 /* the one N last went to                  */
    double song_short;            /* seconds the level outlasts its song by  */
    double level_seconds;         /* how long it lasts, measured with it     */
    ed_dialog_t dialog;
    ui_screen_t dialog_ui;
    widget_t dialog_widgets[3];
    int answer;                   /* the button chosen, 1 to 3; 0: none yet  */
    time_t restore_when;          /* the autosave's date                     */
    int64_t autosave_at_ms;       /* when the next one is due                */
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
    sfVertexArray *objects;       /* the objects in view, in the draw order  */
    size_t *drawn;                /* their indices, sorted each frame        */
    size_t drawn_cap;
    sfVertexArray *lines;         /* the grid and the level's limits         */
    sfRectangleShape *box;
    sfText *text;
    sfSprite *marker;             /* where attempts start                    */
    double end_x;                 /* measured after each edit: the level's end */
    double kill_y;                /* and its kill ceiling                    */
    bool measured;
    sfVector2i pan_from;
    bool leave;                   /* back to the list, after the events      */
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

/* editor_draw.c: shared with the panel */
void editor_fill(gd_t *gd, editor_t *ed, sfFloatRect r, sfColor c,
    sfColor outline);
void editor_label(editor_t *ed, gd_t *gd, const char *s, float x, float y,
    unsigned int size, sfColor c);

/* editor_panel.c: the selection's properties, over the canvas's corner */
bool editor_panel_holds(editor_t *ed, gd_t *gd, sfVector2f at);
void editor_panel_click(editor_t *ed, gd_t *gd, sfVector2f at);
void editor_panel_draw(editor_t *ed, gd_t *gd);

/* editor_song.c: the level's song over the canvas */
void editor_song_toggle(editor_t *ed, gd_t *gd);
void editor_song_stop(editor_t *ed, gd_t *gd);
void editor_song_follow(editor_t *ed, gd_t *gd);
void editor_song_lines(editor_t *ed, gd_t *gd, double left, double right,
    double top, double bottom);
void editor_line(editor_t *ed, double x0, double y0, double x1, double y1,
    sfColor c);

/* editor_scene.c */
bool editor_save(editor_t *ed);              /* false: it couldn't */
void editor_close(editor_t *ed, gd_t *gd);   /* the window's cross */

/* editor_verify.c: the bot, what looks wrong, the strip that says both */
void editor_measure(editor_t *ed, gd_t *gd);
void editor_verify_start(editor_t *ed);
void editor_verify_events(editor_t *ed, gd_t *gd);
void editor_next_issue(editor_t *ed);
bool editor_status_holds(sfVector2f at);
void editor_status_draw(editor_t *ed, gd_t *gd);

/* editor_dialog.c: unsaved changes, and the autosave */
void editor_dialog_open(editor_t *ed, ed_dialog_t which);
void editor_dialog_events(editor_t *ed, gd_t *gd);
void editor_dialog_draw(editor_t *ed, gd_t *gd);
void editor_autosave(editor_t *ed, bool now);

/* editor_play.c: the playtest */
void editor_playtest(editor_t *ed, gd_t *gd, bool from_start_position);
void editor_play_events(editor_t *ed, gd_t *gd);
void editor_play_stop(editor_t *ed, gd_t *gd);

/* editor_canvas.c */
void editor_apply_camera(editor_t *ed, gd_t *gd);
vec2_t editor_mouse_world(editor_t *ed, gd_t *gd);
bool editor_mouse_on_canvas(gd_t *gd);
void editor_zoom_at(editor_t *ed, gd_t *gd, float factor);

#endif
