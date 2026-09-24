/*
** ALEXNEX PROJECT, 2026
** gd
** File description:
** my_gd main file
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

static sfTexture *load_texture(const char *path)
{
    sfTexture *tex = sfTexture_createFromFile(path, NULL);

    if (tex == NULL) {
        dprintf(2, "my_gd: missing asset %s\n", path);
        exit(84);
    }
    return tex;
}

textures_t *load_textures(void)
{
    textures_t *res = xcalloc(1, sizeof(textures_t));

    res->main_background = load_texture("res/main_background.png");
    res->edi_background = load_texture("res/cecilya.png");
    res->list_background = load_texture("res/level_list_background.png");
    res->opt_background = load_texture("res/noe_background.jpeg");
    res->level_background = load_texture("res/level_background.png");
    res->play_button = load_texture("res/play_button.png");
    res->opt_button = load_texture("res/param_button.png");
    res->onli_button = load_texture("res/online_button.png");
    res->ground = load_texture("res/ground.png");
    res->spike = load_texture("res/spike.png");
    res->block = load_texture("res/block.png");
    res->player_icon = load_texture("res/player_icon.png");
    res->ship_icon = load_texture("res/ship_icon.png");
    res->end_level_background = load_texture("res/end_level_background.png");
    res->retry_button = load_texture("res/retry_button.png");
    res->quit_button = load_texture("res/quit_button.png");
    return res;
}

static sfMusic *load_music(const char *path)
{
    sfMusic *music = sfMusic_createFromFile(path);

    if (music == NULL) {
        dprintf(2, "my_gd: missing asset %s\n", path);
        exit(84);
    }
    return music;
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
    music_t *musics = xcalloc(1, sizeof(music_t));

    musics->main = load_music("res/menuLoop.mp3");
    musics->editor = load_music("res/back_mus.ogg");
    musics->param = load_music("res/back_mus.ogg");
    musics->level1 = load_music("res/back_mus.ogg");
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
    gd_t *gd = xcalloc(1, sizeof(gd_t));

    gd->res = load_textures();
    gd->musics = load_musics();
    gd->main_font = sfFont_createFromFile("res/GDfont.ttf");
    if (gd->main_font == NULL) {
        dprintf(2, "my_gd: missing asset res/GDfont.ttf\n");
        exit(84);
    }
    gd->w = create_window(1920, 1080);
    gd->cursor = create_cursor();
    gd->event = xcalloc(1, sizeof(sfEvent));
    gd->menu = 'm';
    gd->selected_level = 1;
    return gd;
}

void handle_playing(gd_t *gd, level_t **level)
{
    if (*level == NULL)
        *level = start_level(gd);
    if (*level == NULL) {
        gd->menu = 'l';
        return;
    }
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

static void chdir_to_executable(void)
{
    char path[4096];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    char *slash;

    if (n <= 0)
        return;
    path[n] = '\0';
    slash = strrchr(path, '/');
    if (slash == NULL)
        return;
    *slash = '\0';
    if (chdir(path) != 0)
        dprintf(2, "my_gd: cannot enter %s\n", path);
}

static void print_usage(int fd)
{
    const char *msg = "usage: ./my_gd [-h]\n";

    write(fd, msg, strlen(msg));
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        chdir_to_executable();
        return main_loop(create_gd());
    }
    if (argc == 2 && strcmp(argv[1], "-h") == 0) {
        my_putstr("GD :)\n");
        my_putstr("🭀 🭁 🭂 🭃 🭄 🭅 🭆 🭇 🭈 🭉 🭊 🭋 🭌 🭍 🭎 🭏 🭐 🭑 🭒 🭓 🭔 🭕 🭖 🭗 🭘 🭙\n");
        print_usage(1);
        return 0;
    }
    print_usage(2);
    return 84;
}
