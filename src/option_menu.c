/*
** ALEXNEX PROJECT, 2026
** option_menu
** File description:
** manage option_menu
*/

#include "mygd.h"

void free_option_menu(option_m_t *om)
{
    sfSprite_destroy(om->background);
    free(om);
}

void print_option_menu(option_m_t *om, sfRenderWindow *w)
{
    sfRenderWindow_drawSprite(w, om->background, NULL);
}

option_m_t *create_option_menu(gd_t *gd)
{
    option_m_t *menu = malloc(sizeof(option_m_t));

    sfMusic_stop(gd->musics->main);
    menu->background = sfSprite_create();
    sfSprite_setTexture(menu->background, gd->res->opt_background, sfTrue);
    gd->menu = 'o';
    return menu;
}