/*
** ALEXNEX PROJECT, 2026
** level
** File description:
** functions to create and manage a level
*/

#include "mygd.h"

void free_block(block_t *block)
{
    sfSprite_destroy(block->sprite);
    free(block);
}

void free_spike(spike_t *spike)
{
    sfSprite_destroy(spike->sprite);
    free(spike);
}

void free_block_list(block_t **list)
{
    int i = 0;

    if (list == NULL)
        return;
    while (list[i] != NULL) {
        free_block(list[i]);
        i += 1;
    }
    free(list);
}

void free_spike_list(spike_t **list)
{
    int i = 0;

    if (list == NULL)
        return;
    while (list[i] != NULL) {
        free_spike(list[i]);
        i += 1;
    }
    free(list);
}

void free_objects(object_list_t *obj_l)
{
    if (obj_l != NULL) {
        free_block(obj_l->ground);
        free_spike_list(obj_l->spikes);
        free_block_list(obj_l->blocks);
        if (obj_l->portal_blocks != NULL) {
            free_block_list(obj_l->portal_blocks);
            obj_l->portal_blocks = NULL;
        }
        free(obj_l);
    }
}

void free_end_level_screen(end_level_screen_t *end_screen)
{
    if (end_screen == NULL)
        return;
    if (end_screen->background != NULL)
        sfSprite_destroy(end_screen->background);
    if (end_screen->title_text != NULL)
        sfText_destroy(end_screen->title_text);
    if (end_screen->attempts_text != NULL)
        sfText_destroy(end_screen->attempts_text);
    if (end_screen->percent_text != NULL)
        sfText_destroy(end_screen->percent_text);
    if (end_screen->retry_button != NULL)
        free_button(end_screen->retry_button);
    if (end_screen->quit_button != NULL)
        free_button(end_screen->quit_button);
    free(end_screen);
}

void free_level(level_t *level)
{
    if (level == NULL)
        return;
    free_player(level->player);
    if (level->background != NULL)
        sfSprite_destroy(level->background);
    if (level->attempt_text != NULL)
        sfText_destroy(level->attempt_text);
    if (level->percent_text != NULL)
        sfText_destroy(level->percent_text);
    if (level->attempt_display_clock != NULL)
        sfClock_destroy(level->attempt_display_clock);
    free_end_level_screen(level->end_screen);
    free_objects(level->objects);
    free(level);
}

void update_percent_display(level_t *level)
{
    char percent_str[50];
    
    level->percent = (level->shift / level->level_end) * 100.0f;
    if (level->percent > 100.0f)
        level->percent = 100.0f;
    if (level->percent < 0.0f)
        level->percent = 0.0f;
    snprintf(percent_str, 50, "%.2f%%", level->percent);
    sfText_setString(level->percent_text, percent_str);
}

void print_objects(gd_t *gd, level_t *level, object_list_t *obj)
{
    int i = 0;

    if (obj->ground != NULL) {
        sfSprite_setPosition(obj->ground->sprite, obj->sprite_ground_pos);
        sfRenderWindow_drawSprite(gd->w, obj->ground->sprite, NULL);
    }
    if (obj->blocks != NULL) {
        while (obj->blocks[i] != NULL) {
            if (obj->blocks[i]->pos.x < 2000 && obj->blocks[i]->pos.x > -100) {
                sfSprite_setPosition(obj->blocks[i]->sprite, obj->blocks[i]->pos);
                sfRenderWindow_drawSprite(gd->w, obj->blocks[i]->sprite, NULL);
            }
            i += 1;
        }
    }
    i = 0;
    if (obj->spikes != NULL) {
        while (obj->spikes[i] != NULL) {
            if (obj->spikes[i]->pos.x < 2000 && obj->spikes[i]->pos.x > -100) {
                sfSprite_setPosition(obj->spikes[i]->sprite, obj->spikes[i]->pos);
                sfRenderWindow_drawSprite(gd->w, obj->spikes[i]->sprite, NULL);
            }
            i += 1;
        }
    }
    i = 0;
    if (obj->portals != NULL) {
        while (obj->portals[i] != NULL) {
            if (obj->portals[i]->pos.x < 2000 && obj->portals[i]->pos.x > -100) {
                sfSprite_setPosition(obj->portals[i]->sprite, obj->portals[i]->pos);
                sfRenderWindow_drawSprite(gd->w, obj->portals[i]->sprite, NULL);
            }
            i += 1;
        }
    }
}

