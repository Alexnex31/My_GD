/*
** ALEXNEX PROJECT, 2026
** get_level
** File description:
** load data from a level file
*/

#include "mygd.h"

int load_block(char **arr, level_t *level, gd_t *gd, int nb_blocks)
{
    block_t *block = xcalloc(1, sizeof(block_t));

    nb_blocks += 1;
    block->pos = (sfVector2f){atof(arr[1]), atof(arr[2])};
    if (atof(arr[1]) + 500 > level->level_end)
        level->level_end = atof(arr[1]) + 500;
    block->size = atoi(arr[3]);
    block->sprite = sfSprite_create();
    sfSprite_setTexture(block->sprite, gd->res->block, sfTrue);
    sfSprite_setPosition(block->sprite, block->pos);
    level->objects->blocks = realloc(level->objects->blocks, sizeof(block_t *) * (nb_blocks + 1));
    level->objects->blocks[nb_blocks - 1] = block;
    level->objects->blocks[nb_blocks] = NULL;
    return nb_blocks;
}

int load_spike(char **arr, level_t *level, gd_t *gd, int nb_spikes)
{
    spike_t *spike = xcalloc(1, sizeof(spike_t));

    nb_spikes += 1;
    spike->pos = (sfVector2f){atof(arr[1]), atof(arr[2])};
    if (atof(arr[1]) + 500 > level->level_end)
        level->level_end = atof(arr[1]) + 500;
    spike->size = atoi(arr[3]);
    spike->sprite = sfSprite_create();
    sfSprite_setTexture(spike->sprite, gd->res->spike, sfTrue);
    sfSprite_setPosition(spike->sprite, spike->pos);
    level->objects->spikes = realloc(level->objects->spikes, sizeof(spike_t *) * (nb_spikes + 1));
    level->objects->spikes[nb_spikes - 1] = spike;
    level->objects->spikes[nb_spikes] = NULL;
    return nb_spikes;
}

void load_object(char *line, level_t *level, gd_t *gd, int *nb_blocks, int *nb_spikes)
{
    char **arr = my_str_word_array_delim(line, " \n\0");

    if (strcmp(arr[0], "spike") == 0) {
        *nb_spikes = load_spike(arr, level, gd, *nb_spikes);
        free_arr(arr);
        return;
    }
    if (strcmp(arr[0], "block") == 0) {
        *nb_blocks = load_block(arr, level, gd, *nb_blocks);
        free_arr(arr);
        return;
    }
    if (strcmp(arr[0], "portal") == 0) {
        load_portal(arr, level, gd);
        free_arr(arr);
        return;
    }
    free_arr(arr);
}

void manage_first_line(char *line, level_t *level)
{
    char **arr = my_str_to_word_array(line);

    level->lvl = atoi(arr[0]);
    level->attempts = atoi(arr[1]);
    level->best = atoi(arr[2]);
    free_arr(arr);
}

block_t *create_ground(gd_t *gd)
{
    block_t *ground = xcalloc(1, sizeof(block_t));

    ground->pos = (sfVector2f){350, 850};
    ground->size = 1;
    ground->sprite = sfSprite_create();
    sfSprite_setTexture(ground->sprite, gd->res->ground, sfTrue);
    sfSprite_setPosition(ground->sprite, ground->pos);
    return ground;
}

int load_level_data(char *levelname, level_t *level, gd_t *gd)
{
    FILE *f = fopen(levelname, "r");
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    int nb_blocks = 0;
    int nb_spikes = 0;
    sfVector2f sprite_ground_pos = {0, 850};

    if (f == NULL)
        return -1;
    nread = getline(&line, &len, f);
    if (nread <= 0) {
        free(line);
        fclose(f);
        return -1;
    }
    level->objects->ground = create_ground(gd);
    level->objects->sprite_ground_pos = sprite_ground_pos;
    level->objects->portal_blocks = NULL;
    level->objects->blocks = NULL;
    level->objects->spikes = NULL;
    level->objects->portals = NULL;
    level->objects->nb_portals = 0;
    manage_first_line(line, level);
    nread = getline(&line, &len, f);
    while (nread > 0) {
        load_object(line, level, gd, &nb_blocks, &nb_spikes);
        nread = getline(&line, &len, f);
    }
    free(line);
    fclose(f);
    return 0;
}

void rewrite_level(level_t *level, gd_t *gd)
{
    FILE *f;
    FILE *tempf;
    char filename[256];
    char temp_filename[262];
    char line[500];
    int line_num = 1;
    int curr_line_num = 1;

    if (level->percent > level->best) {
        level->best = level->percent;
    }
    snprintf(filename, 256, "levels/level%d", gd->selected_level);
    f = fopen(filename, "r");
    if (f == NULL) {
        printf("Error opening file.");
        return;
    }
    snprintf(temp_filename, sizeof(temp_filename), "%s.temp", filename);
    tempf = fopen(temp_filename, "w");
    if (tempf == NULL) {
        printf("Error creating temporary file.");
        fclose(f);
        return;
    }
    while (fgets(line, 500, f) != NULL) {
        if (curr_line_num == line_num) {
            snprintf(line, 500, "%d %d %f\n", gd->selected_level, level->attempts, level->best);
            fputs(line, tempf);
        } else {
            fputs(line, tempf);
        }
        curr_line_num++;
    }
    fclose(f);
    fclose(tempf);
    rename(temp_filename, filename);
}
