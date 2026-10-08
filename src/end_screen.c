/*
** ALEXNEX PROJECT, 2026
** end_screen
** File description:
** the screen shown when a level is completed (6.3)
*/

#include "mygd.h"

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
    if (end_screen->song_text != NULL)
        sfText_destroy(end_screen->song_text);
    if (end_screen->retry_button != NULL)
        free_button(end_screen->retry_button);
    if (end_screen->quit_button != NULL)
        free_button(end_screen->quit_button);
    free(end_screen);
}

static sfText *end_text(gd_t *gd, const char *string, unsigned int size,
    sfVector2f pos)
{
    sfText *text = sfText_create();

    sfText_setString(text, string);
    sfText_setFont(text, gd->main_font);
    sfText_setCharacterSize(text, size);
    sfText_setPosition(text, pos);
    sfText_setOutlineThickness(text, 3);
    sfText_setFillColor(text, sfWhite);
    return text;
}

/*
** The attempts shown are this session's: how many it took this time. The
** store keeps the total, and the level list shows it (6.2).
*/
end_level_screen_t *create_end_level_screen(level_t *level, gd_t *gd)
{
    end_level_screen_t *es = sim_xcalloc(1, sizeof(end_level_screen_t));
    const song_t *song = level->song.song;
    bool counts = level_current_start(level) == &level->sim.lvl.hdr.start;
    char attempts[100];
    char credit[200];

    es->background = sfSprite_create();
    if (gd->res->end_level_background != NULL)
        sfSprite_setTexture(es->background, gd->res->end_level_background,
            sfTrue);
    snprintf(attempts, sizeof(attempts), "Attempts: %d",
        level->stats.attempts);
    es->title_text = end_text(gd, counts ? "LEVEL COMPLETE!"
        : "END REACHED", 80, (sfVector2f){600.0f, 200.0f});
    es->attempts_text = end_text(gd, attempts, 50,
        (sfVector2f){700.0f, 400.0f});
    es->percent_text = end_text(gd, counts ? "Completion: 100%"
        : "From a start position: it doesn't count", 50,
        (sfVector2f){counts ? 680.0f : 420.0f, 500.0f});   /* (11.8) */
    if (song != NULL)                        /* CC BY wants the credit (4.11) */
        snprintf(credit, sizeof(credit), "Song: %s \xe2\x80\x94 %s (%s)",
            song->title, song->artist, song->license);
    else
        snprintf(credit, sizeof(credit), "%s", "No song");
    es->song_text = end_text(gd, "", 30, (sfVector2f){600.0f, 600.0f});
    sfText_setUnicodeString(es->song_text, utf8_to_utf32(credit));
    es->retry_button = create_button(600, 700, 250, gd->res->retry_button);
    es->quit_button = create_button(1000, 700, 250, gd->res->quit_button);
    return es;
}

void print_end_level_screen(gd_t *gd, end_level_screen_t *end_screen)
{
    if (end_screen->background != NULL)
        sfRenderWindow_drawSprite(gd->w, end_screen->background, NULL);
    sfRenderWindow_drawText(gd->w, end_screen->title_text, NULL);
    sfRenderWindow_drawText(gd->w, end_screen->attempts_text, NULL);
    sfRenderWindow_drawText(gd->w, end_screen->percent_text, NULL);
    sfRenderWindow_drawText(gd->w, end_screen->song_text, NULL);
    print_button(end_screen->retry_button, gd->w);
    print_button(end_screen->quit_button, gd->w);
}
