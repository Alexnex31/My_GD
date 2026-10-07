/*
** ALEXNEX PROJECT, 2026
** ui/ui.c
** File description:
** the widget toolkit: focus, presses, capture, keys (FEATURES 3)
*/

#include <math.h>
#include <string.h>

#include "ui/ui.h"

static const binding_t NO_KEY = {BIND_NONE, 0};

bool ui_rect_contains(ui_rect_t r, float x, float y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static bool is_key(binding_t b, key_id_t k)
{
    return b.kind == BIND_KEY && b.code == (int)k;
}

static bool is_left_click(binding_t b)
{
    return b.kind == BIND_MOUSE && b.code == MOUSE_Left;
}

static bool usable(const ui_screen_t *ui, int i)
{
    return i >= 0 && i < ui->count && ui->widgets[i].enabled;
}

static void changed(ui_screen_t *ui, widget_t *w)
{
    if (w->on_change != NULL)
        w->on_change(ui->ctx, w);
}

static void set_value(ui_screen_t *ui, widget_t *w, int v)
{
    if (v == *w->value)
        return;                              /* no spam while dragging (3.5) */
    *w->value = v;
    changed(ui, w);
}

/* The topmost enabled widget under the point, or -1. */
static int widget_at(const ui_screen_t *ui, float x, float y)
{
    for (int i = ui->count - 1; i >= 0; i--)
        if (ui_rect_contains(ui->widgets[i].bounds, x, y))
            return usable(ui, i) ? i : -1;
    return -1;
}

void ui_init(ui_screen_t *ui, widget_t *widgets, int count, void *ctx)
{
    *ui = (ui_screen_t){.widgets = widgets, .count = count, .focused = -1,
        .hovered = -1, .pressed = -1, .captured = -1, .capturing = -1,
        .repeat_key = NO_KEY, .ctx = ctx};
    for (int i = 0; i < count && ui->focused < 0; i++)
        if (usable(ui, i))
            ui->focused = i;
}

float ui_slider_fraction(const widget_t *w)
{
    if (w->max <= w->min)
        return 0.0f;
    return (float)(*w->value - w->min) / (float)(w->max - w->min);
}

int ui_list_rows(const widget_t *w)
{
    int rows = (int)(w->bounds.h / UI_ROW_H);

    return rows < 1 ? 1 : rows;
}

/* The slider's value under the mouse, on its step grid (3.5). */
static void slider_follow(ui_screen_t *ui, widget_t *w, float x)
{
    float t = (x - w->bounds.x) / w->bounds.w;
    int step = w->step > 0 ? w->step : 1;
    int v = 0;

    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    v = w->min + (int)roundf(t * (float)(w->max - w->min) / (float)step)
        * step;
    set_value(ui, w, v > w->max ? w->max : v);
}

static void slider_step(ui_screen_t *ui, widget_t *w, int dir)
{
    int v = *w->value + dir * (w->step > 0 ? w->step : 1);

    set_value(ui, w, v < w->min ? w->min : (v > w->max ? w->max : v));
}

static void cycler_step(ui_screen_t *ui, widget_t *w, int dir)
{
    int span = w->max - w->min + 1;
    int v = w->min + ((*w->value - w->min + dir) % span + span) % span;

    set_value(ui, w, v);
}

/* The selection stays visible: the list scrolls to it. */
static void list_select(ui_screen_t *ui, widget_t *w, int row)
{
    int rows = ui_list_rows(w);

    set_value(ui, w, row);
    if (row < w->scroll)
        w->scroll = row;
    if (row >= w->scroll + rows)
        w->scroll = row - rows + 1;
}

static void list_scroll(widget_t *w, int by)
{
    int last = w->max + 1 - ui_list_rows(w);

    w->scroll += by;
    if (w->scroll > last)
        w->scroll = last;
    if (w->scroll < 0)
        w->scroll = 0;
}

/* Enter, Space, or a click released where it started (3.3). */
static void activate(ui_screen_t *ui, int i)
{
    widget_t *w = &ui->widgets[i];

    if (w->kind == W_TOGGLE)
        set_value(ui, w, *w->value ? 0 : 1);
    if (w->kind == W_CYCLER)
        cycler_step(ui, w, 1);
    if (w->kind == W_KEYBIND)
        ui->capturing = i;               /* the next press is the result (5.5) */
    if ((w->kind == W_BUTTON || w->kind == W_LIST || w->kind == W_TEXT)
        && w->on_activate != NULL)
        w->on_activate(ui->ctx, w);
}

bool ui_typing(const ui_screen_t *ui)
{
    if (ui->modal != NULL)
        return ui_typing(ui->modal);
    return usable(ui, ui->focused) && ui->widgets[ui->focused].kind == W_TEXT;
}

static bool arrow(ui_screen_t *ui, binding_t key);

/* A printable character goes at the end, while there is room for it. */
static void type_char(ui_screen_t *ui, unsigned int ch)
{
    widget_t *w = &ui->widgets[ui->focused];
    int len = (int)strlen(w->text);

    if (ch < 32 || ch > 126 || len + 1 >= w->text_cap)
        return;
    w->text[len] = (char)ch;
    w->text[len + 1] = '\0';
    changed(ui, w);
}

/*
** A focused text field's keys: Backspace erases, Enter confirms, Up and Down
** still leave it, and every other key is a letter that UI_TEXT brings, so
** Space types a space and no shortcut fires (FEATURES 11.6).
*/
static void text_key(ui_screen_t *ui, const ui_event_t *ev, int64_t now)
{
    widget_t *w = &ui->widgets[ui->focused];
    int len = (int)strlen(w->text);

    if (is_key(ev->input, KEY_Backspace) && len > 0) {
        w->text[len - 1] = '\0';
        changed(ui, w);
    }
    if (is_key(ev->input, KEY_Enter))
        activate(ui, ui->focused);
    if (is_key(ev->input, KEY_Escape) && ui->on_back != NULL)
        return ui->on_back(ui->ctx);
    if ((is_key(ev->input, KEY_Up) || is_key(ev->input, KEY_Down))
        && arrow(ui, ev->input)) {
        ui->repeat_key = ev->input;
        ui->repeat_at_ms = now + UI_REPEAT_DELAY_MS;
    }
}

/* A cycler's left third goes back; a list's row is chosen, then activated. */
static void click(ui_screen_t *ui, int i, float x, float y)
{
    widget_t *w = &ui->widgets[i];
    int row = 0;

    if (w->kind == W_CYCLER && x < w->bounds.x + w->bounds.w / 3.0f)
        return cycler_step(ui, w, -1);
    if (w->kind == W_TEXT)
        return;                              /* a click focuses it: Enter confirms */
    if (w->kind != W_LIST)
        return activate(ui, i);
    row = w->scroll + (int)((y - w->bounds.y) / UI_ROW_H);
    if (row < w->min || row > w->max)
        return;
    if (row == *w->value)
        return activate(ui, i);
    list_select(ui, w, row);
}

static void mouse_move(ui_screen_t *ui, float x, float y)
{
    ui->mouse_x = x;
    ui->mouse_y = y;
    if (ui->captured >= 0)
        return slider_follow(ui, &ui->widgets[ui->captured], x);
    ui->hovered = widget_at(ui, x, y);
    if (ui->hovered >= 0)
        ui->focused = ui->hovered;           /* hover moves focus (3.4) */
}

static void mouse_down(ui_screen_t *ui, float x, float y)
{
    mouse_move(ui, x, y);
    ui->mouse_down = true;
    ui->pressed = widget_at(ui, x, y);
    if (ui->pressed < 0)
        return;
    ui->focused = ui->pressed;
    if (ui->widgets[ui->pressed].kind == W_SLIDER) {
        ui->captured = ui->pressed;          /* follows the mouse anywhere */
        slider_follow(ui, &ui->widgets[ui->pressed], x);
    }
}

static void mouse_up(ui_screen_t *ui, float x, float y)
{
    int started = ui->pressed;

    ui->mouse_down = false;
    ui->pressed = -1;
    if (ui->captured >= 0) {
        ui->captured = -1;
        return mouse_move(ui, x, y);
    }
    if (started >= 0 && widget_at(ui, x, y) == started)
        click(ui, started, x, y);
}

/* The next enabled widget that way, wrapping; a list moves its row first. */
static void navigate(ui_screen_t *ui, int dir)
{
    widget_t *w = usable(ui, ui->focused) ? &ui->widgets[ui->focused] : NULL;

    if (w != NULL && w->kind == W_LIST && *w->value + dir >= w->min
        && *w->value + dir <= w->max)
        return list_select(ui, w, *w->value + dir);
    for (int n = 1; n <= ui->count; n++) {
        int i = ((ui->focused < 0 ? (dir > 0 ? -1 : 0) : ui->focused)
            + dir * n % ui->count + ui->count) % ui->count;

        if (usable(ui, i)) {
            ui->focused = i;
            return;
        }
    }
}

static void adjust(ui_screen_t *ui, int dir)
{
    widget_t *w = NULL;

    if (!usable(ui, ui->focused))
        return;
    w = &ui->widgets[ui->focused];
    if (w->kind == W_BUTTON)
        navigate(ui, dir);                   /* a dialog's buttons, side by side */
    if (w->kind == W_SLIDER)
        slider_step(ui, w, dir);
    if (w->kind == W_CYCLER)
        cycler_step(ui, w, dir);
}

/* What an arrow does, on its press and on each repeat. */
static bool arrow(ui_screen_t *ui, binding_t key)
{
    if (is_key(key, KEY_Up) || is_key(key, KEY_Down))
        navigate(ui, is_key(key, KEY_Up) ? -1 : 1);
    else if (is_key(key, KEY_Left) || is_key(key, KEY_Right))
        adjust(ui, is_key(key, KEY_Left) ? -1 : 1);
    else
        return false;
    return true;
}

static void key_press(ui_screen_t *ui, const ui_event_t *ev, int64_t now)
{
    ui->repeat_key = NO_KEY;
    if (ui_typing(ui))
        return text_key(ui, ev, now);
    if (arrow(ui, ev->input)) {
        ui->repeat_key = ev->input;
        ui->repeat_at_ms = now + UI_REPEAT_DELAY_MS;
        return;
    }
    if (is_key(ev->input, KEY_Escape) && ui->on_back != NULL)
        return ui->on_back(ui->ctx);
    if (is_key(ev->input, KEY_Enter) || is_key(ev->input, KEY_Space)) {
        if (usable(ui, ui->focused))
            activate(ui, ui->focused);
        return;
    }
    if (!is_key(ev->input, KEY_Escape) && ui->on_key != NULL)
        ui->on_key(ui->ctx, ev->input, ev->shift);
}

/*
** The modal of a keybind (5.5): Escape cancels, Backspace clears, any other
** key, mouse button or gamepad button is the result. Only presses count, so
** the click that opened it doesn't.
*/
static void capture_event(ui_screen_t *ui, const ui_event_t *ev)
{
    widget_t *w = &ui->widgets[ui->capturing];
    binding_t b = ev->input;

    if (ev->kind == UI_RELEASE && is_left_click(ev->input))
        ui->mouse_down = false;
    if (ev->kind != UI_PRESS)
        return;
    ui->capturing = -1;
    if (is_key(b, KEY_Escape))
        return;
    if (is_key(b, KEY_Backspace))
        b = NO_KEY;
    if (w->on_capture != NULL)
        return w->on_capture(ui->ctx, w, b);
    if (w->binding != NULL && !binding_equal(*w->binding, b)) {
        *w->binding = b;
        changed(ui, w);
    }
}

static void wheel(ui_screen_t *ui, const ui_event_t *ev)
{
    int i = widget_at(ui, ev->x, ev->y);

    if (i >= 0 && ui->widgets[i].kind == W_LIST)
        list_scroll(&ui->widgets[i], -ev->wheel * UI_WHEEL_ROWS);
}

void ui_event(ui_screen_t *ui, const ui_event_t *ev, int64_t now_ms)
{
    if (ui->modal != NULL)
        return ui_event(ui->modal, ev, now_ms);
    if (ui->capturing >= 0)
        return capture_event(ui, ev);
    if (ev->kind == UI_MOVE)
        mouse_move(ui, ev->x, ev->y);
    if (ev->kind == UI_WHEEL)
        wheel(ui, ev);
    if (ev->kind == UI_TEXT && ui_typing(ui))
        type_char(ui, ev->ch);
    if (ev->kind == UI_PRESS && is_left_click(ev->input))
        mouse_down(ui, ev->x, ev->y);
    if (ev->kind == UI_PRESS && ev->input.kind == BIND_KEY)
        key_press(ui, ev, now_ms);
    if (ev->kind == UI_RELEASE && is_left_click(ev->input))
        mouse_up(ui, ev->x, ev->y);
    if (ev->kind == UI_RELEASE && binding_equal(ev->input, ui->repeat_key))
        ui->repeat_key = NO_KEY;
}

/* OS key repeat is off (1.3): a held arrow repeats on this clock instead. */
void ui_update(ui_screen_t *ui, int64_t now_ms)
{
    int caught_up = 0;

    if (ui->modal != NULL)
        return ui_update(ui->modal, now_ms);
    while (ui->repeat_key.kind != BIND_NONE && now_ms >= ui->repeat_at_ms
        && ui->capturing < 0) {
        arrow(ui, ui->repeat_key);
        ui->repeat_at_ms += UI_REPEAT_EVERY_MS;
        if (++caught_up == 4)                /* a hitch doesn't replay a burst */
            ui->repeat_at_ms = now_ms + UI_REPEAT_EVERY_MS;
    }
}

ui_look_t ui_look(const ui_screen_t *ui, int i)
{
    const widget_t *w = &ui->widgets[i];
    bool inside = ui_rect_contains(w->bounds, ui->mouse_x, ui->mouse_y);
    ui_look_t look = {UI_IDLE, 1.0f, 1.0f, i == ui->focused};

    if (!w->enabled)
        return (ui_look_t){UI_DISABLED, 1.0f, 0.5f, false};
    if (ui->pressed == i && ui->mouse_down && (inside || ui->captured == i))
        return look.state = UI_PRESSED, look.scale = 0.92f, look;
    if (inside && !ui->mouse_down && ui->hovered == i)
        return look.state = UI_HOVER, look.scale = 1.05f, look;
    if (i == ui->focused)
        look.state = UI_FOCUSED;
    return look;
}
