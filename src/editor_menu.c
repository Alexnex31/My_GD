/*
** ALEXNEX PROJECT, 2026
** editor_menu
** File description:
** manage editor_menu
*/

#include "mygd.h"

void free_editor_menu(editor_m_t *om)
{
    sfSprite_destroy(om->background);
    free(om);
}

void print_editor_menu(editor_m_t *om, sfRenderWindow *w)
{
    sfRenderWindow_drawSprite(w, om->background, NULL);
}

editor_m_t *create_editor_menu(gd_t *gd)
{
    editor_m_t *menu = xcalloc(1, sizeof(editor_m_t));

    sfMusic_stop(gd->musics->main);
    menu->background = sfSprite_create();
    sfSprite_setTexture(menu->background, gd->res->edi_background, sfTrue);
    gd->menu = 'e';
    return menu;
}