/*
** ALEXNEX PROJECT, 2026
** tests/test_ui.c
** File description:
** the widget toolkit, driven by made-up events (FEATURES 3)
*/

#include "test.h"
#include <string.h>

#include "ui/ui.h"

enum { BUTTON, TOGGLE, SLIDER, OFF, CYCLER, KEYBIND, LIST, COUNT };

typedef struct fixture {
    ui_screen_t ui;
    widget_t w[COUNT];
    int toggle;
    int slider;
    int cycler;
    int row;
    binding_t bound;
    int activations;
    int changes;
    int backs;
    int keys;
    bool shift;
    binding_t captured;
    int captures;
} fixture_t;

static const char *const CHOICES[] = {"a", "b", "c", "d", "e"};
static const char *const ROWS[] = {"0", "1", "2", "3", "4", "5", "6", "7",
    "8", "9"};

static void on_activate(void *ctx, widget_t *w)
{
    (void)w;
    ((fixture_t *)ctx)->activations += 1;
}

static void on_change(void *ctx, widget_t *w)
{
    (void)w;
    ((fixture_t *)ctx)->changes += 1;
}

static void on_back(void *ctx)
{
    ((fixture_t *)ctx)->backs += 1;
}

static void on_key(void *ctx, binding_t key, bool shift)
{
    (void)key;
    ((fixture_t *)ctx)->keys += 1;
    ((fixture_t *)ctx)->shift = shift;
}

static void on_capture(void *ctx, widget_t *w, binding_t b)
{
    (void)w;
    ((fixture_t *)ctx)->captured = b;
    ((fixture_t *)ctx)->captures += 1;
}

static ui_rect_t row_at(int i)
{
    return (ui_rect_t){1400.0f, 100.0f + 100.0f * (float)i, 380.0f, 80.0f};
}

static void setup(fixture_t *f)
{
    *f = (fixture_t){.slider = 50, .cycler = 2, .row = 0,
        .bound = {BIND_KEY, KEY_Space}};
    for (int i = 0; i < COUNT; i++)
        f->w[i] = (widget_t){.kind = W_BUTTON, .label = "x",
            .bounds = row_at(i), .enabled = true, .on_change = on_change,
            .on_activate = on_activate};
    f->w[TOGGLE].kind = W_TOGGLE;
    f->w[TOGGLE].value = &f->toggle;
    f->w[SLIDER] = (widget_t){W_SLIDER, "s", {1400.0f, 300.0f, 400.0f, 80.0f},
        true, &f->slider, NULL, 0, 100, 5, NULL, 0, on_change, NULL, NULL};
    f->w[OFF].enabled = false;
    f->w[CYCLER] = (widget_t){W_CYCLER, "c", row_at(CYCLER), true, &f->cycler,
        NULL, 0, 4, 1, CHOICES, 0, on_change, NULL, NULL};
    f->w[KEYBIND].kind = W_KEYBIND;
    f->w[KEYBIND].binding = &f->bound;
    f->w[LIST] = (widget_t){W_LIST, "l", {1400.0f, 700.0f, 380.0f,
        3 * UI_ROW_H}, true, &f->row, NULL, 0, 9, 1, ROWS, 0, on_change,
        on_activate, NULL};
    ui_init(&f->ui, f->w, COUNT, f);
    f->ui.on_back = on_back;
    f->ui.on_key = on_key;
}

static void send(fixture_t *f, ui_event_kind_t kind, binding_t in, float x,
    float y)
{
    ui_event_t ev = {kind, in, x, y, 0, false};

    ui_event(&f->ui, &ev, 0);
}

static const binding_t LEFT_CLICK = {BIND_MOUSE, MOUSE_Left};

static void move(fixture_t *f, float x, float y)
{
    send(f, UI_MOVE, (binding_t){BIND_NONE, 0}, x, y);
}

static void press_at(fixture_t *f, float x, float y)
{
    send(f, UI_PRESS, LEFT_CLICK, x, y);
}

static void release_at(fixture_t *f, float x, float y)
{
    send(f, UI_RELEASE, LEFT_CLICK, x, y);
}

static void click_at(fixture_t *f, float x, float y)
{
    press_at(f, x, y);
    release_at(f, x, y);
}

static void key(fixture_t *f, key_id_t k, int64_t now)
{
    ui_event_t down = {UI_PRESS, {BIND_KEY, (int)k}, 0, 0, 0, false};
    ui_event_t up = {UI_RELEASE, {BIND_KEY, (int)k}, 0, 0, 0, false};

    ui_event(&f->ui, &down, now);
    ui_event(&f->ui, &up, now);
}