void print_player(gd_t *gd, level_t *level, object_list_t *obj)
{
    if (level->player != NULL) {
        if (level->player->sprite != NULL) {
            if (level->player->gamemode == 'c') {
                if (level->player->pos.y < 750 && level->player->allow_jump == 'n')
                    sfSprite_rotate(level->player->sprite, 5.4);
                else
                    sfSprite_setRotation(level->player->sprite, 0);
            }
            if (level->player->gamemode == 'p') {
                sfSprite_setRotation(level->player->sprite, 0);
            }
            sfSprite_setPosition(level->player->sprite, level->player->pos);
            sfRenderWindow_drawSprite(gd->w, level->player->sprite, NULL);
        }
    }
}

void print_ui_texts(gd_t *gd, level_t *level)
{
    sfTime elapsed;
    float seconds;

    if (level->percent_text != NULL) {
        update_percent_display(level);
        sfRenderWindow_drawText(gd->w, level->percent_text, NULL);
    }
    if (level->show_attempt_text == 'y' && level->attempt_text != NULL) {
        elapsed = sfClock_getElapsedTime(level->attempt_display_clock);
        seconds = sfTime_asSeconds(elapsed);
        if (seconds < 1.5f) {
            sfRenderWindow_drawText(gd->w, level->attempt_text, NULL);
        } else {
            level->show_attempt_text = 'n';
        }
    }
}

end_level_screen_t *create_end_level_screen(level_t *level, gd_t *gd)
{
    end_level_screen_t *end_screen = malloc(sizeof(end_level_screen_t));
    char attempts_str[100];
    char percent_str[100];
    sfVector2f pos;

    end_screen->background = sfSprite_create();
    if (gd->res->end_level_background != NULL)
        sfSprite_setTexture(end_screen->background, gd->res->end_level_background, sfTrue);
    
    end_screen->title_text = sfText_create();
    sfText_setString(end_screen->title_text, "LEVEL COMPLETE!");
    sfText_setFont(end_screen->title_text, gd->main_font);
    sfText_setCharacterSize(end_screen->title_text, 80);
    pos = create_vector_f(600, 200);
    sfText_setPosition(end_screen->title_text, pos);
    sfText_setOutlineThickness(end_screen->title_text, 4);
    sfText_setFillColor(end_screen->title_text, sfWhite);
    
    end_screen->attempts_text = sfText_create();
    snprintf(attempts_str, 100, "Attempts: %d", level->curr_attempts);
    sfText_setString(end_screen->attempts_text, attempts_str);
    sfText_setFont(end_screen->attempts_text, gd->main_font);
    sfText_setCharacterSize(end_screen->attempts_text, 50);
    pos = create_vector_f(700, 400);
    sfText_setPosition(end_screen->attempts_text, pos);
    sfText_setOutlineThickness(end_screen->attempts_text, 3);
    sfText_setFillColor(end_screen->attempts_text, sfWhite);
    
    end_screen->percent_text = sfText_create();
    snprintf(percent_str, 100, "Completion: 100%%");
    sfText_setString(end_screen->percent_text, percent_str);
    sfText_setFont(end_screen->percent_text, gd->main_font);
    sfText_setCharacterSize(end_screen->percent_text, 50);
    pos = create_vector_f(680, 500);
    sfText_setPosition(end_screen->percent_text, pos);
    sfText_setOutlineThickness(end_screen->percent_text, 3);
    sfText_setFillColor(end_screen->percent_text, sfWhite);
    
    end_screen->retry_button = create_button(600, 700, 250, gd->res->retry_button);
    end_screen->quit_button = create_button(1000, 700, 250, gd->res->quit_button);
    
    return end_screen;
}

