/*
** ALEXNEX PROJECT, 2026
** main_menu
** File description:
** manage main_menu
*/

#include "mygd.h"

void free_main_menu(main_m_t *m)
{
    if (m == NULL)
        return;
    if (m->background != NULL)
        sfSprite_destroy(m->background);
    if (m->play != NULL)
        free_button(m->play);
    if (m->param != NULL)
        free_button(m->param);
    if (m->online != NULL)
        free_button(m->online);
    if (m->title != NULL)
        sfText_destroy(m->title);
    if (m->title_string != NULL)
        free(m->title_string);
    free(m);
}

void print_main_menu(main_m_t *m, sfRenderWindow *w)
{
    sfRenderWindow_drawSprite(w, m->background, NULL);
    print_button(m->play, w);
    print_button(m->param, w);
    print_button(m->online, w);
    sfRenderWindow_drawText(w, m->title, NULL);
}

main_m_t *create_main_menu(gd_t *gd)
{
    main_m_t *menu = malloc(sizeof(main_m_t));
    sfVector2f title_pos = {720, 220};

    if (menu == NULL)
        return NULL;
    sfMusic_setLoop(gd->musics->main, sfTrue);
    sfMusic_play(gd->musics->main);
    menu->background = sfSprite_create();
    menu->title = sfText_create();
    menu->title_string = strdup("My_GD");
    sfSprite_setTexture(menu->background, gd->res->main_background, sfTrue);
    sfText_setString(menu->title, menu->title_string);
    sfText_setCharacterSize(menu->title, 140);
    sfText_setPosition(menu->title, title_pos);
    sfText_setFont(menu->title, gd->main_font);
    sfText_setOutlineThickness(menu->title, 5);
    menu->play = create_button(800, 500, 300, gd->res->play_button);
    menu->param = create_button(500, 500, 200, gd->res->opt_button);
    menu->online = create_button(1200, 500, 200, gd->res->onli_button);
    gd->menu = 'm';
    return menu;
}
