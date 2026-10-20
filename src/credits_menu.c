/*
** ALEXNEX PROJECT, 2026
** credits_menu
** File description:
** the Credits screen: every song, then res/CREDITS.md (FEATURES 4.11)
*/

#include "mygd.h"
#include "music/credits.h"
#include "sim/save_file.h"

static void credits_back(void *ctx)
{
    ((credits_t *)ctx)->back = true;
}

static void back_pressed(void *ctx, widget_t *w)
{
    (void)w;
    credits_back(ctx);
}

/* A list to read through with the arrows or the wheel, and Back. */
credits_t *create_credits(gd_t *gd)
{
    credits_t *c = sim_xcalloc(1, sizeof(credits_t));
    char *md = NULL;

    music_menu(gd);                          /* it plays on (FEATURES 4.10) */
    if (save_read(CREDITS_FILE, &md) != SAVE_READ)
        md = NULL;
    c->rows = credits_rows(&gd->library, md, &c->nb_rows);
    free(md);
    c->background = sfSprite_create();
    sfSprite_setTexture(c->background, gd->res->list_background, sfTrue);
    c->widgets[0] = (widget_t){.kind = W_LIST, .enabled = true,
        .bounds = {260.0f, 170.0f, 1400.0f, 13.0f * UI_ROW_H},
        .value = &c->row, .max = c->nb_rows - 1,
        .choices = (const char *const *)c->rows};
    c->widgets[1] = (widget_t){.kind = W_BUTTON, .label = "Back",
        .enabled = true, .bounds = {810.0f, 975.0f, 300.0f, 80.0f},
        .on_activate = back_pressed};
    ui_init(&c->ui, c->widgets, 2, c);
    c->ui.on_back = credits_back;
    gd->menu = 'c';
    return c;
}

void free_credits(credits_t *c)
{
    if (c == NULL)
        return;
    credits_free(c->rows);
    sfSprite_destroy(c->background);
    free(c);
}

void print_credits(credits_t *c, gd_t *gd)
{
    sfRenderWindow_drawSprite(gd->w, c->background, NULL);
    ui_update(&c->ui, ui_now_ms());
    ui_draw(gd, &c->ui);
    ui_caption(gd, "Credits", VIEW_W / 2.0f, 110.0f);
}

void keyboard_events_credits(credits_t **c, gd_t *gd)
{
    ui_event_t ev;

    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed)
            return close_window(gd->w);
        if (ui_from_sf(gd, gd->event, &ev))
            ui_event(&(*c)->ui, &ev, ui_now_ms());
    }
    if (!(*c)->back)
        return;
    free_credits(*c);
    *c = NULL;
    gd->menu = 'm';
}
