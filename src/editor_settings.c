/*
** ALEXNEX PROJECT, 2026
** editor_settings
** File description:
** the level's settings window: its song and how its attempts start (FEATURES 11.16)
*/

#include "editor.h"

    #define WIN_X 460.0f
    #define WIN_Y 110.0f
    #define WIN_W 1000.0f
    #define WIN_H 880.0f
    #define LABEL_X (WIN_X + 100.0f)
    #define ROW_X (WIN_X + 540.0f)
    #define ROW_W 400.0f
    #define ROW_H 60.0f
    #define ROW_STEP 72.0f
    #define SONG_Y 245.0f             /* the Song group's first row ... */
    #define START_Y 590.0f            /* ... and the Start group's     */
    #define CLOSE ED_SET_COUNT        /* the widget after the rows     */

static const sfColor WINDOW = {18, 20, 32, 250};
static const sfColor EDGE = {90, 255, 140, 255};
static const sfColor GROUP = {90, 255, 140, 255};
static const sfColor QUIET = {180, 190, 210, 255};

/* GD's settings are a button of the editor's too: the toolbar's right end. */
static const ui_rect_t BUTTON = {VIEW_W - 300.0f, 12.0f, 280.0f, 56.0f};

static float row_y(int s)
{
    if (s < ED_SET_FIRST_START)
        return SONG_Y + ROW_STEP * (float)s;
    return START_Y + ROW_STEP * (float)(s - ED_SET_FIRST_START);
}

/* What a row asked for, done after the event with the song library. */
static void stepped(void *ctx, widget_t *w, int dir)
{
    editor_t *ed = ctx;

    ed->settings_row = (int)(w - ed->settings_widgets);
    ed->settings_dir = dir;
}

static void close_pressed(void *ctx, widget_t *w)
{
    (void)w;
    ((editor_t *)ctx)->settings_open = false;
}

static void back(void *ctx)
{
    ((editor_t *)ctx)->settings_open = false;
}

/*
** One row per setting, then Close. Every row is a stepper: the window shows
** what the level says and steps it, as the properties panel does (11.4).
*/
void editor_settings_open(editor_t *ed)
{
    for (int s = 0; s < ED_SET_COUNT; s++)
        ed->settings_widgets[s] = (widget_t){.kind = W_STEPPER,
            .label = ed_setting_name((ed_setting_t)s), .enabled = true,
            .bounds = {ROW_X, row_y(s), ROW_W, ROW_H},
            .text = ed->settings_text[s], .on_step = stepped};
    ed->settings_widgets[CLOSE] = (widget_t){.kind = W_BUTTON,
        .label = "Close", .enabled = true, .on_activate = close_pressed,
        .bounds = {VIEW_W / 2.0f - 150.0f, WIN_Y + WIN_H - 85.0f, 300.0f,
        ROW_H}};
    ui_init(&ed->settings_ui, ed->settings_widgets, CLOSE + 1, ed);
    ed->settings_ui.focused = 0;
    ed->settings_ui.label_x = LABEL_X;
    ed->settings_ui.on_back = back;
    ed->settings_row = -1;
    ed->settings_open = true;
    ed->repeating = false;                   /* the canvas's held key stops */
}

/*
** Shift makes the song rows' steps bigger, as the panel's did (11.11). A
** new song stops the one the canvas plays; anything measured from the
** level is measured again.
*/
static void apply_step(editor_t *ed, gd_t *gd)
{
    ed_setting_t s = (ed_setting_t)ed->settings_row;
    bool big = sfKeyboard_isKeyPressed(sfKeyLShift)
        || sfKeyboard_isKeyPressed(sfKeyRShift);

    if (ed->settings_row < 0)
        return;
    ed->settings_row = -1;
    ed_gesture(&ed->hist);
    if (!ed_setting_step(&ed->lv, &ed->hist, &gd->library, s,
        ed->settings_dir, big))
        return;
    if (s == ED_SET_SONG)
        editor_song_stop(ed, gd);
    ed->measured = false;
}

