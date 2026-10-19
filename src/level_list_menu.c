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
    if (lb->song_text != NULL)
        sfText_destroy(lb->song_text);
    free(lb);
}

void free_level_list_menu(level_list_t *level_list)
{
    int i = 0;

    song_picker_discard(level_list);
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
    free(level_list->widgets);
    sfSprite_destroy(level_list->background);
    sfView_destroy(level_list->scrolled);
    free(level_list);
}

/* How far the grid can go up: until its last row is whole on screen. */
float level_list_max_scroll(const level_list_t *list)
{
    int rows = (list->nb_levels + LIST_COLUMNS - 1) / LIST_COLUMNS;
    float bottom = LIST_TOP + (float)rows * LIST_ROW_H;

    return bottom > VIEW_H ? bottom - VIEW_H : 0.0f;
}

void level_list_scroll(level_list_t *list, float by)
{
    float max = level_list_max_scroll(list);

    list->scroll += by;
    list->scroll = list->scroll < 0.0f ? 0.0f : list->scroll;
    list->scroll = list->scroll > max ? max : list->scroll;
}

/* The song line's widget is where its text is: it changes with the song. */
static void song_bounds(level_list_t *list, int i)
{
    sfFloatRect b = sfText_getGlobalBounds(list->level_buttons[i]->song_text);

    list->widgets[2 * i + 1].bounds = (ui_rect_t){b.left - 8.0f,
        b.top - 8.0f, b.width + 16.0f, b.height + 16.0f};
}

/*
** The background stays; the levels are drawn through the same view moved
** down by the scroll, so more than three rows of them can be reached.
** Their widgets are in that moved space too (level_list_event).
*/
void print_level_list(level_list_t *list, gd_t *gd)
{
    const sfView *ui = gd->ui_view;
    sfVector2f c = sfView_getCenter(ui);

    sfRenderWindow_drawSprite(gd->w, list->background, NULL);
    sfView_setViewport(list->scrolled, sfView_getViewport(ui));
    sfView_setCenter(list->scrolled, (sfVector2f){c.x, c.y + list->scroll});
    sfRenderWindow_setView(gd->w, list->scrolled);
    for (int i = 0; i < list->nb_levels; i++) {
        level_button_t *lb = list->level_buttons[i];

        ui_draw_sprite(gd, &list->ui, 2 * i, lb->play_button->sprite);
        sfRenderWindow_drawText(gd->w, lb->name_text, NULL);
        sfRenderWindow_drawText(gd->w, lb->attempts_text, NULL);
        sfRenderWindow_drawText(gd->w, lb->best_text, NULL);
        song_bounds(list, i);
        sfText_setFillColor(lb->song_text, ui_look(&list->ui, 2 * i + 1).state
            == UI_HOVER ? sfWhite : (sfColor){180, 230, 255, 255});
        sfRenderWindow_drawText(gd->w, lb->song_text, NULL);
        ui_draw_focus(gd, &list->ui, 2 * i + 1);
    }
    sfRenderWindow_setView(gd->w, gd->ui_view);
}

/* The callbacks only say what was chosen: it happens after the events. */
static void play_chosen(void *ctx, widget_t *w)
{
    level_list_t *list = ctx;

    list->pending_play = (int)(w - list->widgets) / 2;
}

static void song_chosen(void *ctx, widget_t *w)
{
    level_list_t *list = ctx;

    list->pending_song = (int)(w - list->widgets) / 2;
}

static void list_back(void *ctx)
{
    ((level_list_t *)ctx)->pending_back = true;
}

/* A key moved the focus: the grid scrolls until its level is whole on screen. */
void level_list_follow(level_list_t *list)
{
    int k = list->ui.focused / 2;
    float top = 0.0f;
    float bottom = 0.0f;

    if (!list->follow || list->ui.focused < 0)
        return;
    list->follow = false;
    top = sfText_getGlobalBounds(list->level_buttons[k]->name_text).top
        - 20.0f;
    bottom = list->widgets[2 * k + 1].bounds.y
        + list->widgets[2 * k + 1].bounds.h + 20.0f;
    if (top < list->scroll)
        level_list_scroll(list, top - list->scroll);
    else if (bottom > list->scroll + VIEW_H)
        level_list_scroll(list, bottom - VIEW_H - list->scroll);
}

