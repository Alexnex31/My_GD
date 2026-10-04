/*
** ALEXNEX PROJECT, 2026
** options_rows
** File description:
** the options screen's rows, section by section, and what each one applies (FEATURES 5.2)
*/

#include "mygd.h"

static const char *const FPS_LABELS[] = {"Off", "60", "120", "144", "240"};
static const int FPS_VALUES[] = {0, 60, 120, 144, 240};

void options_notice(option_m_t *om, const char *text)
{
    snprintf(om->notice, sizeof(om->notice), "%s", text);
    om->notice_until_ms = ui_now_ms() + 2000;
}

static option_m_t *menu_of(void *ctx)
{
    ((option_m_t *)ctx)->gd->settings.dirty = true;     /* 5.7 */
    return ctx;
}

static void music_volume_changed(void *ctx, widget_t *w)
{
    option_m_t *om = menu_of(ctx);

    om->gd->settings.music_volume = *w->value;
    music_set_volume(&om->gd->music, *w->value);
}

static void sfx_volume_changed(void *ctx, widget_t *w)
{
    menu_of(ctx)->gd->settings.sfx_volume = *w->value;   /* no sounds yet */
}

/* The new menu song plays at once, from its start (5.2). */
static void menu_song_changed(void *ctx, widget_t *w)
{
    option_m_t *om = menu_of(ctx);
    const song_t *s = &om->gd->library.songs[*w->value];

    snprintf(om->gd->settings.menu_song, sizeof(om->gd->settings.menu_song),
        "%s", s->file);
    snprintf(om->gd->music.menu_file, sizeof(om->gd->music.menu_file), "%s",
        s->file);
    om->gd->music.menu_position = 0.0f;
    om->gd->music.menu_playing = false;      /* the old one is replaced */
    music_menu(om->gd);
}

static void audio_offset_changed(void *ctx, widget_t *w)
{
    option_m_t *om = menu_of(ctx);

    om->gd->settings.audio_offset_ms = *w->value;
    om->gd->music.audio_offset = *w->value / 1000.0;
}

static void toggle_changed(void *ctx, widget_t *w)
{
    option_m_t *om = menu_of(ctx);
    settings_t *s = &om->gd->settings;

    if (w->value == &om->show_percent)
        s->show_percent = *w->value;
    if (w->value == &om->show_progress_bar)
        s->show_progress_bar = *w->value;
    if (w->value == &om->show_attempts)
        s->show_attempts = *w->value;
}

/* A captured input (5.5): swaps, clears, or is refused, with a notice. */
static void slot_captured(void *ctx, widget_t *w, binding_t b)
{
    option_m_t *om = ctx;
    int slot = (int)(w - om->row_widgets);
    int other = -1;
    char a[32];
    char c[32];
    char name[BINDING_NAME_MAX];
    char text[160];
    rebind_result_t r = rebind(om->slots, slot, b, &other);

    if (r == REBIND_REFUSED)
        return options_notice(om, "At least one jump input has to stay");
    if (r == REBIND_SAME)
        return;
    menu_of(ctx);
    slots_to_settings(&om->gd->settings, om->slots);
    if (r != REBIND_SWAPPED)
        return;
    slot_name(slot, a, sizeof(a));
    slot_name(other, c, sizeof(c));
    binding_format(b, name, sizeof(name));
    snprintf(text, sizeof(text), "%s moved: %s <-> %s", name, c, a);
    options_notice(om, text);
}

static void display_changed(void *ctx, widget_t *w)
{
    option_m_t *om = menu_of(ctx);

    (void)w;
    om->pending_display = true;              /* a new window, after the events */
}

static void vsync_changed(void *ctx, widget_t *w)
{
    option_m_t *om = menu_of(ctx);

    om->gd->settings.vsync = *w->value;
    window_apply_sync(om->gd->w, &om->gd->settings);
    om->row_widgets[3].enabled = !*w->value;  /* the FPS row (5.2) */
}

static void fps_changed(void *ctx, widget_t *w)
{
    option_m_t *om = menu_of(ctx);

    om->gd->settings.fps_limit = FPS_VALUES[*w->value];
    window_apply_sync(om->gd->w, &om->gd->settings);
}

static void reset_pressed(void *ctx, widget_t *w)
{
    (void)w;
    ((option_m_t *)ctx)->pending_dialog = DIALOG_RESET;
}

static void folder_pressed(void *ctx, widget_t *w)
{
    (void)w;
    options_open_save_folder(ctx);
}

static widget_t row(int i, widget_kind_t kind, const char *label, int *value)
{
    return (widget_t){.kind = kind, .label = label, .enabled = true,
        .value = value, .bounds = {1400.0f, OPT_ROW_Y + OPT_ROW_STEP
        * (float)i, 380.0f, 70.0f}};
}

static widget_t slider(int i, const char *label, int *value, widget_fn fn)
{
    widget_t w = row(i, W_SLIDER, label, value);

    w.max = 100;
    w.step = 5;
    w.on_change = fn;
    return w;
}