/* The key that opened it closes it too, as a combination (11.6). */
static bool toggle_key(gd_t *gd, const sfEvent *ev)
{
    binding_t b;

    if (ev->type != sfEvtKeyPressed)
        return false;
    b = (binding_t){BIND_KEY, (int)ev->key.code, (ev->key.control
        ? MOD_CTRL : 0) | (ev->key.shift ? MOD_SHIFT : 0)
        | (ev->key.alt ? MOD_ALT : 0)};
    return binding_equal(b, gd->settings.editor_keys[ED_KEY_LEVEL_PANEL]);
}

/* It has every event while it is open: the canvas waits under it. */
void editor_settings_events(editor_t *ed, gd_t *gd)
{
    ui_event_t ev;

    while (ed->settings_open && poll_event(gd)) {
        if (gd->event->type == sfEvtClosed)
            return editor_close(ed, gd);
        if (toggle_key(gd, gd->event)) {
            ed->settings_open = false;
            return;
        }
        if (ui_from_sf(gd, gd->event, &ev))
            ui_event(&ed->settings_ui, &ev, ui_now_ms());
        apply_step(ed, gd);
    }
}

static void group(editor_t *ed, gd_t *gd, const char *name, float y)
{
    editor_label(ed, gd, name, LABEL_X - 40.0f, y, 30, GROUP);
}

void editor_settings_draw(editor_t *ed, gd_t *gd)
{
    char hint[96];
    char key[BINDING_NAME_MAX];

    if (!ed->settings_open)
        return;
    ui_update(&ed->settings_ui, ui_now_ms());    /* a held arrow repeats */
    apply_step(ed, gd);
    for (int s = 0; s < ED_SET_COUNT; s++)
        ed_setting_text(&ed->lv, &gd->library, (ed_setting_t)s,
            ed->settings_text[s], sizeof(ed->settings_text[s]));
    editor_fill(gd, ed, (sfFloatRect){0.0f, 0.0f, VIEW_W, VIEW_H},
        (sfColor){0, 0, 0, 150}, sfTransparent);
    editor_fill(gd, ed, (sfFloatRect){WIN_X, WIN_Y, WIN_W, WIN_H}, WINDOW,
        EDGE);
    editor_label(ed, gd, "Level settings", LABEL_X - 40.0f, WIN_Y + 20.0f, 40,
        sfWhite);
    group(ed, gd, "Song", SONG_Y - 50.0f);
    group(ed, gd, "Start", START_Y - 50.0f);
    editor_key_name(gd, ED_KEY_LEVEL_PANEL, key, sizeof(key));
    snprintf(hint, sizeof(hint), "Shift: bigger song steps   -   Escape or "
        "%s: close", key);
    editor_label(ed, gd, hint, WIN_X + WIN_W - 40.0f
        - editor_label_width(ed, hint, 20), WIN_Y + 34.0f, 20, QUIET);
    ui_draw(gd, &ed->settings_ui);
}

bool editor_settings_button_holds(sfVector2f at)
{
    return ui_rect_contains(BUTTON, at.x, at.y);
}

void editor_settings_button_draw(editor_t *ed, gd_t *gd)
{
    char key[BINDING_NAME_MAX];
    char text[64];

    editor_key_name(gd, ED_KEY_LEVEL_PANEL, key, sizeof(key));
    snprintf(text, sizeof(text), "Level settings (%s)", key);
    editor_fill(gd, ed, (sfFloatRect){BUTTON.x, BUTTON.y, BUTTON.w, BUTTON.h},
        (sfColor){60, 110, 200, 255}, sfTransparent);
    editor_label_fit(ed, gd, text, (sfVector2f){BUTTON.x + 16.0f,
        BUTTON.y + 14.0f}, BUTTON.w - 32.0f, 22, sfWhite);
}
