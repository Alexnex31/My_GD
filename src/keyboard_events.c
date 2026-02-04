/*
** ALEXNEX PROJECT, 2026
** events
** File description:
** functions for events like key press
*/

#include "mygd.h"

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
    while (sfRenderWindow_pollEvent(gd->w, gd->event)) {
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
            buttons_menu(menu, gd, gd->event->mouseButton.x, gd->event->mouseButton.y);
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
    while (sfRenderWindow_pollEvent(gd->w, gd->event)) {
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
    while (sfRenderWindow_pollEvent(gd->w, gd->event)) {
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
            gd->selected_level = (*lvl_list)->level_buttons[i]->level_num;
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
    while (sfRenderWindow_pollEvent(gd->w, gd->event)) {
        if (gd->event->type == sfEvtClosed) {
            close_window(gd->w);
            return;
        }
        if (gd->event->type == sfEvtKeyPressed && gd->event->key.code == sfKeyEscape) {
            go_back_list_main(lvl_list, gd);
            return;
        }
        if (gd->event->type == sfEvtMouseButtonPressed && gd->event->mouseButton.button == sfMouseLeft) {
            handle_level_buttons_click(lvl_list, gd, gd->event->mouseButton.x, gd->event->mouseButton.y);
            return;
        }
    }
}

void go_back_playing_level_list(gd_t *gd, level_t **level)
{
    free_level(*level);
    *level = NULL;
    gd->menu = 'l';
}

void jump(level_t *level, gd_t *gd)
{
    write(1, "jump\n", 5);
    if (level->player->allow_jump == 'y')
        level->player->vy += 20;
}

void keyboard_events_playing(level_t **level, gd_t *gd)
{
    while (sfRenderWindow_pollEvent(gd->w, gd->event)) {
        if (gd->event->type == sfEvtClosed) {
            close_window(gd->w);
            return;
        }
        if (gd->event->type == sfEvtKeyPressed && gd->event->key.code == sfKeyEscape) {
            go_back_playing_level_list(gd, level);
            return;
        }
        if (gd->event->type == sfEvtKeyPressed && gd->event->key.code == sfKeySpace) {
            jump(*level, gd);
        }
        if (gd->event->type == sfEvtMouseButtonPressed)
            jump(*level, gd);
    }
}