static void test_button_activates_on_release(void)
{
    fixture_t f;

    setup(&f);
    CHECK(f.ui.focused == BUTTON);
    press_at(&f, 1500, 120);
    CHECK(f.activations == 0);                       /* not on the press */
    release_at(&f, 1500, 120);
    CHECK(f.activations == 1);
    press_at(&f, 1500, 120);
    release_at(&f, 100, 120);                        /* dragged away: cancelled */
    press_at(&f, 100, 120);
    release_at(&f, 1500, 120);                       /* started elsewhere */
    press_at(&f, 1500, 120);
    release_at(&f, 1500, 220);                       /* released on another */
    CHECK(f.activations == 1 && f.toggle == 0);
}

static void test_toggle_and_disabled(void)
{
    fixture_t f;

    setup(&f);
    click_at(&f, 1500, 220);
    CHECK(f.toggle == 1 && f.changes == 1);
    click_at(&f, 1500, 220);
    CHECK(f.toggle == 0 && f.changes == 2);
    move(&f, 1500, 420);                             /* the disabled one */
    CHECK(f.ui.focused == TOGGLE && f.ui.hovered == -1);
    click_at(&f, 1500, 420);
    CHECK(f.activations == 0);
}

/* The slider follows a dragged mouse anywhere, on its step grid (3.5). */
static void test_slider_capture(void)
{
    fixture_t f;

    setup(&f);
    press_at(&f, 1400 + 400 * 0.33f, 320);
    CHECK(f.slider == 35 && f.ui.captured == SLIDER);
    move(&f, 3000, 900);                             /* far outside */
    CHECK(f.slider == 100);
    move(&f, -50, 0);
    CHECK(f.slider == 0);
    move(&f, -80, 0);
    CHECK(f.changes == 3);                           /* no change, no call */
    release_at(&f, -80, 0);
    CHECK(f.ui.captured == -1);
    move(&f, 1700, 320);
    CHECK(f.slider == 0);                            /* released: no more */
    CHECK(ui_slider_fraction(&f.w[SLIDER]) == 0.0f);
    f.w[SLIDER].max = 10;                            /* a step that overshoots */
    f.w[SLIDER].step = 4;
    press_at(&f, 1790, 320);
    CHECK(f.slider == 8);                            /* 97.5%: 2.4 steps */
    move(&f, 3000, 320);
    CHECK(f.slider == 10);                           /* 3 steps, 12, is past max */
    release_at(&f, 3000, 320);
}

static void test_keyboard_navigation(void)
{
    fixture_t f;
    int order[] = {TOGGLE, SLIDER, CYCLER, KEYBIND, LIST};

    setup(&f);
    for (int i = 0; i < 5; i++) {
        key(&f, KEY_Down, 0);
        CHECK(f.ui.focused == order[i]);             /* OFF is skipped */
    }
    setup(&f);
    key(&f, KEY_Up, 0);
    CHECK(f.ui.focused == LIST);                     /* wraps */
    setup(&f);
    key(&f, KEY_Enter, 0);
    CHECK(f.activations == 1);
    key(&f, KEY_Down, 0);
    key(&f, KEY_Space, 0);
    CHECK(f.toggle == 1);
    key(&f, KEY_Escape, 0);
    CHECK(f.backs == 1 && f.keys == 0);
    ui_event(&f.ui, &(ui_event_t){UI_PRESS, {BIND_KEY, KEY_Tab}, 0, 0, 0,
        true}, 0);
    CHECK(f.keys == 1 && f.shift);
}

static void test_left_right(void)
{
    fixture_t f;

    setup(&f);
    f.ui.focused = SLIDER;
    key(&f, KEY_Right, 0);
    CHECK(f.slider == 55);
    for (int i = 0; i < 30; i++)
        key(&f, KEY_Left, 0);
    CHECK(f.slider == 0);                            /* clamped */
    f.ui.focused = CYCLER;
    f.cycler = 0;
    key(&f, KEY_Left, 0);
    CHECK(f.cycler == 4);                            /* wraps */
    key(&f, KEY_Right, 0);
    CHECK(f.cycler == 0);
    click_at(&f, 1410, 520);                         /* the left arrow */
    CHECK(f.cycler == 4);
    click_at(&f, 1700, 520);
    CHECK(f.cycler == 0);
    setup(&f);                                       /* on a button: they move */
    key(&f, KEY_Right, 0);
    CHECK(f.ui.focused == TOGGLE);
    key(&f, KEY_Right, 0);
    CHECK(f.ui.focused == TOGGLE && f.toggle == 0);  /* only from a button */
    setup(&f);
    key(&f, KEY_Left, 0);
    CHECK(f.ui.focused == LIST);
}

