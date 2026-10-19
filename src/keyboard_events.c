/*
** ALEXNEX PROJECT, 2026
** events
** File description:
** functions for events like key press
*/

#include "mygd.h"

/*
** Every scene polls through here, so a resize letterboxes the views (9.7)
** whatever scene is showing.
*/
bool poll_event(gd_t *gd)
{
    if (!sfRenderWindow_pollEvent(gd->w, gd->event))
        return false;
    if (gd->event->type == sfEvtResized)
        apply_letterbox(gd, gd->event->size.width, gd->event->size.height);
    return true;
}

/* After the events (3.5): the scene a button chose, or the window closed. */
void keyboard_events_main_menu(main_m_t **menu, gd_t *gd)
{
    ui_event_t ev;
    char next = 0;

    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed)
            return close_window(gd->w);
        if (ui_from_sf(gd, gd->event, &ev))
            ui_event(&(*menu)->ui, &ev, ui_now_ms());
    }
    if ((*menu)->quit)
        return close_window(gd->w);
    next = (*menu)->next;
    if (next == 0)
        return;
    free_main_menu(*menu);
    *menu = NULL;
    gd->menu = next;
}

/*
** The grid's widgets are in the scrolled space: a mouse position is moved
** by the scroll. The wheel scrolls; a key that moves the focus scrolls to it.
*/
static void level_list_event(level_list_t *list, gd_t *gd)
{
    ui_event_t ev;

    if (!ui_from_sf(gd, gd->event, &ev))
        return;
    if (ev.kind == UI_WHEEL)
        level_list_scroll(list, -(float)ev.wheel * LIST_ROW_H / 2.0f);
    if (ev.kind == UI_MOVE || ev.kind == UI_WHEEL
        || ev.input.kind == BIND_MOUSE)
        ev.y += list->scroll;
    if (ev.kind == UI_PRESS && ev.input.kind == BIND_KEY)
        list->follow = true;
    ui_event(&list->ui, &ev, ui_now_ms());
}

/*
** A level chosen starts once the list is gone; whatever chose it, a click
** or a key that is also a jump, is no jump until released (input_start).
*/
static void level_list_act(level_list_t **lvl_list, gd_t *gd)
{
    level_list_t *list = *lvl_list;

    if (list->pending_song >= 0)
        song_picker_open(list, gd, list->pending_song);
    list->pending_song = -1;
    if (list->pending_play < 0 && !list->pending_back)
        return;
    if (list->pending_play >= 0)
        snprintf(gd->selected_id, sizeof(gd->selected_id), "%s",
            list->level_buttons[list->pending_play]->id);
    gd->menu = list->pending_play >= 0 ? 'P' : 'm';
    free_level_list_menu(list);
    *lvl_list = NULL;
}

void keyboard_events_level_list(level_list_t **lvl_list, gd_t *gd)
{
    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed)
            return close_window(gd->w);
        if ((*lvl_list)->picker_for >= 0)
            song_picker_event(*lvl_list, gd);    /* it has every event */
        else
            level_list_event(*lvl_list, gd);
    }
    song_picker_finish(*lvl_list, gd);       /* what a callback closed */
    song_picker_update(*lvl_list, gd);
    ui_update(&(*lvl_list)->ui, ui_now_ms());
    level_list_follow(*lvl_list);
    level_list_act(lvl_list, gd);
}

void go_back_playing_level_list(gd_t *gd, level_t **level)
{
    level_free(*level, gd);                  /* flushes the session's numbers */
    *level = NULL;
    gd->menu = 'l';
}

void retry_level(gd_t *gd, level_t **level)
{
    char id[24];

    snprintf(id, sizeof(id), "%s", (*level)->id);
    level_free(*level, gd);
    *level = level_start(gd, id);
}

/* The end screen is up: the end sequence is over (9.8). */
static bool end_screen_shown(const level_t *lv)
{
    return lv->state == LEVEL_COMPLETE && lv->end_screen != NULL
        && lv->end_time >= level_end_flight((level_t *)lv) + END_FLASH;
}

/* Retry or Quit, once the events are read: both free the level. */
static void end_screen_act(level_t **level, gd_t *gd)
{
    int action = (*level)->end_screen->action;

    if (action == 1)
        retry_level(gd, level);
    else if (action == 2)
        go_back_playing_level_list(gd, level);
}

/*
** The level scene's keys. The jump has its own thread (FEATURES 1). Escape
** pauses (PLAN 10.4); once the level is complete it skips the end sequence
** to the end screen (9.8), and there it leaves, as Quit does.
*/
static void playing_key(level_t **level, gd_t *gd, sfKeyCode key)
{
    float shown = level_end_flight(*level) + END_FLASH;

    if (key == sfKeyEscape && (*level)->state == LEVEL_COMPLETE
        && (*level)->end_time < shown) {
        (*level)->end_time = shown;
        return;
    }
    if (key == sfKeyEscape && (*level)->state == LEVEL_COMPLETE)
        return go_back_playing_level_list(gd, level);
    if (key == sfKeyEscape)
        return level_pause(*level, gd);
    if (key == sfKeyF3)
        gd->debug_overlay = !gd->debug_overlay;
    if (key == sfKeyA || key == sfKeyE)      /* the start positions (11.8) */
        level_change_start(*level, gd, key == sfKeyE ? 1 : -1);
}

/* While the pause menu is open, every event is its own. */
static bool paused_event(level_t *lv, gd_t *gd)
{
    if (!lv->paused)
        return false;
    pause_event(lv, gd);
    return true;
}

void keyboard_events_playing(level_t **level, gd_t *gd)
{
    ui_event_t ev;

    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed) {
            level_free(*level, gd);
            *level = NULL;
            close_window(gd->w);
            return;
        }
        if (gd->event->type == sfEvtLostFocus)
            level_pause(*level, gd);         /* the keys are someone else's */
        if (gd->event->type == sfEvtGainedFocus)
            (*level)->last_frame_us = input_now_us(gd);  /* no burst after a pause */
        if (paused_event(*level, gd))
            continue;
        if (end_screen_shown(*level)) {      /* it has every event */
            if (ui_from_sf(gd, gd->event, &ev))
                ui_event(&(*level)->end_screen->ui, &ev, ui_now_ms());
            continue;
        }
        if (gd->event->type == sfEvtKeyPressed) {
            playing_key(level, gd, gd->event->key.code);
            if (*level == NULL)
                return;
        }
        if (input_event_is(gd->event, gd->settings.restart_key)
            && (*level)->state != LEVEL_COMPLETE)
            level_restart(*level, gd);       /* 6.5, any key or button */
        if (input_event_is(gd->event, gd->settings.checkpoint_key))
            practice_place(*level);          /* practice only (13.1) */
        if (input_event_is(gd->event, gd->settings.remove_checkpoint_key))
            practice_remove(*level);
    }
    if (*level != NULL && (*level)->paused)
        pause_act(level, gd);
    else if (*level != NULL && end_screen_shown(*level))
        end_screen_act(level, gd);
}
