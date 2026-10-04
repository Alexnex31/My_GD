/*
** ALEXNEX PROJECT, 2026
** ui_sfml
** File description:
** the widget toolkit's window side: SFML events in, widgets drawn (FEATURES 3)
*/

#include "mygd.h"

#define TEXT_SIZE 36
#define LABEL_GAP 40.0f               /* a label ends this far left of its widget */
#define OUTLINE_W 4.0f

static const sfColor PANEL = {25, 30, 70, 230};
static const sfColor ACCENT = {80, 220, 120, 255};
static const sfColor TRACK = {10, 10, 30, 255};
static const sfColor DIM = {0, 0, 0, 170};

/* One shape and one text, reused for every widget: nothing per frame (3.6). */
void ui_gfx_create(gd_t *gd)
{
    gd->ui_shape = sfRectangleShape_create();
    gd->ui_text = sfText_create();
    sfText_setFont(gd->ui_text, gd->main_font);
    sfText_setCharacterSize(gd->ui_text, TEXT_SIZE);
    sfText_setOutlineThickness(gd->ui_text, 2.0f);
}

void ui_gfx_free(gd_t *gd)
{
    sfRectangleShape_destroy(gd->ui_shape);
    sfText_destroy(gd->ui_text);
}

/*
** SFML reads a char * in the locale's encoding, not UTF-8: song titles and
** the em dash go through this. A static buffer: sfText copies it at once.
*/
const sfUint32 *utf8_to_utf32(const char *s)
{
    static sfUint32 out[512];
    const unsigned char *p = (const unsigned char *)s;
    size_t n = 0;

    while (*p != '\0' && n < 511) {
        int extra = *p >= 0xF0 ? 3 : (*p >= 0xE0 ? 2 : (*p >= 0xC0 ? 1 : 0));
        sfUint32 c = *p++ & (0x7Fu >> extra);

        for (int i = 0; i < extra && (*p & 0xC0) == 0x80; i++)
            c = (c << 6) | (*p++ & 0x3Fu);
        out[n++] = c;
    }
    out[n] = 0;
    return out;
}

int64_t ui_now_ms(void)
{
    return input_clock_us() / 1000;
}

/* Window pixels to the UI view's 1920x1080 (3.1): letterboxing included. */
static void ui_pos(gd_t *gd, int px, int py, ui_event_t *out)
{
    sfVector2f p = sfRenderWindow_mapPixelToCoords(gd->w,
        (sfVector2i){px, py}, gd->ui_view);

    out->x = p.x;
    out->y = p.y;
}

static bool mouse_event(gd_t *gd, const sfEvent *ev, ui_event_t *out)
{
    if (ev->type == sfEvtMouseMoved) {
        out->kind = UI_MOVE;
        ui_pos(gd, ev->mouseMove.x, ev->mouseMove.y, out);
        return true;
    }
    if (ev->type == sfEvtMouseWheelScrolled
        && ev->mouseWheelScroll.wheel == sfMouseVerticalWheel) {
        out->kind = UI_WHEEL;
        out->wheel = ev->mouseWheelScroll.delta > 0.0f ? 1 : -1;
        ui_pos(gd, ev->mouseWheelScroll.x, ev->mouseWheelScroll.y, out);
        return true;
    }
    if ((ev->type != sfEvtMouseButtonPressed
        && ev->type != sfEvtMouseButtonReleased)
        || (int)ev->mouseButton.button >= MOUSE_COUNT)
        return false;
    out->kind = ev->type == sfEvtMouseButtonPressed ? UI_PRESS : UI_RELEASE;
    out->input = (binding_t){BIND_MOUSE, (int)ev->mouseButton.button};
    ui_pos(gd, ev->mouseButton.x, ev->mouseButton.y, out);
    return true;
}