/* A held arrow: first repeat after 300 ms, then every 80 ms (3.5). */
static void test_key_repeat(void)
{
    fixture_t f;
    ui_event_t down = {UI_PRESS, {BIND_KEY, KEY_Right}, 0, 0, 0, false};
    ui_event_t up = {UI_RELEASE, {BIND_KEY, KEY_Right}, 0, 0, 0, false};

    setup(&f);
    f.ui.focused = SLIDER;
    f.slider = 0;
    ui_event(&f.ui, &down, 1000);
    CHECK(f.slider == 5);
    ui_update(&f.ui, 1299);
    CHECK(f.slider == 5);
    ui_update(&f.ui, 1300);
    CHECK(f.slider == 10);
    ui_update(&f.ui, 1379);
    CHECK(f.slider == 10);
    ui_update(&f.ui, 1380);
    CHECK(f.slider == 15);
    ui_update(&f.ui, 9000);                          /* a hitch: no burst */
    CHECK(f.slider == 35);
    ui_event(&f.ui, &up, 9001);
    ui_update(&f.ui, 20000);
    CHECK(f.slider == 35);
}

/* The keybind's modal: only the next press counts (5.5). */
static void test_capture(void)
{
    fixture_t f;

    setup(&f);
    click_at(&f, 1500, 620);                         /* opens it */
    CHECK(f.ui.capturing == KEYBIND);
    send(&f, UI_PRESS, (binding_t){BIND_MOUSE, MOUSE_Right}, 0, 0);
    CHECK(f.ui.capturing == -1);
    CHECK(binding_equal(f.bound, (binding_t){BIND_MOUSE, MOUSE_Right}));
    CHECK(f.changes == 1);
    f.ui.focused = KEYBIND;
    key(&f, KEY_Enter, 0);                           /* its release: ignored */
    CHECK(f.ui.capturing == KEYBIND);
    key(&f, KEY_Escape, 0);
    CHECK(f.ui.capturing == -1 && f.backs == 0);
    CHECK(binding_equal(f.bound, (binding_t){BIND_MOUSE, MOUSE_Right}));
    key(&f, KEY_Enter, 0);
    key(&f, KEY_Backspace, 0);
    CHECK(f.bound.kind == BIND_NONE && f.changes == 2);
    key(&f, KEY_Enter, 0);
    send(&f, UI_PRESS, (binding_t){BIND_JOY, 3}, 0, 0);
    CHECK(binding_equal(f.bound, (binding_t){BIND_JOY, 3}));
    f.w[KEYBIND].on_capture = on_capture;            /* the screen decides */
    key(&f, KEY_Enter, 0);
    key(&f, KEY_W, 0);
    CHECK(f.captures == 1 && binding_equal(f.captured, (binding_t){BIND_KEY,
        KEY_W}));
    CHECK(binding_equal(f.bound, (binding_t){BIND_JOY, 3}));
}

static void test_list(void)
{
    fixture_t f;

    setup(&f);
    f.ui.focused = LIST;
    for (int i = 0; i < 4; i++)
        key(&f, KEY_Down, 0);
    CHECK(f.row == 4 && f.w[LIST].scroll == 2);      /* kept visible */
    key(&f, KEY_Up, 0);
    key(&f, KEY_Up, 0);
    key(&f, KEY_Up, 0);
    CHECK(f.row == 1 && f.w[LIST].scroll == 1);
    for (int i = 0; i < 8; i++)
        key(&f, KEY_Down, 0);
    CHECK(f.row == 9 && f.ui.focused == LIST);
    key(&f, KEY_Down, 0);
    CHECK(f.ui.focused == BUTTON);                   /* off the end: next widget */
    send(&f, UI_WHEEL, (binding_t){BIND_NONE, 0}, 1500, 750);
    f.ui.widgets[LIST].scroll = 0;
    ui_event(&f.ui, &(ui_event_t){UI_WHEEL, {BIND_NONE, 0}, 1500, 750, -1,
        false}, 0);
    CHECK(f.w[LIST].scroll == 3);
    for (int i = 0; i < 5; i++)
        ui_event(&f.ui, &(ui_event_t){UI_WHEEL, {BIND_NONE, 0}, 1500, 750,
            -1, false}, 0);
    CHECK(f.w[LIST].scroll == 7);                    /* the last 3 rows */
    f.w[LIST].scroll = 0;
    f.activations = 0;
    click_at(&f, 1500, 700 + UI_ROW_H * 1.5f);
    CHECK(f.row == 1 && f.activations == 0);         /* chosen */
    click_at(&f, 1500, 700 + UI_ROW_H * 1.5f);
    CHECK(f.activations == 1);                       /* then activated */
    f.ui.focused = LIST;
    key(&f, KEY_Enter, 0);
    CHECK(f.activations == 2);
}

