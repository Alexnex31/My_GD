/*
** ALEXNEX PROJECT, 2026
** level_list_menu
** File description:
** manage level_list_menu
*/

#include "mygd.h"

void free_level_button(level_button_t *lb)
{
    if (lb == NULL)
        return;
    if (lb->filename != NULL)
        free(lb->filename);
    if (lb->display_name != NULL)
        free(lb->display_name);
    if (lb->play_button != NULL)
        free_button(lb->play_button);
    if (lb->name_text != NULL)
        sfText_destroy(lb->name_text);
    if (lb->attempts_text != NULL)
        sfText_destroy(lb->attempts_text);
    if (lb->best_text != NULL)
        sfText_destroy(lb->best_text);
    free(lb);
}

void free_level_list_menu(level_list_t *level_list)
{
    int i = 0;

    if (level_list->names != NULL) {
        while (level_list->names[i] != NULL) {
            free(level_list->names[i]);
            i += 1;
        }
        free(level_list->names);
    }
    if (level_list->level_buttons != NULL) {
        i = 0;
        while (i < level_list->nb_levels) {
            free_level_button(level_list->level_buttons[i]);
            i += 1;
        }
        free(level_list->level_buttons);
    }
    sfSprite_destroy(level_list->background);
    free(level_list);
}

void print_level_list(level_list_t *level_list, sfRenderWindow *w)
{
    int i = 0;

    sfRenderWindow_drawSprite(w, level_list->background, NULL);
    while (i < level_list->nb_levels) {
        if (level_list->level_buttons[i] != NULL) {
            print_button(level_list->level_buttons[i]->play_button, w);
            sfRenderWindow_drawText(w, level_list->level_buttons[i]->name_text, NULL);
            sfRenderWindow_drawText(w, level_list->level_buttons[i]->attempts_text, NULL);
            sfRenderWindow_drawText(w, level_list->level_buttons[i]->best_text, NULL);
        }
        i += 1;
    }
}

int count_levels(void)
{
    int nb = 0;
    DIR *d = opendir("levels");
    struct dirent *dir;

    if (d == NULL)
        return 0;
    dir = readdir(d);
    while (dir != NULL) {
        if (dir->d_name[0] != '.')
            nb += 1;
        dir = readdir(d);
    }
    closedir(d);
    return nb;
}

char **fill_names_list(void)
{
    int nb = count_levels();
    DIR *d = opendir("levels");
    struct dirent *dir;
    char **names;
    int i = 0;

    if (d == NULL)
        return NULL;
    dir = readdir(d);
    names = xcalloc(nb + 1, sizeof(char *));
    while (dir != NULL) {
        if (dir->d_name[0] != '.') {
            names[i] = strdup(dir->d_name);
            i += 1;
        }
        dir = readdir(d);
    }
    names[i] = NULL;
    closedir(d);
    return names;
}

void read_level_info(char *filepath, int *level_num, int *attempts, float *best)
{
    FILE *f = fopen(filepath, "r");
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    char **arr;

    *level_num = 0;
    *attempts = 0;
    *best = 0.0f;
    if (f == NULL)
        return;
    nread = getline(&line, &len, f);
    if (nread <= 0) {
        fclose(f);
        return;
    }
    arr = my_str_to_word_array(line);
    if (arr[0] != NULL)
        *level_num = atoi(arr[0]);
    if (arr[1] != NULL)
        *attempts = atoi(arr[1]);
    if (arr[2] != NULL)
        *best = atof(arr[2]);
    free_arr(arr);
    free(line);
    fclose(f);
}

char *create_display_name(char *filename)
{
    int len = strlen(filename);
    char *name = malloc(len + 1);
    int i = 0;

    while (filename[i] != '\0') {
        if (filename[i] >= 'a' && filename[i] <= 'z')
            name[i] = filename[i] - 32;
        else
            name[i] = filename[i];
        i += 1;
    }
    name[i] = '\0';
    return name;
}

level_button_t *create_level_button(char *filename, int index, gd_t *gd)
{
    level_button_t *lb = xcalloc(1, sizeof(level_button_t));
    char filepath[256];
    char *attempts_str;
    char *best_str;
    float x = 200 + (index % 3) * 500;
    float y = 200 + (index / 3) * 250;
    sfVector2f text_pos;

    snprintf(filepath, 256, "levels/%s", filename);
    lb->filename = strdup(filepath);
    lb->display_name = create_display_name(filename);
    read_level_info(filepath, &lb->level_num, &lb->attempts, &lb->best);
    lb->play_button = create_button(x + 200, y + 100, 100, gd->res->play_button);
    lb->name_text = sfText_create();
    text_pos = create_vector_f(x, y);
    sfText_setString(lb->name_text, lb->display_name);
    sfText_setFont(lb->name_text, gd->main_font);
    sfText_setCharacterSize(lb->name_text, 35);
    sfText_setPosition(lb->name_text, text_pos);
    sfText_setOutlineThickness(lb->name_text, 2);
    lb->attempts_text = sfText_create();
    attempts_str = malloc(50);
    snprintf(attempts_str, 50, "Attempts: %d", lb->attempts);
    text_pos = create_vector_f(x, y + 50);
    sfText_setString(lb->attempts_text, attempts_str);
    sfText_setFont(lb->attempts_text, gd->main_font);
    sfText_setCharacterSize(lb->attempts_text, 25);
    sfText_setPosition(lb->attempts_text, text_pos);
    lb->best_text = sfText_create();
    best_str = float_to_str(lb->best);
    text_pos = create_vector_f(x, y + 80);
    sfText_setString(lb->best_text, best_str);
    sfText_setFont(lb->best_text, gd->main_font);
    sfText_setCharacterSize(lb->best_text, 25);
    sfText_setPosition(lb->best_text, text_pos);
    free(attempts_str);
    free(best_str);
    return lb;
}

level_list_t *create_level_list(gd_t *gd)
{
    level_list_t *menu = xcalloc(1, sizeof(level_list_t));
    int i = 0;

    sfMusic_stop(gd->musics->main);
    sfMusic_stop(gd->musics->level1);
    menu->background = sfSprite_create();
    sfSprite_setTexture(menu->background, gd->res->list_background, sfTrue);
    menu->names = fill_names_list();
    menu->nb_levels = count_levels();
    menu->level_buttons = xcalloc(menu->nb_levels + 1, sizeof(level_button_t *));
    while (i < menu->nb_levels && menu->names[i] != NULL) {
        menu->level_buttons[i] = create_level_button(menu->names[i], i, gd);
        i += 1;
    }
    gd->menu = 'l';
    return menu;
}