/* An SFML event as the toolkit's, or false if it has nothing to do with it. */
bool ui_from_sf(gd_t *gd, const sfEvent *ev, ui_event_t *out)
{
    *out = (ui_event_t){0};
    if ((ev->type == sfEvtKeyPressed || ev->type == sfEvtKeyReleased)
        && ev->key.code >= 0 && (int)ev->key.code < KEY_COUNT) {
        out->kind = ev->type == sfEvtKeyPressed ? UI_PRESS : UI_RELEASE;
        out->input = (binding_t){BIND_KEY, (int)ev->key.code};
        out->shift = ev->key.shift;
        return true;
    }
    if ((ev->type == sfEvtJoystickButtonPressed
        || ev->type == sfEvtJoystickButtonReleased)
        && ev->joystickButton.joystickId == 0
        && ev->joystickButton.button < JOY_BUTTONS) {
        out->kind = ev->type == sfEvtJoystickButtonPressed ? UI_PRESS
            : UI_RELEASE;
        out->input = (binding_t){BIND_JOY, (int)ev->joystickButton.button};
        return true;
    }
    return mouse_event(gd, ev, out);
}

static sfColor faded(sfColor c, float alpha)
{
    c.a = (sfUint8)((float)c.a * alpha);
    return c;
}

static void rect(gd_t *gd, ui_rect_t r, sfColor fill, float outline)
{
    sfRectangleShape_setPosition(gd->ui_shape, (sfVector2f){r.x, r.y});
    sfRectangleShape_setSize(gd->ui_shape, (sfVector2f){r.w, r.h});
    sfRectangleShape_setFillColor(gd->ui_shape, fill);
    sfRectangleShape_setOutlineThickness(gd->ui_shape, outline);
    sfRectangleShape_setOutlineColor(gd->ui_shape, faded(sfWhite,
        fill.a / 255.0f));
    draw_rect(gd, gd->ui_shape);
}

/* align: 0 left at x, 1 centered on x, 2 right ending at x. */
static void text(gd_t *gd, const char *s, sfVector2f at, int align,
    float alpha)
{
    sfFloatRect b;

    sfText_setUnicodeString(gd->ui_text, utf8_to_utf32(s));
    b = sfText_getLocalBounds(gd->ui_text);
    sfText_setOrigin(gd->ui_text, (sfVector2f){b.left + b.width * align
        / 2.0f, b.top + b.height / 2.0f});
    sfText_setPosition(gd->ui_text, at);
    sfText_setFillColor(gd->ui_text, faded(sfWhite, alpha));
    sfText_setOutlineColor(gd->ui_text, faded(sfBlack, alpha));
    draw_text(gd, gd->ui_text);
}

/* The bounds grown or shrunk around their center (3.2). */
static ui_rect_t scaled(ui_rect_t r, float s)
{
    return (ui_rect_t){r.x + r.w * (1.0f - s) / 2.0f,
        r.y + r.h * (1.0f - s) / 2.0f, r.w * s, r.h * s};
}

static sfVector2f center(ui_rect_t r)
{
    return (sfVector2f){r.x + r.w / 2.0f, r.y + r.h / 2.0f};
}

static void draw_slider(gd_t *gd, const widget_t *w, ui_rect_t r, float a)
{
    float f = ui_slider_fraction(w);
    char value[16];

    rect(gd, (ui_rect_t){r.x + 10.0f, r.y + r.h / 2.0f - 6.0f, r.w - 20.0f,
        12.0f}, faded(TRACK, a), 0.0f);
    rect(gd, (ui_rect_t){r.x + 10.0f, r.y + r.h / 2.0f - 6.0f,
        (r.w - 20.0f) * f, 12.0f}, faded(ACCENT, a), 0.0f);
    rect(gd, (ui_rect_t){r.x + 10.0f + (r.w - 20.0f) * f - 10.0f,
        r.y + 8.0f, 20.0f, r.h - 16.0f}, faded(sfWhite, a), 0.0f);
    snprintf(value, sizeof(value), "%d", *w->value);
    text(gd, value, (sfVector2f){r.x + r.w + 20.0f, r.y + r.h / 2.0f}, 0, a);
}

