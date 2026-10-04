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
    if (res->explosion != NULL)
        sfTexture_destroy(res->explosion);
    if (res->spike != NULL)
        sfTexture_destroy(res->spike);
    if (res->block != NULL)
        sfTexture_destroy(res->block);
    if (res->cube_portal != NULL)
        sfTexture_destroy(res->cube_portal);
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
    textures_t *res = sim_xcalloc(1, sizeof(textures_t));

    res->main_background = load_texture("res/main_background.png");
    res->edi_background = load_texture("res/cecilya.png");
    res->list_background = load_texture("res/level_list_background.png");
    res->opt_background = load_texture("res/noe_background.jpeg");
    res->level_background = load_texture("res/level_background.png");
    res->play_button = load_texture("res/play_button.png");
    res->opt_button = load_texture("res/param_button.png");
    res->onli_button = load_texture("res/online_button.png");
    res->ground = load_texture("res/ground.png");
    res->explosion = load_texture("res/explos.png");
    res->spike = load_texture("res/spike.png");
    res->block = load_texture("res/block.png");
    res->cube_portal = load_texture("res/cube_portal.png");
    res->player_icon = load_texture("res/player_icon.png");
    res->ship_icon = load_texture("res/ship_icon.png");
    res->end_level_background = load_texture("res/end_level_background.png");
    res->retry_button = load_texture("res/retry_button.png");
    res->quit_button = load_texture("res/quit_button.png");
    return res;
}

void free_gd(gd_t *gd)
{
    settings_free(&gd->settings);
    ui_gfx_free(gd);
    progress_free(&gd->progress);
    sfTexture_destroy(gd->atlas);
    sfView_destroy(gd->ui_view);
    sfView_destroy(gd->level_view);
    free_textures(gd->res);
    music_free(&gd->music);
    library_free(&gd->library);
    sfFont_destroy(gd->main_font);
    free_cursor(gd->cursor);
    free(gd->event);
    destroy_all(gd->w);
    free(gd);
}

static void log_warning(const char *msg)
{
    dprintf(2, "my_gd: %s\n", msg);
}

/*
** The library, then the menu's song: the setting's, else the library's
** first with one warning, else silence (FEATURES 4.2).
*/
static void music_init(gd_t *gd)
{
    const song_t *menu = NULL;

    library_scan(&gd->library, MUSIC_DIR, music_probe, log_warning);
    gd->music.volume = gd->settings.music_volume;
    gd->music.audio_offset = gd->settings.audio_offset_ms / 1000.0;
    menu = music_menu_song(&gd->library, gd->settings.menu_song);
    if (menu != NULL && strcmp(menu->file, gd->settings.menu_song) != 0)
        dprintf(2, "my_gd: menu song %s isn't in %s/, playing %s\n",
            gd->settings.menu_song, MUSIC_DIR, menu->file);
    if (menu != NULL)
        snprintf(gd->music.menu_file, sizeof(gd->music.menu_file), "%s",
            menu->file);
}

gd_t *create_gd(void)
{
    gd_t *gd = sim_xcalloc(1, sizeof(gd_t));

    settings_load(&gd->settings, SETTINGS_PATH, log_warning);  /* before the window */
    gd->res = load_textures();
    music_init(gd);
    gd->main_font = sfFont_createFromFile("res/GDfont.ttf");
    if (gd->main_font == NULL) {
        dprintf(2, "my_gd: missing asset res/GDfont.ttf\n");
        exit(84);
    }
    gd->w = create_window(&gd->settings);
    if (gd->w == NULL) {
        dprintf(2, "my_gd: cannot open a window\n");
        exit(84);
    }
    ui_gfx_create(gd);
    gd->ui_view = sfView_createFromRect((sfFloatRect){0, 0, VIEW_W, VIEW_H});
    gd->level_view = sfView_createFromRect((sfFloatRect){0, 0, VIEW_W, VIEW_H});
    apply_letterbox(gd, sfRenderWindow_getSize(gd->w).x,
        sfRenderWindow_getSize(gd->w).y);
    gd->cursor = create_cursor();
    gd->event = sim_xcalloc(1, sizeof(sfEvent));
    gd->menu = 'm';
    snprintf(gd->selected_id, sizeof(gd->selected_id), "1");
    atlas_build(gd);
    progress_load(&gd->progress, SAVE_PATH);
    return gd;
}

void handle_level_list(gd_t *gd, level_list_t **level_list)
{
    if (*level_list == NULL)
        *level_list = create_level_list(gd);
    print_level_list(*level_list, gd->w);
    song_picker_draw(*level_list, gd);
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
    print_option_menu(*option_menu, gd);
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
        sfRenderWindow_clear(gd->w, sfBlack);
        sfRenderWindow_setView(gd->w, gd->ui_view);
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
        level_free(level, gd);
    if ((gd->settings.dirty || access(gd->settings.path, F_OK) != 0)
        && !gd->settings.unreadable && settings_save(&gd->settings) != 0)
        dprintf(2, "my_gd: cannot save %s\n", gd->settings.path);   /* 5.7 */
    free_gd(gd);
    return 0;
}

int exe_dir(char *buf, size_t size)
{
    ssize_t n = readlink("/proc/self/exe", buf, size - 1);
    char *slash = NULL;

    if (n <= 0)
        return -1;
    buf[n] = '\0';
    slash = strrchr(buf, '/');
    if (slash == NULL)
        return -1;
    *slash = '\0';
    return 0;
}

static void chdir_to_executable(void)
{
    char path[4096];

    if (exe_dir(path, sizeof(path)) == 0 && chdir(path) != 0)
        dprintf(2, "my_gd: cannot enter %s\n", path);
}

static void print_usage(int fd)
{
    const char *msg = "usage: ./my_gd [-h | --check <level.gd>]\n";

    write(fd, msg, strlen(msg));
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        input_init_threads();                /* before any X call */
        chdir_to_executable();
        return main_loop(create_gd());
    }
    if (argc == 3 && strcmp(argv[1], "--check") == 0)
        return level_check(argv[2]);         /* the user's path: no chdir */
    if (argc == 2 && strcmp(argv[1], "-h") == 0) {
        printf("GD :)\n");
        printf("🭀 🭁 🭂 🭃 🭄 🭅 🭆 🭇 🭈 🭉 🭊 🭋 🭌 🭍 🭎 🭏 🭐 🭑 🭒 🭓 🭔 🭕 🭖 🭗 🭘 🭙\n");
        fflush(stdout);                      /* before print_usage's write */
        print_usage(1);
        return 0;
    }
    print_usage(2);
    return 84;
}
