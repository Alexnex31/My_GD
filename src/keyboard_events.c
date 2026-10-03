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
static bool poll_event(gd_t *gd)
{
    if (!sfRenderWindow_pollEvent(gd->w, gd->event))
        return false;
    if (gd->event->type == sfEvtResized)
        apply_letterbox(gd, gd->event->size.width, gd->event->size.height);
    return true;
}

/* A click in window pixels, in the menus' 1920x1080 space (9.1, 9.7). */
static sfVector2i click_pos(gd_t *gd)
{
    sfVector2i pixel = {gd->event->mouseButton.x, gd->event->mouseButton.y};
    sfVector2f pos = sfRenderWindow_mapPixelToCoords(gd->w, pixel,
        gd->ui_view);

    return (sfVector2i){(int)pos.x, (int)pos.y};
}

void play_button(main_m_t **menu, gd_t *gd)
{
    free_main_menu(*menu);
    *menu = NULL;
    gd->menu = 'l';
}

void param_button(main_m_t **menu, gd_t *gd)
{
    free_main_menu(*menu);
    *menu = NULL;
    gd->menu = 'o';
}

void online_button(main_m_t **menu, gd_t *gd)
{
    free_main_menu(*menu);
    *menu = NULL;
    gd->menu = 'e';
}

void buttons_menu(main_m_t **menu, gd_t *gd, int mx, int my)
{
    if (mx >= (*menu)->play->pos.x && mx <= (*menu)->play->pos.x + (*menu)->play->size) {
        if (my >= (*menu)->play->pos.y && my <= (*menu)->play->pos.y + (*menu)->play->size) {
            play_button(menu, gd);
            return;
        }
    }
    if (mx >= (*menu)->param->pos.x && mx <= (*menu)->param->pos.x + (*menu)->param->size) {
        if (my >= (*menu)->param->pos.y && my <= (*menu)->param->pos.y + (*menu)->param->size) {
            param_button(menu, gd);
            return;
        }
    }
    if (mx >= (*menu)->online->pos.x && mx <= (*menu)->online->pos.x + (*menu)->online->size) {
        if (my >= (*menu)->online->pos.y && my <= (*menu)->online->pos.y + (*menu)->online->size) {
            online_button(menu, gd);
            return;
        }
    }
}

void keyboard_events_main_menu(main_m_t **menu, gd_t *gd)
{
    sfVector2i pos;

    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed) {
            close_window(gd->w);
            return;
        }
        if (gd->event->type == sfEvtKeyPressed && gd->event->key.code == sfKeyEscape) {
            close_window(gd->w);
            return;
        }
        if (gd->event->type == sfEvtKeyPressed && gd->event->key.code == sfKeySpace) {
            play_button(menu, gd);
            return;
        }
        if (gd->event->type == sfEvtMouseButtonPressed && gd->event->mouseButton.button == sfMouseLeft) {
            pos = click_pos(gd);
            buttons_menu(menu, gd, pos.x, pos.y);
            return;
        }
    }
}

void go_back_option_main(option_m_t **om, gd_t *gd)
{
    free_option_menu(*om);
    *om = NULL;
    gd->menu = 'm';
}

void keyboard_events_option_menu(option_m_t **om, gd_t *gd)
{
    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed) {
            close_window(gd->w);
            return;
        }
        if (gd->event->type == sfEvtKeyPressed && gd->event->key.code == sfKeyEscape) {
            go_back_option_main(om, gd);
            return;
        }
    }
}

void go_back_editorm_main(editor_m_t **editor_m, gd_t *gd)
{
    free_editor_menu(*editor_m);
    *editor_m = NULL;
    gd->menu = 'm';
}

void keyboard_events_editor_menu(editor_m_t **editor_m, gd_t *gd)
{
    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed) {
            close_window(gd->w);
            return;
        }
        if (gd->event->type == sfEvtKeyPressed && gd->event->key.code == sfKeyEscape) {
            go_back_editorm_main(editor_m, gd);
            return;
        }
    }
}

