/*
** ALEXNEX PROJECT, 2026
** level
** File description:
** the level scene: the fixed-timestep loop, death, respawn (3.6, 6.1)
*/

#include "mygd.h"

static void log_level_warning(const char *msg)
{
    dprintf(2, "my_gd: %s\n", msg);
}

void level_count_attempt(level_t *lv)
{
    char text[64];

    lv->stats.attempts += 1;
    lv->stats.dirty = true;
    if (lv->attempt_text == NULL)
        return;                              /* the first one, before the text */
    snprintf(text, sizeof(text), "Attempt %d", lv->stats.attempts);
    sfText_setString(lv->attempt_text, text);
}

void level_record_best(level_t *lv)
{
    float pct = sim_percent(&lv->sim);

    if (pct <= lv->stats.best)
        return;
    lv->stats.best = pct;
    lv->stats.dirty = true;
}

/* The one place that touches the store and the file (6.2, 6.4). */
void level_flush_stats(level_t *lv, gd_t *gd)
{
    progress_entry_t *pe = NULL;

    if (!lv->stats.dirty)
        return;
    pe = progress_get(&gd->progress, lv->id);
    if (pe == NULL)
        return;
    pe->attempts += lv->stats.attempts;
    if (lv->stats.best > pe->best) {
        pe->best = lv->stats.best;
        pe->level_hash = lv->file_hash;      /* the version it was set on */
    }
    if (lv->stats.practice_best > pe->practice_best)
        pe->practice_best = lv->stats.practice_best;
    lv->stats = (level_stats_t){0};
    progress_save(&gd->progress);
}

level_t *level_start(gd_t *gd, const char *id)
{
    level_t *lv = xcalloc(1, sizeof(level_t));
    char path[300];

    snprintf(path, sizeof(path), "levels/%s.gd", id);
    if (sim_load(&lv->sim, path, log_level_warning) != 0) {
        dprintf(2, "my_gd: cannot load %s\n", path);
        free(lv);
        return NULL;
    }
    snprintf(lv->id, sizeof(lv->id), "%s", id);
    level_read_header(path, &lv->sim.lvl.hdr, &lv->file_hash);
    lv->state = LEVEL_PLAYING;
    lv->clock = sfClock_create();
    level_build_chunks(lv, gd);
    lv->seen_spent = xcalloc(lv->sim.st.spent_words + 1, sizeof(uint64_t));
    lv->flash_sprite = sfSprite_create();
    sfSprite_setTexture(lv->flash_sprite, gd->atlas, sfTrue);
    lv->player_sprite = sfSprite_create();
    lv->ground_sprite = sfSprite_create();
    sfTexture_setRepeated(gd->res->ground, sfTrue);      /* tiled, not stretched */
    sfSprite_setTexture(lv->ground_sprite, gd->res->ground, sfTrue);
    lv->background_sprite = sfSprite_create();
    sfTexture_setRepeated(gd->res->level_background, sfTrue);
    sfSprite_setTexture(lv->background_sprite, gd->res->level_background,
        sfTrue);
    sfTexture_setRepeated(gd->res->block, sfTrue);   /* the corridor strips (5.3) */
    lv->strip_sprite = sfSprite_create();
    sfSprite_setTexture(lv->strip_sprite, gd->res->block, sfTrue);
    lv->explosion_sprite = sfSprite_create();
    sfSprite_setTexture(lv->explosion_sprite, gd->res->explosion, sfTrue);
    sfSprite_setOrigin(lv->explosion_sprite, (sfVector2f){55.0f, 55.0f});
    lv->hud_text = sfText_create();
    sfText_setFont(lv->hud_text, gd->main_font);
    sfText_setCharacterSize(lv->hud_text, 40);
    sfText_setOutlineThickness(lv->hud_text, 3);
    lv->attempt_text = sfText_create();
    sfText_setFont(lv->attempt_text, gd->main_font);
    sfText_setCharacterSize(lv->attempt_text, 60);
    sfText_setOutlineThickness(lv->attempt_text, 4);
    lv->bar_back = sfRectangleShape_create();
    sfRectangleShape_setSize(lv->bar_back, (sfVector2f){BAR_W, BAR_H});
    sfRectangleShape_setFillColor(lv->bar_back, (sfColor){0, 0, 0, 120});
    sfRectangleShape_setOutlineThickness(lv->bar_back, 3.0f);
    sfRectangleShape_setOutlineColor(lv->bar_back, sfWhite);
    sfRectangleShape_setPosition(lv->bar_back,
        (sfVector2f){(VIEW_W - BAR_W) / 2.0f, 30.0f});
    lv->flash = sfRectangleShape_create();
    sfRectangleShape_setSize(lv->flash, (sfVector2f){VIEW_W, VIEW_H});
    lv->bar_fill = sfRectangleShape_create();
    sfRectangleShape_setFillColor(lv->bar_fill, (sfColor){80, 220, 120, 255});
    sfRectangleShape_setPosition(lv->bar_fill,
        (sfVector2f){(VIEW_W - BAR_W) / 2.0f, 30.0f});
    level_count_attempt(lv);
    sfMusic_play(gd->musics->level1);
    return lv;
}

