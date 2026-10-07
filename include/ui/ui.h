/*
** ALEXNEX PROJECT, 2026
** ui/ui.h
** File description:
** header file for my_gd project
*/

#ifndef UI_UI_H
    #define UI_UI_H

    #include <stdint.h>

    #include "sim/binding.h"

    #define UI_ROW_H 60.0f            /* a list row, UI pixels                 */
    #define UI_REPEAT_DELAY_MS 300    /* held arrow: first repeat              */
    #define UI_REPEAT_EVERY_MS 80     /* then every                            */
    #define UI_WHEEL_ROWS 3           /* a list scrolls this much per notch    */

/*
** The widget toolkit (FEATURES 3). Pure C: the game layer turns its window's
** events into ui_event_t and draws from ui_look; the tests drive it alone.
** Every position is in the UI view's 1920x1080 pixels (3.1).
*/
typedef struct ui_rect {
    float x;
    float y;
    float w;
    float h;
} ui_rect_t;

typedef enum ui_event_kind {
    UI_MOVE,                      /* the mouse moved to x, y                 */
    UI_PRESS,                     /* `input` went down (at x, y if a mouse)  */
    UI_RELEASE,                   /* `input` went up                         */
    UI_WHEEL,                     /* `wheel` notches at x, y, up positive    */
    UI_TEXT                       /* the character `ch` was typed            */
} ui_event_kind_t;

typedef struct ui_event {
    ui_event_kind_t kind;
    binding_t input;              /* a key, mouse button or gamepad button   */
    float x;
    float y;
    int wheel;
    bool shift;
    unsigned int ch;
} ui_event_t;

typedef enum widget_kind {
    W_BUTTON,                     /* activates                               */
    W_TOGGLE,                     /* *value 0 or 1                           */
    W_SLIDER,                     /* *value min..max by step                 */
    W_CYCLER,                     /* *value min..max, choices[value], wraps  */
    W_KEYBIND,                    /* *binding, through a capture modal       */
    W_LIST,                       /* *value the selected row, choices[0..max] */
    W_TEXT                        /* text[0..text_cap - 1], typed when focused */
} widget_kind_t;

typedef struct widget widget_t;

/*
** Callbacks get the screen's ctx. They must never free the screen they're
** called from: a scene that changes sets a flag, and switches after its
** events (PLAN 10.1).
*/
typedef void (*widget_fn)(void *ctx, widget_t *w);
typedef void (*capture_fn)(void *ctx, widget_t *w, binding_t b);

struct widget {
    widget_kind_t kind;
    const char *label;
    ui_rect_t bounds;
    bool enabled;
    int *value;
    binding_t *binding;
    int min;
    int max;
    int step;
    const char *const *choices;   /* cycler and list: max + 1 entries       */
    int scroll;                   /* list: the first row shown              */
    widget_fn on_change;          /* only when the value really changed     */
    widget_fn on_activate;        /* a button, or a list's chosen row       */
    capture_fn on_capture;        /* NULL: the binding is set as captured   */
    char *text;                   /* text field: the caller's buffer        */
    int text_cap;                 /* its size, the NUL included             */
};

typedef struct ui_screen ui_screen_t;

struct ui_screen {
    widget_t *widgets;
    int count;
    int focused;                  /* -1 when nothing can be focused          */
    int hovered;                  /* under the mouse, -1 if none             */
    int pressed;                  /* where the left press started, -1        */
    int captured;                 /* the slider being dragged, -1            */
    int capturing;                /* the keybind waiting for an input, -1    */
    bool mouse_down;
    float mouse_x;
    float mouse_y;
    binding_t repeat_key;         /* the arrow held, BIND_NONE               */
    int64_t repeat_at_ms;
    ui_screen_t *modal;           /* a dialog on top: it gets every event    */
    void *ctx;
    float label_x;                /* where row labels start; 0: right-aligned
                                     against their widget                    */
    ui_rect_t panel;              /* drawn behind a dialog; no width: none   */
    void (*on_back)(void *ctx);                        /* Escape            */
    void (*on_key)(void *ctx, binding_t key, bool shift);  /* the others: Tab */
};

typedef enum ui_state {
    UI_IDLE,
    UI_HOVER,                     /* under the mouse, no button down         */
    UI_FOCUSED,                   /* the keyboard's                          */
    UI_PRESSED,                   /* the left press started here, held here  */
    UI_DISABLED
} ui_state_t;

/* What the renderer draws a widget as (3.2). */
typedef struct ui_look {
    ui_state_t state;
    float scale;                  /* 1.05 hovered, 0.92 pressed, else 1      */
    float alpha;                  /* 0.5 disabled, else 1                    */
    bool outline;                 /* focused                                 */
} ui_look_t;

/* Once, after the widgets are laid out: focus on the first enabled one. */
void ui_init(ui_screen_t *ui, widget_t *widgets, int count, void *ctx);

void ui_event(ui_screen_t *ui, const ui_event_t *ev, int64_t now_ms);

/* Every frame: a held arrow repeats (3.5). */
void ui_update(ui_screen_t *ui, int64_t now_ms);

ui_look_t ui_look(const ui_screen_t *ui, int i);

/* Where a slider's knob is, 0 to 1. */
float ui_slider_fraction(const widget_t *w);

/* How many of a list's rows fit in its bounds. */
int ui_list_rows(const widget_t *w);

bool ui_rect_contains(ui_rect_t r, float x, float y);

/* A text field has the focus: keys are its letters, not shortcuts. */
bool ui_typing(const ui_screen_t *ui);

#endif
