/*
** ALEXNEX PROJECT, 2026
** level_list_menu
** File description:
** manage level_list_menu
*/

#include "mygd.h"

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
    sfSprite_destroy(level_list->background);
    free(level_list);
}

void print_level_list(level_list_t *level_list, sfRenderWindow *w)
{
    int i = 0;

    sfRenderWindow_drawSprite(w, level_list->background, NULL);
    printf("\n\n");
    while (level_list->names[i] != NULL) {
        printf("%s\n", level_list->names[i]);
        i += 1;
    }
    printf("\n\n");
}

int count_levels(void)
{
    int nb = 0;
    DIR *d = opendir("levels");
    struct dirent *dir = readdir(d);

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
    struct dirent *dir = readdir(d);
    char **names = malloc(sizeof(char *) * (nb + 1));
    int i = 0;

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

level_list_t *create_level_list(gd_t *gd)
{
    level_list_t *menu = malloc(sizeof(level_list_t));

    menu->background = sfSprite_create();
    sfSprite_setTexture(menu->background, gd->res->list_background, sfTrue);
    menu->names = fill_names_list();
    gd->menu = 'l';
    return menu;
}