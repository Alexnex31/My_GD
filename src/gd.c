/*
** ALEXNEX PROJECT, 2026
** hunter
** File description:
** my_hunter main file
*/

#include "mygd.h"

void free_textures(textures_t *res)
{
    if (res->edi_background != NULL)
        sfTexture_destroy(res->edi_background);
    if (res->main_background != NULL)
        sfTexture_destroy(res->main_background);
    if (res->level_background != NULL)
        sfTexture_destroy(res->level_background);
    if (res->list_background != NULL)
        sfTexture_destroy(res->list_background);
    if (res->opt_background != NULL)
        sfTexture_destroy(res->opt_background);
    if (res->play_button != NULL)
        sfTexture_destroy(res->play_button);
    if (res->opt_button != NULL)
        sfTexture_destroy(res->opt_button);
    if (res->onli_button != NULL)
        sfTexture_destroy(res->onli_button);
    if (res->ground != NULL)
        sfTexture_destroy(res->ground);
    if (res->spike != NULL)
        sfTexture_destroy(res->spike);
    if (res->block != NULL)
        sfTexture_destroy(res->block);
    if (res->player_icon != NULL)
        sfTexture_destroy(res->player_icon);
    if (res->ship_icon != NULL)
        sfTexture_destroy(res->ship_icon);
    if (res->end_level_background != NULL)
        sfTexture_destroy(res->end_level_background);
    if (res->retry_button != NULL)
        sfTexture_destroy(res->retry_button);
    if (res->quit_button != NULL)
        sfTexture_destroy(res->quit_button);
    free(res);
}

textures_t *load_textures(void)
{
    textures_t *res = malloc(sizeof(textures_t));

    res->main_background = sfTexture_createFromFile("res/main_background.png", NULL);
    if (res->main_background == NULL)
        printf("Warning: Could not load res/main_background.png\n");
    res->edi_background = sfTexture_createFromFile("res/cecilya.png", NULL);
    if (res->edi_background == NULL)
        printf("Warning: Could not load res/cecilya.png\n");
    res->list_background = sfTexture_createFromFile("res/level_list_background.png", NULL);
    if (res->list_background == NULL)
        printf("Warning: Could not load res/level_list_background.png\n");
    res->opt_background = sfTexture_createFromFile("res/noe_background.jpeg", NULL);
    if (res->opt_background == NULL)
        printf("Warning: Could not load res/noe_background.jpeg\n");
    res->level_background = sfTexture_createFromFile("res/level_background.png", NULL);
    if (res->level_background == NULL)
        printf("Warning: Could not load res/level_background.png\n");
    res->play_button = sfTexture_createFromFile("res/play_button.png", NULL);
    if (res->play_button == NULL)
        printf("Warning: Could not load res/play_button.png\n");
    res->opt_button = sfTexture_createFromFile("res/param_button.png", NULL);
    if (res->opt_button == NULL)
        printf("Warning: Could not load res/param_button.png\n");
    res->onli_button = sfTexture_createFromFile("res/online_button.png", NULL);
    if (res->onli_button == NULL)
        printf("Warning: Could not load res/online_button.png\n");
    res->ground = sfTexture_createFromFile("res/ground.png", NULL);
    if (res->ground == NULL)
        printf("Warning: Could not load res/ground.png\n");
    res->spike = sfTexture_createFromFile("res/spike.png", NULL);
    if (res->spike == NULL)
        printf("Warning: Could not load res/spike.png\n");
    res->block = sfTexture_createFromFile("res/block.png", NULL);
    if (res->block == NULL)
        printf("Warning: Could not load res/block.png\n");
    res->player_icon = sfTexture_createFromFile("res/player_icon.png", NULL);
    if (res->player_icon == NULL)
        printf("Warning: Could not load res/player_icon.png\n");
    res->ship_icon = sfTexture_createFromFile("res/ship_icon.png", NULL);
    if (res->ship_icon == NULL)
        printf("Warning: Could not load res/ship_icon.png\n");
    res->end_level_background = sfTexture_createFromFile("res/end_level_background.png", NULL);
    if (res->end_level_background == NULL)
        printf("Warning: Could not load res/end_level_background.png\n");
    res->retry_button = sfTexture_createFromFile("res/retry_button.png", NULL);
    if (res->retry_button == NULL)
        printf("Warning: Could not load res/retry_button.png\n");
    res->quit_button = sfTexture_createFromFile("res/quit_button.png", NULL);
    if (res->quit_button == NULL)
        printf("Warning: Could not load res/quit_button.png\n");
    return res;
}

void free_musics(music_t *musics)
{
    if (musics == NULL)
        return;
    if (musics->editor != NULL)
        sfMusic_destroy(musics->editor);
    if (musics->main != NULL)
        sfMusic_destroy(musics->main);
    if (musics->param != NULL)
        sfMusic_destroy(musics->param);
    if (musics->level1 != NULL)
        sfMusic_destroy(musics->level1);
    free(musics);
}

music_t *load_musics(void)
{
    music_t *musics = malloc(sizeof(music_t));

    musics->main = sfMusic_createFromFile("res/menuLoop.mp3");
    musics->editor = sfMusic_createFromFile("res/back_mus.ogg");
    musics->param = sfMusic_createFromFile("res/back_mus.ogg");
    musics->level1 = sfMusic_createFromFile("res/back_mus.ogg");
    return musics;
}

void free_gd(gd_t *gd)
{
    free_textures(gd->res);
    free_musics(gd->musics);
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
    gd->musics = load_musics();
    gd->main_font = sfFont_createFromFile("res/GDfont.ttf");
    if (gd->main_font == NULL)
        printf("Warning: Could not load res/GDfont.ttf\n");
    gd->w = create_window(1920, 1080);
    gd->cursor = create_cursor();
    gd->event = malloc(sizeof(sfEvent));
    gd->menu = 'm';
    gd->selected_level = 1;
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
    if ((*level)->level_completed == 'y')
        print_cursor(gd->cursor, gd->w);
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
