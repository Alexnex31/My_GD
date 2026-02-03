/*
** ALEXNEX PROJECT, 2026
** hunter
** File description:
** my_hunter main file
*/

#include "mygd.h"

void free_textures(textures_t *res)
{
    sfTexture_destroy(res->edi_background);
    sfTexture_destroy(res->main_background);
    sfTexture_destroy(res->level_background);
    sfTexture_destroy(res->list_background);
    sfTexture_destroy(res->opt_background);
    sfTexture_destroy(res->play_button);
    sfTexture_destroy(res->opt_button);
    sfTexture_destroy(res->onli_button);
    free(res);
}

textures_t *load_textures(void)
{
    textures_t *res = malloc(sizeof(textures_t));

    res->main_background = sfTexture_createFromFile("res/main_background.png", NULL);
    res->edi_background = sfTexture_createFromFile("res/editor_background.png", NULL);
    res->list_background = sfTexture_createFromFile("res/level_list_background.png", NULL);
    res->opt_background = sfTexture_createFromFile("res/opt_background.png", NULL);
    res->level_background = sfTexture_createFromFile("res/level_background.png", NULL);
    res->play_button = sfTexture_createFromFile("res/play_button.png", NULL);
    res->opt_button = sfTexture_createFromFile("res/param_button.png", NULL);
    res->onli_button = sfTexture_createFromFile("res/online_button.png", NULL);
    return res;
}

void free_gd(gd_t *gd)
{
    free_textures(gd->res);
    sfFont_destroy(gd->main_font);
    free_cursor(gd->cursor);
    free(gd->event);
    destroy_all(gd->w);
    free(gd);
}

gd_t *create_gd(void)
{
    gd_t *gd = malloc(sizeof(gd_t));

    gd->res = load_textures();
    gd->main_font = sfFont_createFromFile("res/GDfont.ttf");
    gd->w = create_window(1920, 1080);
    gd->cursor = create_cursor();
    gd->event = malloc(sizeof(sfEvent));
    gd->menu = 'm';
    return gd;
}

static int my_strlen(char *str)
{
    int i = 0;

    while (str[i] != '\0') {
        i += 1;
    }
    return i;
}

void handle_playing(gd_t *gd, level_t **level)
{
    if (*level == NULL)
        *level = start_level(gd);
    print_level(gd, *level);
    keyboard_events_playing(level, gd);
}

void handle_level_list(gd_t *gd, level_list_t **level_list)
{
    if (*level_list == NULL)
        *level_list = create_level_list(gd);
    print_level_list(*level_list, gd->w);
    print_cursor(gd->cursor, gd->w);
    keyboard_events_level_list(level_list, gd);
}

void handle_editor_menu(gd_t *gd, editor_m_t **editor_menu)
{
    if (*editor_menu == NULL)
        *editor_menu = create_editor_menu(gd);
    print_editor_menu(*editor_menu, gd->w);
    print_cursor(gd->cursor, gd->w);
    keyboard_events_editor_menu(editor_menu, gd);
}

void handle_option_menu(gd_t *gd, option_m_t **option_menu)
{
    if (*option_menu == NULL)
        *option_menu = create_option_menu(gd);
    print_option_menu(*option_menu, gd->w);
    print_cursor(gd->cursor, gd->w);
    keyboard_events_option_menu(option_menu, gd);
}

void handle_main_menu(gd_t *gd, main_m_t **menu)
{
    if (*menu == NULL)
        *menu = create_main_menu(gd);
    print_main_menu(*menu, gd->w);
    print_cursor(gd->cursor, gd->w);
    keyboard_events_main_menu(menu, gd);
}

int main_loop(gd_t *gd)
{
    main_m_t *main_menu = NULL;
    option_m_t *option_menu = NULL;
    editor_m_t *editor_menu = NULL;
    level_list_t *level_list = NULL;
    level_t *level = NULL;

    while (sfRenderWindow_isOpen(gd->w)) {
        sfRenderWindow_clear(gd->w, sfTransparent);
        if (gd->menu == 'm')
            handle_main_menu(gd, &main_menu);
        if (gd->menu == 'o')
            handle_option_menu(gd, &option_menu);
        if (gd->menu == 'e')
            handle_editor_menu(gd, &editor_menu);
        if (gd->menu == 'l')
            handle_level_list(gd, &level_list);
        if (gd->menu == 'P')
            handle_playing(gd, &level);
        sfRenderWindow_display(gd->w);
    }
    if (main_menu != NULL)
        free_main_menu(main_menu);
    if (option_menu != NULL)
        free_option_menu(option_menu);
    if (editor_menu != NULL)
        free_editor_menu(editor_menu);
    if (level_list != NULL)
        free_level_list_menu(level_list);
    if (level != NULL)
        free_level(level);
    free_gd(gd);
    return 0;
}

int info(char *argv)
{
    if (argv[0] == '-' && argv[1] == 'h') {
        my_putstr("GD :)\n");
        my_putstr("🭀 🭁 🭂 🭃 🭄 🭅 🭆 🭇 🭈 🭉 🭊 🭋 🭌 🭍 🭎 🭏 🭐 🭑 🭒 🭓 🭔 🭕 🭖 🭗 🭘 🭙\n");
        return 0;
    }
    write(2, "wrong arguments, try : ""./my_hunter -h or ./my_hunter\n", 54);
    return 84;
}

int main(int argc, char **argv)
{
    gd_t *gd;

    if (argc == 1) {
        gd = create_gd();
        return main_loop(gd);
    }
    if (argc > 2) {
        write(2, "too many arguments, try : ", 27);
        write(2, "./my_hunter -h or ./my_gd\n", 31);
        return 84;
    }
    if (my_strlen(argv[1]) == 2)
        return info(argv[1]);
    write(2, "wrong arguments, try : ""./my_gd -h or ./my_gd\n", 54);
    return 84;
}