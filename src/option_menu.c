/*
** ALEXNEX PROJECT, 2026
** option_menu
** File description:
** the options screen: sections, rows, saving when it's left (FEATURES 5)
*/

#include "mygd.h"

static const char *const SECTIONS[NB_SECTIONS] = {"Audio", "Gameplay",
    "Controls", "Display", "Data"};

static void tab_pressed(void *ctx, widget_t *w)
{
    option_m_t *om = ctx;

    om->pending_section = (int)(w - om->tab_widgets);
}

static void back_pressed(void *ctx, widget_t *w)
{
    (void)w;
    ((option_m_t *)ctx)->pending_leave = true;
}

/* The current section's tab reads "> Audio". */
static void label_tabs(option_m_t *om)
{
    for (int i = 0; i < NB_SECTIONS; i++) {
        snprintf(om->tab_labels[i], sizeof(om->tab_labels[i]), "%s%s",
            i == (int)om->section ? "> " : "", SECTIONS[i]);
        om->tab_widgets[i].label = om->tab_labels[i];
    }
}

/* Left column: the sections, one per 110 px; Back bottom-left (5.1). */
static void build_tabs(option_m_t *om)
{
    for (int i = 0; i < NB_SECTIONS; i++)
        om->tab_widgets[i] = (widget_t){.kind = W_BUTTON, .enabled = true,
            .bounds = {120.0f, 200.0f + 110.0f * (float)i, 340.0f, 80.0f},
            .on_activate = tab_pressed};
    om->tab_widgets[NB_SECTIONS] = (widget_t){.kind = W_BUTTON,
        .label = "Back", .enabled = true,
        .bounds = {120.0f, 940.0f, 340.0f, 80.0f}, .on_activate = back_pressed};
    ui_init(&om->tabs, om->tab_widgets, NB_SECTIONS + 1, om);
    om->tabs.focused = -1;                   /* the keyboard is the rows' */
    label_tabs(om);
}

option_m_t *create_option_menu(gd_t *gd)
{
    option_m_t *om = sim_xcalloc(1, sizeof(option_m_t));

    music_menu(gd);                          /* it plays on (FEATURES 4.10) */
    om->gd = gd;
    om->pending_section = -1;
    om->background = sfSprite_create();
    sfSprite_setTexture(om->background, gd->res->opt_background, sfTrue);
    om->text = sfText_create();
    sfText_setFont(om->text, gd->main_font);
    sfText_setCharacterSize(om->text, 32);
    sfText_setOutlineThickness(om->text, 3);
    om->song_titles = sim_xcalloc(gd->library.count + 1, sizeof(char *));
    for (size_t i = 0; i < gd->library.count; i++)
        om->song_titles[i] = gd->library.songs[i].title;
    if (gd->library.count == 0)
        om->song_titles[0] = "none";
    build_tabs(om);
    options_build_rows(om);
    gd->menu = 'o';
    return om;
}

void free_option_menu(option_m_t *om)
{
    sfSprite_destroy(om->background);
    sfText_destroy(om->text);
    free(om->song_titles);
    free(om);
}

static void draw_line(option_m_t *om, const char *s, float x, float y)
{
    sfText_setUnicodeString(om->text, utf8_to_utf32(s));
    sfText_setPosition(om->text, (sfVector2f){x, y});
    draw_text(om->gd, om->text);
}

void print_option_menu(option_m_t *om, gd_t *gd)
{
    sfRenderWindow_drawSprite(gd->w, om->background, NULL);
    ui_draw(gd, &om->tabs);
    if (om->section == SEC_DISPLAY) {        /* 5.3: worth saying */
        draw_line(om, "VSync and the frame limit change smoothness and power"
            " use,", OPT_LABEL_X, 640.0f);
        draw_line(om, "never the game's speed.", OPT_LABEL_X, 690.0f);
    }
    ui_draw(gd, &om->rows);
    options_draw_dialog(om);
    if (ui_now_ms() < om->notice_until_ms)
        draw_line(om, om->notice, OPT_LABEL_X, 1010.0f);
}

/* Leaving saves what changed (5.7); the main menu takes over. */
static void leave(option_m_t **om, gd_t *gd)
{
    if (gd->settings.dirty && settings_save(&gd->settings) != 0)
        dprintf(2, "my_gd: cannot save %s\n", gd->settings.path);
    free_option_menu(*om);
    *om = NULL;
    gd->menu = 'm';
}

/* The mouse goes to the tabs too, the keyboard only to the rows (5.1). */
static void route(option_m_t *om, gd_t *gd)
{
    ui_event_t ev;
    int64_t now = ui_now_ms();

    if (!ui_from_sf(gd, gd->event, &ev))
        return;
    if (om->dialog == DIALOG_NONE && om->rows.capturing < 0
        && ev.input.kind != BIND_KEY && ev.input.kind != BIND_JOY) {
        ui_event(&om->tabs, &ev, now);
        if (om->tabs.hovered < 0)
            om->tabs.focused = -1;
    }
    ui_event(&om->rows, &ev, now);
}

/* What the callbacks asked for, once the events are done (3.5). */
static void act(option_m_t **om, gd_t *gd)
{
    option_m_t *o = *om;

    if (o->pending_answer != 0)
        options_answer(o, o->pending_answer);
    if (o->pending_dialog != DIALOG_NONE)
        options_open_dialog(o, o->pending_dialog);
    if (o->pending_display)
        options_apply_display(o);
    if (o->pending_section >= 0 && o->dialog == DIALOG_NONE) {
        o->section = (options_section_t)o->pending_section;
        label_tabs(o);
        options_build_rows(o);
    }
    o->pending_answer = 0;
    o->pending_dialog = DIALOG_NONE;
    o->pending_display = false;
    o->pending_section = -1;
    if (o->pending_leave && o->dialog == DIALOG_NONE)
        leave(om, gd);
    else
        o->pending_leave = false;
}

void keyboard_events_option_menu(option_m_t **om, gd_t *gd)
{
    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed) {
            close_window(gd->w);
            return;
        }
        route(*om, gd);
    }
    act(om, gd);
    if (*om == NULL)
        return;
    ui_update(&(*om)->rows, ui_now_ms());
    options_dialogs_update(*om);
}