static int audio_rows(option_m_t *om, widget_t *r)
{
    const library_t *lib = &om->gd->library;

    r[0] = slider(0, "Music volume", &om->music_volume, music_volume_changed);
    r[1] = slider(1, "Effects volume", &om->sfx_volume, sfx_volume_changed);
    r[2] = row(2, W_CYCLER, "Menu music", &om->menu_song);
    r[2].max = lib->count > 0 ? (int)lib->count - 1 : 0;
    r[2].choices = om->song_titles;
    r[2].enabled = lib->count > 0;
    r[2].on_change = menu_song_changed;
    r[3] = slider(3, "Audio offset (ms)", &om->audio_offset,
        audio_offset_changed);
    r[3].min = -300;
    r[3].max = 300;
    return 4;
}

static int gameplay_rows(option_m_t *om, widget_t *r)
{
    r[0] = row(0, W_TOGGLE, "Show percentage", &om->show_percent);
    r[1] = row(1, W_TOGGLE, "Show progress bar", &om->show_progress_bar);
    r[2] = row(2, W_TOGGLE, "Show attempts", &om->show_attempts);
    for (int i = 0; i < 3; i++)
        r[i].on_change = toggle_changed;
    return 3;
}

static int controls_rows(option_m_t *om, widget_t *r)
{
    static const char *const LABELS[NB_SLOTS] = {"Jump 1", "Jump 2",
        "Jump 3", "Jump 4", "Jump 5", "Jump 6", "Restart", "Place checkpoint",
        "Remove checkpoint"};

    slots_from_settings(&om->gd->settings, om->slots);
    for (int i = 0; i < NB_SLOTS; i++) {
        r[i] = row(i, W_KEYBIND, LABELS[i], NULL);
        r[i].binding = &om->slots[i];
        r[i].on_capture = slot_captured;
        r[i].bounds.y = OPT_ROW_Y - 40.0f + 90.0f * (float)i;   /* 9 rows fit */
    }
    return NB_SLOTS;
}

static int display_rows(option_m_t *om, widget_t *r)
{
    r[0] = row(0, W_TOGGLE, "Fullscreen", &om->fullscreen);
    r[0].on_change = display_changed;
    r[1] = row(1, W_CYCLER, "Window size", &om->window_size);
    r[1].max = om->sizes.count - 1;
    r[1].choices = om->sizes.labels;
    r[1].enabled = !om->fullscreen;
    r[1].on_change = display_changed;
    r[2] = row(2, W_TOGGLE, "VSync", &om->vsync);
    r[2].on_change = vsync_changed;
    r[3] = row(3, W_CYCLER, "FPS limit", &om->fps);
    r[3].max = 4;
    r[3].choices = FPS_LABELS;
    r[3].enabled = !om->vsync;
    r[3].on_change = fps_changed;
    return 4;
}

static int data_rows(option_m_t *om, widget_t *r)
{
    (void)om;
    r[0] = row(0, W_BUTTON, "Reset progress", NULL);
    r[0].on_activate = reset_pressed;
    r[1] = row(1, W_BUTTON, "Open save folder", NULL);
    r[1].on_activate = folder_pressed;
    return 2;
}

static void rows_back(void *ctx)
{
    ((option_m_t *)ctx)->pending_leave = true;
}

/* Tab and Shift+Tab: the next and the previous section (5.1). */
static void rows_key(void *ctx, binding_t key, bool shift)
{
    option_m_t *om = ctx;

    if (key.kind == BIND_KEY && key.code == KEY_Tab)
        om->pending_section = (om->section + (shift ? NB_SECTIONS - 1 : 1))
            % NB_SECTIONS;
}

/* The ints the widgets show, from the settings. */
static void read_values(option_m_t *om)
{
    const settings_t *s = &om->gd->settings;

    om->music_volume = s->music_volume;
    om->sfx_volume = s->sfx_volume;
    om->audio_offset = s->audio_offset_ms;
    om->show_percent = s->show_percent;
    om->show_progress_bar = s->show_progress_bar;
    om->show_attempts = s->show_attempts;
    om->fullscreen = s->fullscreen;
    om->vsync = s->vsync;
    om->fps = 0;
    for (int i = 0; i < 5; i++)
        if (FPS_VALUES[i] == s->fps_limit)
            om->fps = i;
    om->menu_song = 0;
    for (size_t i = 0; i < om->gd->library.count; i++)
        if (strcmp(om->gd->library.songs[i].file, om->gd->music.menu_file)
            == 0)
            om->menu_song = (int)i;
    window_sizes_list(&om->sizes, (int)sfVideoMode_getDesktopMode().width,
        (int)sfVideoMode_getDesktopMode().height, s->window_width,
        s->window_height);
    om->window_size = om->sizes.current;
}

void options_build_rows(option_m_t *om)
{
    static int (*const BUILD[NB_SECTIONS])(option_m_t *, widget_t *) = {
        audio_rows, gameplay_rows, controls_rows, display_rows, data_rows};
    int n = 0;

    read_values(om);
    n = BUILD[om->section](om, om->row_widgets);
    ui_init(&om->rows, om->row_widgets, n, om);
    om->rows.label_x = OPT_LABEL_X;
    om->rows.on_back = rows_back;
    om->rows.on_key = rows_key;
}
