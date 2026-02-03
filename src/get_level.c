/*
** ALEXNEX PROJECT, 2026
** get_level
** File description:
** load data from a level file
*/

#include "mygd.h"

void load_block(char **arr, level_t *level, gd_t *gd)
{
    level->objects->id += 1;
}

void load_spike(char **arr, level_t *level, gd_t *gd)
{
    level->objects->id += 1;
}

void load_object(char *line, level_t *level, gd_t *gd)
{
    char **arr = my_str_to_word_array(line);

    printf("object\n");
    if (strcmp(arr[0], "spike") == 0) {
        load_spike(arr, level, gd);
        free_arr(arr);
        return;
    }
    if (strcmp(arr[0], "block") == 0) {
        load_block(arr, level, gd);
        free_arr(arr);
        return;
    }
    free_arr(arr);
}

void manage_first_line(char *line, level_t *level, gd_t *gd)
{
    char **arr = my_str_to_word_array(line);

    level->lvl = atoi(arr[0]);
    level->attempts = atoi(arr[1]);
    level->best = atoi(arr[2]);
    free_arr(arr);
}

void load_level_data(char *levelname, level_t *level, gd_t *gd)
{
    FILE *f = fopen(levelname, "r");
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    if (f == NULL)
        return;
    nread = getline(&line, &len, f);
    if (nread <= 0) {
        fclose(f);
        return;
    }
    printf("loading level...\n");
    manage_first_line(line, level, gd);
    nread = getline(&line, &len, f);
    while (nread > 0) {
        load_object(line, level, gd);
        nread = getline(&line, &len, f);
    }
    free(line);
    fclose(f);
}