void go_back_list_main(level_list_t **lvl_list, gd_t *gd)
{
    free_level_list_menu(*lvl_list);
    *lvl_list = NULL;
    gd->menu = 'm';
}

int check_level_button_click(level_button_t *lb, int mx, int my)
{
    button_t *btn = lb->play_button;

    if (mx >= btn->pos.x && mx <= btn->pos.x + btn->size) {
        if (my >= btn->pos.y && my <= btn->pos.y + btn->size) {
            return 1;
        }
    }
    return 0;
}

void handle_level_buttons_click(level_list_t **lvl_list, gd_t *gd, int mx, int my)
{
    int i = 0;

    while (i < (*lvl_list)->nb_levels) {
        if (check_level_button_click((*lvl_list)->level_buttons[i], mx, my)) {
            snprintf(gd->selected_id, sizeof(gd->selected_id), "%s",
                (*lvl_list)->level_buttons[i]->id);
            free_level_list_menu(*lvl_list);
            *lvl_list = NULL;
            gd->menu = 'P';
            return;
        }
        i += 1;
    }
}

void keyboard_events_level_list(level_list_t **lvl_list, gd_t *gd)
{
    sfVector2i pos;

    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed) {
            close_window(gd->w);
            return;
        }
        if (gd->event->type == sfEvtKeyPressed && gd->event->key.code == sfKeyEscape) {
            go_back_list_main(lvl_list, gd);
            return;
        }
        if (gd->event->type == sfEvtMouseButtonPressed && gd->event->mouseButton.button == sfMouseLeft) {
            pos = click_pos(gd);
            handle_level_buttons_click(lvl_list, gd, pos.x, pos.y);
            return;
        }
    }
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

int check_end_screen_buttons(end_level_screen_t *end_screen, int mx, int my)
{
    button_t *retry = end_screen->retry_button;
    button_t *quit = end_screen->quit_button;

    if (mx >= retry->pos.x && mx <= retry->pos.x + retry->size) {
        if (my >= retry->pos.y && my <= retry->pos.y + retry->size) {
            return 1;
        }
    }
    if (mx >= quit->pos.x && mx <= quit->pos.x + quit->size) {
        if (my >= quit->pos.y && my <= quit->pos.y + quit->size) {
            return 2;
        }
    }
    return 0;
}

void handle_end_screen_click(level_t **level, gd_t *gd, int mx, int my)
{
    int result;

    if ((*level)->end_screen == NULL)
        return;
    result = check_end_screen_buttons((*level)->end_screen, mx, my);
    if (result == 1)
        retry_level(gd, level);
    else if (result == 2)
        go_back_playing_level_list(gd, level);
}

/* The level scene's events. The jump itself is polled per tick (3.6). */
static void playing_key(level_t **level, gd_t *gd, sfKeyCode key)
{
    if (key == sfKeyEscape)
        return go_back_playing_level_list(gd, level);
    if (key == sfKeyF3)
        gd->debug_overlay = !gd->debug_overlay;
    if (key == sfKeyR && (*level)->state != LEVEL_COMPLETE)
        level_restart(*level, gd);
}

void keyboard_events_playing(level_t **level, gd_t *gd)
{
    sfVector2i pos;

    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed) {
            level_free(*level, gd);
            *level = NULL;
            close_window(gd->w);
            return;
        }
        if (gd->event->type == sfEvtGainedFocus)
            sfClock_restart((*level)->clock);   /* no burst after a pause */
        if (gd->event->type == sfEvtKeyPressed) {
            playing_key(level, gd, gd->event->key.code);
            if (*level == NULL)
                return;
        }
        if (gd->event->type == sfEvtMouseButtonPressed
            && (*level)->state == LEVEL_COMPLETE
            && (*level)->end_time >= level_end_flight(*level) + END_FLASH) {
            pos = click_pos(gd);
            handle_end_screen_click(level, gd, pos.x, pos.y);
            return;
        }
    }
}