/* Each level: its Play button, then its song line (4.6). */
static void build_widgets(level_list_t *list)
{
    list->widgets = sim_xcalloc((size_t)list->nb_levels * 2 + 1,
        sizeof(widget_t));
    for (int i = 0; i < list->nb_levels; i++) {
        level_button_t *lb = list->level_buttons[i];

        list->widgets[2 * i] = button_widget(lb->play_button,
            lb->display_name, play_chosen);
        list->widgets[2 * i + 1] = (widget_t){.kind = W_BUTTON,
            .label = "Song", .enabled = true, .on_activate = song_chosen};
        song_bounds(list, i);
    }
    ui_init(&list->ui, list->widgets, list->nb_levels * 2, list);
    list->ui.on_back = list_back;
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
    char **bigger = NULL;
    char id[24];
    int n = 0;
    int cap = 64;

    if (d == NULL)
        return NULL;
    ids = sim_xcalloc((size_t)cap + 1, sizeof(char *));
    for (dir = readdir(d); dir != NULL; dir = readdir(d)) {
        if (level_id_from_path(dir->d_name, id, sizeof(id))) {  /* 7.2, 7.5 */
            if (n == cap) {                  /* every one: a new id is free */
                cap *= 2;
                bigger = sim_xcalloc((size_t)cap + 1, sizeof(char *));
                memcpy(bigger, ids, (size_t)n * sizeof(char *));
                free(ids);
                ids = bigger;
            }
            ids[n] = sim_xstrdup(id);
            n += 1;
        } else if (dir->d_name[0] != '.')
            dprintf(2, "my_gd: levels/%s is not <digits>.gd (at most %d"
                " digits), skipped\n", dir->d_name, LEVEL_ID_MAX);
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
    const progress_entry_t *pe = progress_find(&gd->progress, lb->id);
    uint64_t hash = 0;

    if (level_read_header(lb->filename, &lb->hdr, &hash) == 0)
        lb->display_name = strdup(lb->hdr.name);
    else
        lb->display_name = strdup(lb->id);
    lb->file_hash = hash;
    if (pe == NULL)
        return;
    lb->attempts = pe->attempts;
    lb->best = pe->best;
    lb->practice_best = pe->practice_best;
    lb->edited = pe->level_hash != 0 && pe->level_hash != hash;
}

level_button_t *create_level_button(char *id, int index, gd_t *gd)
{
    level_button_t *lb = sim_xcalloc(1, sizeof(level_button_t));
    char filepath[256];
    char attempts_str[50];
    char best_str[64];
    float x = 120 + (index % LIST_COLUMNS) * 450;
    float y = LIST_TOP + (index / LIST_COLUMNS) * LIST_ROW_H;
    sfVector2f text_pos;

    snprintf(filepath, 256, "levels/%s.gd", id);
    lb->filename = strdup(filepath);
    snprintf(lb->id, sizeof(lb->id), "%s", id);
    fill_level_info(lb, gd);
    lb->play_button = create_button(x + 200, y + 100, 100,
        gd->res->play_button);
    lb->name_text = sfText_create();
    text_pos = (sfVector2f){x, y};
    sfText_setUnicodeString(lb->name_text, utf8_to_utf32(lb->display_name));
    sfText_setFont(lb->name_text, gd->main_font);
    sfText_setCharacterSize(lb->name_text, 35);
    sfText_setPosition(lb->name_text, text_pos);
    sfText_setOutlineThickness(lb->name_text, 2);
    lb->attempts_text = sfText_create();
    snprintf(attempts_str, sizeof(attempts_str), "Attempts: %d", lb->attempts);
    text_pos = (sfVector2f){x, y + 50};
    sfText_setString(lb->attempts_text, attempts_str);
    sfText_setFont(lb->attempts_text, gd->main_font);
    sfText_setCharacterSize(lb->attempts_text, 25);
    sfText_setPosition(lb->attempts_text, text_pos);
    lb->best_text = sfText_create();
    if (lb->practice_best > 0.0f)
        snprintf(best_str, sizeof(best_str), "Best: %.2f%%%s   Practice: %.0f%%",
            progress_printable(lb->best), lb->edited ? " (edited)" : "",
            progress_printable(lb->practice_best));
    else
        snprintf(best_str, sizeof(best_str), "Best: %.2f%%%s", progress_printable(lb->best),
            lb->edited ? " (edited)" : "");
    text_pos = (sfVector2f){x, y + 80};
    sfText_setString(lb->best_text, best_str);
    sfText_setFont(lb->best_text, gd->main_font);
    sfText_setCharacterSize(lb->best_text, 25);
    sfText_setPosition(lb->best_text, text_pos);
    lb->song_text = sfText_create();
    sfText_setFont(lb->song_text, gd->main_font);
    sfText_setCharacterSize(lb->song_text, 22);
    sfText_setPosition(lb->song_text, (sfVector2f){x, y + 205});  /* under Play */
    sfText_setFillColor(lb->song_text, (sfColor){180, 230, 255, 255});
    song_line_refresh(gd, lb);
    return lb;
}

level_list_t *create_level_list(gd_t *gd)
{
    level_list_t *menu = sim_xcalloc(1, sizeof(level_list_t));
    int i = 0;

    music_menu(gd);                          /* back from a level: resumes (4.10) */
    menu->gd = gd;
    menu->picker_for = -1;
    menu->pending_play = -1;
    menu->pending_song = -1;
    menu->background = sfSprite_create();
    sfSprite_setTexture(menu->background, gd->res->list_background, sfTrue);
    menu->scrolled = sfView_copy(gd->ui_view);
    menu->names = fill_names_list();
    menu->nb_levels = count_names(menu->names);
    menu->level_buttons = sim_xcalloc(menu->nb_levels + 1,
        sizeof(level_button_t *));
    while (i < menu->nb_levels) {
        menu->level_buttons[i] = create_level_button(menu->names[i], i, gd);
        i += 1;
    }
    build_widgets(menu);
    gd->menu = 'l';
    return menu;
}
