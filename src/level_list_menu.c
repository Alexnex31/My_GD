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

/* A level is levels/<digits>.gd and nothing else (7.2, 7.5). */
static bool level_file_id(const char *name, char *id, size_t size)
{
    size_t n = strlen(name);

    if (n < 4 || strcmp(name + n - 3, ".gd") != 0 || n - 3 >= size)
        return false;
    for (size_t i = 0; i < n - 3; i++)
        if (name[i] < '0' || name[i] > '9')
            return false;
    memcpy(id, name, n - 3);
    id[n - 3] = '\0';
    return true;
}

static int cmp_id(const void *a, const void *b)
{
    long ia = atol(*(char *const *)a);
    long ib = atol(*(char *const *)b);

    return (ia > ib) - (ia < ib);            /* 2 before 10, unlike strcmp */
}

/* The ids in levels/, sorted numerically. NULL terminated. */
char **fill_names_list(void)
{
    DIR *d = opendir("levels");
    struct dirent *dir = NULL;
    char **ids = NULL;
    char id[24];
    int n = 0;

    if (d == NULL)
        return NULL;
    ids = xcalloc(257, sizeof(char *));
    for (dir = readdir(d); dir != NULL && n < 256; dir = readdir(d)) {
        if (level_file_id(dir->d_name, id, sizeof(id))) {
            ids[n] = strdup(id);
            n += 1;
        } else if (dir->d_name[0] != '.')
            dprintf(2, "my_gd: levels/%s is not <digits>.gd, skipped\n",
                dir->d_name);
    }
    closedir(d);
    qsort(ids, n, sizeof(char *), cmp_id);
    return ids;
}

/* The list's own names, not a second scan: two scans can disagree (7.5). */
static int count_names(char **names)
{
    int n = 0;

    while (names != NULL && names[n] != NULL)
        n += 1;
    return n;
}

/* The prose name from the file's header, the numbers from the store (6.4). */
static void fill_level_info(level_button_t *lb, gd_t *gd)
{
    level_header_t hdr = {0};
    const progress_entry_t *pe = progress_find(&gd->progress, lb->id);
    uint64_t hash = 0;

    if (level_read_header(lb->filename, &hdr, &hash) == 0)
        lb->display_name = strdup(hdr.name);
    else
        lb->display_name = strdup(lb->id);
    lb->file_hash = hash;
    if (pe == NULL)
        return;
    lb->attempts = pe->attempts;
    lb->best = pe->best;
    lb->edited = pe->level_hash != 0 && pe->level_hash != hash;
}

level_button_t *create_level_button(char *id, int index, gd_t *gd)
{
    level_button_t *lb = xcalloc(1, sizeof(level_button_t));
    char filepath[256];
    char *attempts_str;
    char *best_str;
    float x = 200 + (index % 3) * 500;
    float y = 200 + (index / 3) * 250;
    sfVector2f text_pos;

    snprintf(filepath, 256, "levels/%s.gd", id);
    lb->filename = strdup(filepath);
    snprintf(lb->id, sizeof(lb->id), "%s", id);
    fill_level_info(lb, gd);
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
    best_str = xcalloc(64, 1);
    snprintf(best_str, 64, "Best: %.2f%%%s", progress_printable(lb->best),
        lb->edited ? " (edited)" : "");
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
    menu->nb_levels = count_names(menu->names);
    menu->level_buttons = xcalloc(menu->nb_levels + 1, sizeof(level_button_t *));
    while (i < menu->nb_levels) {
        menu->level_buttons[i] = create_level_button(menu->names[i], i, gd);
        i += 1;
    }
    gd->menu = 'l';
    return menu;
}