static void test_looks(void)
{
    fixture_t f;
    ui_look_t look;

    setup(&f);
    move(&f, 1500, 120);
    look = ui_look(&f.ui, BUTTON);
    CHECK(look.state == UI_HOVER && look.scale == 1.05f && look.outline);
    press_at(&f, 1500, 120);
    CHECK(ui_look(&f.ui, BUTTON).state == UI_PRESSED);
    CHECK(ui_look(&f.ui, BUTTON).scale == 0.92f);
    move(&f, 100, 120);
    CHECK(ui_look(&f.ui, BUTTON).state != UI_PRESSED);   /* dragged out */
    release_at(&f, 100, 120);
    press_at(&f, 100, 220);                          /* held down elsewhere */
    move(&f, 1500, 220);
    CHECK(ui_look(&f.ui, TOGGLE).state != UI_HOVER);
    release_at(&f, 1500, 220);
    CHECK(f.toggle == 0);
    move(&f, 1500, 520);
    CHECK(f.ui.focused == CYCLER);                   /* hover moves focus */
    look = ui_look(&f.ui, OFF);
    CHECK(look.state == UI_DISABLED && look.alpha == 0.5f && !look.outline);
    key(&f, KEY_Down, 0);                            /* from the hovered cycler */
    CHECK(ui_look(&f.ui, KEYBIND).state == UI_FOCUSED);
    CHECK(ui_look(&f.ui, BUTTON).state == UI_IDLE);
}

/* A dialog on top gets everything; the screen under it nothing. */
static void test_modal(void)
{
    fixture_t f;
    fixture_t dialog;

    setup(&f);
    setup(&dialog);
    f.ui.modal = &dialog.ui;
    click_at(&f, 1500, 120);
    key(&f, KEY_Escape, 0);
    CHECK(f.activations == 0 && f.backs == 0);
    CHECK(dialog.activations == 1 && dialog.backs == 1);
    f.ui.modal = NULL;
    click_at(&f, 1500, 120);
    CHECK(f.activations == 1);
}

static void type(fixture_t *f, const char *s)
{
    for (; *s != '\0'; s++)
        ui_event(&f->ui, &(ui_event_t){.kind = UI_TEXT,
            .ch = (unsigned char)*s}, 0);
}

/*
** A text field: typed characters go in while it has the focus, Backspace
** erases, Enter confirms, and no key is a shortcut meanwhile (11.6).
*/
static void test_text_field(void)
{
    fixture_t f;
    char name[8] = "ab";

    setup(&f);
    f.w[0] = (widget_t){.kind = W_TEXT, .label = "Name", .bounds = row_at(0),
        .enabled = true, .text = name, .text_cap = sizeof(name),
        .on_change = on_change, .on_activate = on_activate};
    f.ui.focused = 0;
    CHECK(ui_typing(&f.ui));
    type(&f, "c d");
    CHECK(strcmp(name, "abc d") == 0 && f.changes == 3);
    type(&f, "efgh");                          /* room for two more only */
    CHECK(strcmp(name, "abc def") == 0);
    type(&f, "\b\n\x7f");                      /* control codes type nothing */
    CHECK(strcmp(name, "abc def") == 0);
    key(&f, KEY_Backspace, 0);
    CHECK(strcmp(name, "abc de") == 0);
    key(&f, KEY_Space, 0);                     /* a letter, not "activate" */
    key(&f, KEY_S, 0);                         /* nor a shortcut */
    CHECK(f.activations == 0 && f.keys == 0);
    key(&f, KEY_Enter, 0);
    CHECK(f.activations == 1);
    click_at(&f, 1500, 120);                   /* a click only focuses it */
    CHECK(f.activations == 1 && ui_typing(&f.ui));
    key(&f, KEY_Escape, 0);
    CHECK(f.backs == 1);
    key(&f, KEY_Down, 0);                      /* the arrows still leave it */
    CHECK(f.ui.focused == 1 && !ui_typing(&f.ui));
    type(&f, "zz");
    CHECK(strcmp(name, "abc de") == 0);        /* not focused: not typed */
    key(&f, KEY_S, 0);
    CHECK(f.keys == 1);
    name[0] = '\0';
    f.ui.focused = 0;
    key(&f, KEY_Backspace, 0);                 /* empty: nothing to erase */
    CHECK(name[0] == '\0');
}

void test_ui(void)
{
    test_button_activates_on_release();
    test_toggle_and_disabled();
    test_slider_capture();
    test_keyboard_navigation();
    test_left_right();
    test_key_repeat();
    test_capture();
    test_list();
    test_looks();
    test_modal();
    test_text_field();
}