void level_free(level_t *lv, gd_t *gd)
{
    if (lv == NULL)
        return;
    level_flush_stats(lv, gd);               /* every way out goes through here */
    if (lv->end_screen != NULL)
        free_end_level_screen(lv->end_screen);
    level_free_chunks(lv);
    sfSprite_destroy(lv->flash_sprite);
    free(lv->seen_spent);
    sfSprite_destroy(lv->player_sprite);
    sfSprite_destroy(lv->ground_sprite);
    sfSprite_destroy(lv->background_sprite);
    sfSprite_destroy(lv->strip_sprite);
    sfSprite_destroy(lv->explosion_sprite);
    sfText_destroy(lv->hud_text);
    sfText_destroy(lv->attempt_text);
    sfRectangleShape_destroy(lv->bar_back);
    sfRectangleShape_destroy(lv->bar_fill);
    sfRectangleShape_destroy(lv->flash);
    sfClock_destroy(lv->clock);
    sim_free(&lv->sim);
    free(lv);
}

void level_on_death(level_t *lv, gd_t *gd)
{
    level_record_best(lv);
    lv->state = LEVEL_DYING;
    lv->death_ticks = DEATH_DELAY_TICKS;
    lv->death_pos = lv->sim.st.player.pos;
    lv->fade_left = 0.0f;                    /* a death clears the old strips */
    sfMusic_stop(gd->musics->level1);
}

void level_respawn(level_t *lv, gd_t *gd)
{
    sim_reset(&lv->sim);
    lv->state = LEVEL_PLAYING;
    lv->fade_left = 0.0f;
    lv->drawn_bounds = lv->sim.st.bounds;
    memset(lv->seen_spent, 0, lv->sim.st.spent_words * sizeof(uint64_t));
    for (int i = 0; i < MAX_FLASHES; i++)
        lv->flash_left[i] = 0.0f;
    lv->accumulator = 0;
    sfClock_restart(lv->clock);
    level_count_attempt(lv);
    sfMusic_play(gd->musics->level1);        /* play on a stopped music restarts it */
}

/* A voluntary death: the percentage counts, but no delay and no explosion. */
void level_restart(level_t *lv, gd_t *gd)
{
    if (lv->state == LEVEL_PLAYING)
        level_record_best(lv);
    level_respawn(lv, gd);
}

void level_on_complete(level_t *lv, gd_t *gd)
{
    level_record_best(lv);                   /* sim_percent is 100 here */
    lv->state = LEVEL_COMPLETE;
    lv->end_time = 0.0f;                     /* the end sequence starts (9.8) */
    if (lv->end_screen == NULL)
        lv->end_screen = create_end_level_screen(lv, gd);
}

void level_step(level_t *lv, gd_t *gd, input_t in)
{
    if (lv->state == LEVEL_DYING) {
        lv->death_ticks -= 1;
        if (lv->death_ticks <= 0)
            level_respawn(lv, gd);
        return;
    }
    if (lv->state != LEVEL_PLAYING)
        return;
    sim_tick(&lv->sim, in);
    if (!lv->sim.st.player.alive)
        level_on_death(lv, gd);
    else if (lv->sim.st.complete)
        level_on_complete(lv, gd);
}

/*
** Events, then whole ticks, then one render (3.6). The accumulator is an
** integer number of microseconds x TICK_RATE, so a tick costs exactly
** 1000000 of it and nothing ever rounds.
*/
void handle_playing(gd_t *gd, level_t **level)
{
    level_t *lv = NULL;
    sfInt64 frame_us = 0;

    if (*level == NULL)
        *level = level_start(gd, gd->selected_id);
    lv = *level;
    if (lv == NULL) {
        gd->menu = 'l';
        return;
    }
    keyboard_events_playing(level, gd);
    if (*level == NULL || !sfRenderWindow_isOpen(gd->w))
        return;                              /* the scene changed */
    frame_us = sfClock_restart(lv->clock).microseconds;
    if (frame_us > 250000)
        frame_us = 250000;                   /* no burst of ticks after a hitch */
    lv->frame_us = frame_us;                 /* the renderer's own timers (5.3) */
    lv->accumulator += frame_us * TICK_RATE;
    while (lv->accumulator >= 1000000) {
        level_step(lv, gd, input_for_tick(gd));
        lv->accumulator -= 1000000;
    }
    level_render(gd, lv);
}
