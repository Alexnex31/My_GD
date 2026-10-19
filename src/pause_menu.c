/*
** ALEXNEX PROJECT, 2026
** pause_menu
** File description:
** the pause menu: the level stops, its song waits, Resume, Restart, Practice, Quit (PLAN 10.4)
*/

#include "mygd.h"

enum {
    PAUSE_NONE,
    PAUSE_RESUME,
    PAUSE_RESTART,
    PAUSE_PRACTICE,
    PAUSE_QUIT
};

#define BUTTON_W 420.0f
#define BUTTON_H 90.0f
#define FIRST_Y 440.0f
#define GAP_Y 115.0f

static void resume_button(void *ctx, widget_t *w)
{
    (void)w;
    ((level_t *)ctx)->pause_action = PAUSE_RESUME;
}

static void restart_button(void *ctx, widget_t *w)
{
    (void)w;
    ((level_t *)ctx)->pause_action = PAUSE_RESTART;
}

static void practice_button(void *ctx, widget_t *w)
{
    (void)w;
    ((level_t *)ctx)->pause_action = PAUSE_PRACTICE;
}

static void quit_button(void *ctx, widget_t *w)
{
    (void)w;
    ((level_t *)ctx)->pause_action = PAUSE_QUIT;
}

static void pause_back(void *ctx)
{
    ((level_t *)ctx)->pause_action = PAUSE_RESUME;   /* Escape again */
}

static widget_t button(const char *label, int row, widget_fn fn)
{
    return (widget_t){.kind = W_BUTTON, .enabled = true, .label = label,
        .bounds = {(VIEW_W - BUTTON_W) / 2.0f, FIRST_Y + row * GAP_Y,
        BUTTON_W, BUTTON_H}, .on_activate = fn};
}

/*
** Ticks stop where they are and the song waits (FEATURES 4.8). Resume is
** focused, so Enter or Escape gives the level back at once.
*/
void level_pause(level_t *lv, gd_t *gd)
{
    if (lv->paused || lv->state == LEVEL_COMPLETE)
        return;
    lv->paused = true;
    lv->pause_action = PAUSE_NONE;
    lv->pause_widgets[0] = button("Resume", 0, resume_button);
    lv->pause_widgets[1] = button("Restart", 1, restart_button);
    lv->pause_widgets[2] = button(lv->practice ? "Practice: on"
        : "Practice: off", 2, practice_button);
    lv->pause_widgets[3] = button("Quit", 3, quit_button);
    ui_init(&lv->pause_ui, lv->pause_widgets, PAUSE_ITEMS, lv);
    lv->pause_ui.on_back = pause_back;
    lv->pause_ui.panel = (ui_rect_t){(VIEW_W - 760.0f) / 2.0f, 230.0f,
        760.0f, 690.0f};
    music_pause(&gd->music);
}

void pause_event(level_t *lv, gd_t *gd)
{
    ui_event_t ev;

    if (ui_from_sf(gd, gd->event, &ev))
        ui_event(&lv->pause_ui, &ev, ui_now_ms());
}

/*
** The level goes on from the very tick it stopped at: the clock restarts now,
** so the pause is no burst of ticks (3.6). A dying player's song is already
** stopped; its respawn starts it again.
*/
static void resume(level_t *lv, gd_t *gd, bool song)
{
    lv->paused = false;
    lv->last_frame_us = input_now_us(gd);
    input_resume(gd);
    if (song && lv->state == LEVEL_PLAYING && lv->song.song != NULL)
        music_play_at_tick(&gd->music, lv->sim.st.tick, 0.0);
}

/*
** After the events (PLAN 10.1): Quit frees the level, so nothing of it may be
** used once it has run. Quit is neither a death nor a completion: no best,
** and leaving writes the session's attempts (6.2).
*/
void pause_act(level_t **level, gd_t *gd)
{
    level_t *lv = *level;
    int action = lv->pause_action;

    lv->pause_action = PAUSE_NONE;
    if (action == PAUSE_RESUME)
        resume(lv, gd, true);
    if (action == PAUSE_RESTART) {
        resume(lv, gd, false);               /* the respawn starts the song */
        level_restart(lv, gd);               /* 6.5: the attempt ends there */
    }
    if (action == PAUSE_PRACTICE && !lv->practice) {
        practice_set(lv, gd, true);          /* the run goes on as it is */
        lv->pause_widgets[2].label = "Practice: on";
    } else if (action == PAUSE_PRACTICE) {
        resume(lv, gd, false);               /* from the start, its song too */
        practice_set(lv, gd, false);
    }
    if (action == PAUSE_QUIT)
        go_back_playing_level_list(gd, level);
}

/* Over the frozen level: dimmed, the level's name, how it's going. */
void pause_draw(gd_t *gd, level_t *lv)
{
    char line[200];

    ui_dim(gd);
    ui_update(&lv->pause_ui, ui_now_ms());
    ui_draw(gd, &lv->pause_ui);
    ui_caption(gd, lv->sim.lvl.hdr.name, VIEW_W / 2.0f, 295.0f);
    snprintf(line, sizeof(line), "Attempt %d  -  %.0f%%%s", lv->stats.attempts,
        sim_percent(&lv->sim), lv->practice ? "  -  practice" : "");
    ui_caption(gd, line, VIEW_W / 2.0f, 365.0f);
    print_cursor(gd->cursor, gd->w);
}