void print_end_level_screen(gd_t *gd, end_level_screen_t *end_screen)
{
    if (end_screen->background != NULL)
        sfRenderWindow_drawSprite(gd->w, end_screen->background, NULL);
    sfRenderWindow_drawText(gd->w, end_screen->title_text, NULL);
    sfRenderWindow_drawText(gd->w, end_screen->attempts_text, NULL);
    sfRenderWindow_drawText(gd->w, end_screen->percent_text, NULL);
    print_button(end_screen->retry_button, gd->w);
    print_button(end_screen->quit_button, gd->w);
}

void print_level(gd_t *gd, level_t *level)
{
    if (level->shift == 0)
        sfMusic_play(gd->musics->level1);
    if (level->level_completed == 'y') {
        if (level->end_screen == NULL)
            level->end_screen = create_end_level_screen(level, gd);
        print_end_level_screen(gd, level->end_screen);
        return;
    }
    
    if (level->background != NULL)
        sfRenderWindow_drawSprite(gd->w, level->background, NULL);
    print_objects(gd, level, level->objects);
    if (level->objects->portal_blocks != NULL) {
        sfRenderWindow_drawSprite(gd->w, level->objects->portal_blocks[0]->sprite, NULL);
        sfRenderWindow_drawSprite(gd->w, level->objects->portal_blocks[1]->sprite, NULL);
    }
    print_player(gd, level, level->objects);
    print_ui_texts(gd, level);
    apply_physics(gd, level, level->objects);
    check_collisions(gd, level, level->objects);
}

void create_ui_texts(level_t *level, gd_t *gd)
{
    char attempt_str[50];
    sfVector2f attempt_pos = {860, 500};
    sfVector2f percent_pos = {920, 20};

    level->attempt_text = sfText_create();
    snprintf(attempt_str, 50, "Attempt %d", level->curr_attempts);
    sfText_setString(level->attempt_text, attempt_str);
    sfText_setFont(level->attempt_text, gd->main_font);
    sfText_setCharacterSize(level->attempt_text, 50);
    sfText_setPosition(level->attempt_text, attempt_pos);
    sfText_setOutlineThickness(level->attempt_text, 3);
    sfText_setFillColor(level->attempt_text, sfWhite);
    
    level->percent_text = sfText_create();
    sfText_setString(level->percent_text, "0%");
    sfText_setFont(level->percent_text, gd->main_font);
    sfText_setCharacterSize(level->percent_text, 40);
    sfText_setPosition(level->percent_text, percent_pos);
    sfText_setOutlineThickness(level->percent_text, 2);
    sfText_setFillColor(level->percent_text, sfWhite);
    
    level->attempt_display_clock = sfClock_create();
    level->show_attempt_text = 'y';
}

void reset_attempt_display(level_t *level)
{
    char attempt_str[50];
    
    snprintf(attempt_str, 50, "Attempt %d", level->curr_attempts);
    sfText_setString(level->attempt_text, attempt_str);
    sfClock_restart(level->attempt_display_clock);
    level->show_attempt_text = 'y';
    level->player->gamemode = 'c';
}

level_t *start_level(gd_t *gd)
{
    level_t *level = malloc(sizeof(level_t));
    char levelpath[256];

    level->background = sfSprite_create();
    if (level->background == NULL) {
        printf("Error: Failed to create background sprite\n");
        level->objects = malloc(sizeof(object_list_t));
        level->best = 0.0f;
        level->percent = 0.0f;
        level->attempts = 1;
        level->lvl = gd->selected_level;
        return level;
    }
    
    if (gd->res->level_background == NULL) {
        printf("Error: level_background texture is NULL\n");
    } else {
        sfSprite_setTexture(level->background, gd->res->level_background, sfTrue);
    }
    level->player = create_player(gd);
    level->objects = malloc(sizeof(object_list_t));
    level->speed = 1;
    level->shift = 0;
    level->yshift = 0;
    level->level_end = 100;
    level->best = 0.0f;
    level->percent = 0.0f;
    level->attempts = 0;
    level->curr_attempts = 1;
    level->lvl = gd->selected_level;
    level->level_completed = 'n';
    level->end_screen = NULL;
    snprintf(levelpath, 256, "levels/level%d", gd->selected_level);
    load_level_data(levelpath, level, gd);
    level->attempts += 1;
    create_ui_texts(level, gd);
    return level;
}