static void draw_list(gd_t *gd, const widget_t *w, ui_rect_t r, float a)
{
    int rows = ui_list_rows(w);

    for (int i = 0; i < rows && w->scroll + i <= w->max; i++) {
        int row = w->scroll + i;
        ui_rect_t line = {r.x, r.y + (float)i * UI_ROW_H, r.w, UI_ROW_H};

        if (row == *w->value)
            rect(gd, line, faded(ACCENT, 0.6f * a), 0.0f);
        text(gd, w->choices[row], (sfVector2f){line.x + 20.0f,
            line.y + UI_ROW_H / 2.0f}, 0, a);
    }
}

static void draw_content(gd_t *gd, const widget_t *w, ui_rect_t r, float a)
{
    char name[BINDING_NAME_MAX];

    if (w->kind == W_BUTTON)
        text(gd, w->label, center(r), 1, a);
    if (w->kind == W_TOGGLE) {
        if (*w->value)
            rect(gd, scaled(r, 0.8f), faded(ACCENT, a), 0.0f);
        text(gd, *w->value ? "ON" : "OFF", center(r), 1, a);
    }
    if (w->kind == W_SLIDER)
        draw_slider(gd, w, r, a);
    if (w->kind == W_CYCLER) {
        text(gd, "<", (sfVector2f){r.x + 20.0f, center(r).y}, 0, a);
        text(gd, w->choices[*w->value - w->min], center(r), 1, a);
        text(gd, ">", (sfVector2f){r.x + r.w - 20.0f, center(r).y}, 2, a);
    }
    if (w->kind == W_KEYBIND) {
        binding_format(*w->binding, name, sizeof(name));
        text(gd, name[0] != '\0' ? name : "-", center(r), 1, a);
    }
    if (w->kind == W_LIST)
        draw_list(gd, w, r, a);
}

static void draw_widget(gd_t *gd, const ui_screen_t *ui, int i)
{
    const widget_t *w = &ui->widgets[i];
    ui_look_t look = ui_look(ui, i);
    ui_rect_t r = scaled(w->bounds, look.scale);

    if (w->kind != W_BUTTON && w->label != NULL)
        text(gd, w->label, (sfVector2f){ui->label_x > 0.0f ? ui->label_x
            : w->bounds.x - LABEL_GAP, center(w->bounds).y},
            ui->label_x > 0.0f ? 0 : 2, look.alpha);
    rect(gd, r, faded(PANEL, look.alpha), look.outline ? OUTLINE_W : 0.0f);
    draw_content(gd, w, r, look.alpha);
}

/* The keybind's modal (5.5), over everything. */
static void draw_capture(gd_t *gd, const widget_t *w)
{
    char line[160];

    rect(gd, (ui_rect_t){0.0f, 0.0f, VIEW_W, VIEW_H}, DIM, 0.0f);
    rect(gd, (ui_rect_t){360.0f, 390.0f, 1200.0f, 300.0f}, PANEL, OUTLINE_W);
    snprintf(line, sizeof(line), "Press a key, mouse button or gamepad"
        " button for %s", w->label != NULL ? w->label : "this action");
    text(gd, line, (sfVector2f){VIEW_W / 2.0f, 500.0f}, 1, 1.0f);
    text(gd, "Esc to cancel, Backspace to clear",
        (sfVector2f){VIEW_W / 2.0f, 580.0f}, 1, 1.0f);
}

/* In the UI view. A dialog on top is drawn over a dimmed screen. */
void ui_draw(gd_t *gd, const ui_screen_t *ui)
{
    sfRenderWindow_setView(gd->w, gd->ui_view);
    for (int i = 0; i < ui->count; i++)
        draw_widget(gd, ui, i);
    if (ui->capturing >= 0)
        draw_capture(gd, &ui->widgets[ui->capturing]);
    if (ui->modal != NULL) {
        rect(gd, (ui_rect_t){0.0f, 0.0f, VIEW_W, VIEW_H}, DIM, 0.0f);
        ui_draw(gd, ui->modal);
    }
}
