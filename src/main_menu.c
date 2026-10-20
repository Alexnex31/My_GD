/*
** ALEXNEX PROJECT, 2026
** main_menu
** File description:
** the main menu: Options, Play, Editor, Credits and Icons, on the toolkit (FEATURES 3)
*/

#include "mygd.h"

    #define MAIN_OPTIONS 0
    #define MAIN_PLAY 1
    #define MAIN_EDITOR 2
    #define MAIN_CREDITS 3
    #define MAIN_ICONS 4
    #define MAIN_IMAGES 4             /* the image buttons' slots in buttons[] */

void free_main_menu(main_m_t *m)
{
    if (m == NULL)
        return;
    sfSprite_destroy(m->background);
    for (int i = 0; i < MAIN_IMAGES; i++)
        free_button(m->buttons[i]);
    sfText_destroy(m->title);
    free(m);
}

void print_main_menu(main_m_t *m, gd_t *gd)
{
    sfRenderWindow_drawSprite(gd->w, m->background, NULL);
    ui_update(&m->ui, ui_now_ms());
    for (int i = 0; i < 3; i++)
        ui_draw_sprite(gd, &m->ui, i, m->buttons[i]->sprite);
    ui_draw_sprite(gd, &m->ui, MAIN_ICONS, m->buttons[3]->sprite);
    ui_draw_widget(gd, &m->ui, MAIN_CREDITS);
    sfRenderWindow_drawText(gd->w, m->title, NULL);
}

/* The callbacks only say where to go: the scene changes after the events. */
static void go(void *ctx, widget_t *w)
{
    static const char SCENES[5] = {'o', 'l', 'e', 'c', 'i'};
    main_m_t *m = ctx;

    m->next = SCENES[w - m->widgets];
}

static void quit(void *ctx)
{
    ((main_m_t *)ctx)->quit = true;
}

/* Play has the focus: Enter or Space play, as GD's big button does. */
main_m_t *create_main_menu(gd_t *gd)
{
    main_m_t *menu = sim_xcalloc(1, sizeof(main_m_t));
    static const char *const LABELS[3] = {"Options", "Play", "Editor"};

    music_menu(gd);                          /* FEATURES 4.10 */
    menu->background = sfSprite_create();
    menu->title = sfText_create();
    sfSprite_setTexture(menu->background, gd->res->main_background, sfTrue);
    sfText_setString(menu->title, "My_GD");
    sfText_setCharacterSize(menu->title, 140);
    sfText_setPosition(menu->title, (sfVector2f){720, 220});
    sfText_setFont(menu->title, gd->main_font);
    sfText_setOutlineThickness(menu->title, 5);
    menu->buttons[MAIN_OPTIONS] = create_button(500, 500, 200,
        gd->res->opt_button);
    menu->buttons[MAIN_PLAY] = create_button(800, 500, 300,
        gd->res->play_button);
    menu->buttons[MAIN_EDITOR] = create_button(1200, 500, 200,
        gd->res->onli_button);
    for (int i = 0; i < 3; i++)
        menu->widgets[i] = button_widget(menu->buttons[i], LABELS[i], go);
    menu->widgets[MAIN_CREDITS] = (widget_t){.kind = W_BUTTON,
        .label = "Credits", .enabled = true,
        .bounds = {1600.0f, 975.0f, 280.0f, 75.0f}, .on_activate = go};
    menu->buttons[3] = create_button(40.0f, 900.0f, 150,   /* the cube worn */
        icon_texture(gd, MODE_CUBE));
    menu->widgets[MAIN_ICONS] = button_widget(menu->buttons[3], "Icons", go);
    ui_init(&menu->ui, menu->widgets, 5, menu);
    menu->ui.focused = MAIN_PLAY;
    menu->ui.on_back = quit;
    gd->menu = 'm';
    return menu;
}
