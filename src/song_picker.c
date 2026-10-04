/*
** ALEXNEX PROJECT, 2026
** song_picker
** File description:
** the level list's song line and the picker that overrides it (FEATURES 4.6)
*/

#include "mygd.h"

#define PICKER_ROWS 10

static const char *override_of(gd_t *gd, const level_button_t *lb)
{
    const progress_entry_t *pe = progress_find(&gd->progress, lb->id);

    return pe == NULL ? NULL : pe->song;
}

/* "Song: Title — Artist", what the level plays now (4.4, 4.6). */
void song_line_refresh(gd_t *gd, level_button_t *lb)
{
    song_choice_t c = music_choose(&gd->library, override_of(gd, lb),
        &lb->hdr, SONG_OVERRIDE);
    char line[200];

    if (c.song == NULL)
        snprintf(line, sizeof(line), "%s", "Song: none");
    else
        snprintf(line, sizeof(line), "Song: %s \xe2\x80\x94 %s%s",
            c.song->title, c.song->artist,
            c.source == SONG_OVERRIDE ? " (yours)" : "");
    sfText_setUnicodeString(lb->song_text, utf8_to_utf32(line));
}

/* "Title — Artist   3:05   CC BY 4.0", marked when it's the one in use. */
static char *song_row(const song_t *s, bool current)
{
    char row[256];
    int secs = (int)(s->duration + 0.5f);

    snprintf(row, sizeof(row), "%s%s \xe2\x80\x94 %s   %d:%02d   %s",
        current ? "* " : "", s->title, s->artist, secs / 60, secs % 60,
        s->license);
    return strdup(row);
}

static void picker_close(level_list_t *list)
{
    list->picker_closing = true;             /* freed after the events (3.5) */
}

/* Each row moved to plays its 10 s; the first is the level's own song. */
static void picker_moved(void *ctx, widget_t *w)
{
    level_list_t *list = ctx;
    gd_t *gd = list->gd;
    level_button_t *lb = list->level_buttons[list->picker_for];

    if (*w->value == 0)
        music_preview(gd, music_choose(&gd->library, NULL, &lb->hdr,
            SONG_LEVEL).song);
    else
        music_preview(gd, &gd->library.songs[*w->value - 1]);
}

/* Enter: the choice goes to the progress file's song= (4.6). */
static void picker_chosen(void *ctx, widget_t *w)
{
    level_list_t *list = ctx;
    gd_t *gd = list->gd;
    level_button_t *lb = list->level_buttons[list->picker_for];
    progress_entry_t *pe = progress_get(&gd->progress, lb->id);

    if (pe != NULL) {
        snprintf(pe->song, sizeof(pe->song), "%s", *w->value == 0 ? ""
            : gd->library.songs[*w->value - 1].file);
        if (progress_save(&gd->progress) != 0)
            dprintf(2, "my_gd: cannot save %s\n", gd->progress.path);
        song_line_refresh(gd, lb);
    }
    picker_close(list);
}

static void picker_back(void *ctx)
{
    picker_close(ctx);
}

static void build_rows(level_list_t *list, gd_t *gd, const char *override)
{
    const library_t *lib = &gd->library;

    list->picker_rows = sim_xcalloc(lib->count + 1, sizeof(char *));
    list->picker_row = 0;
    list->picker_rows[0] = strdup(override == NULL || override[0] == '\0'
        ? "* Default (level's song)" : "Default (level's song)");
    for (size_t i = 0; i < lib->count; i++) {
        bool current = override != NULL
            && strcmp(override, lib->songs[i].file) == 0;

        list->picker_rows[i + 1] = song_row(&lib->songs[i], current);
        if (current)
            list->picker_row = (int)i + 1;
    }
}

void song_picker_open(level_list_t *list, gd_t *gd, int index)
{
    level_button_t *lb = list->level_buttons[index];
    char title[200];

    list->gd = gd;
    list->picker_for = index;
    build_rows(list, gd, override_of(gd, lb));
    list->picker_list = (widget_t){W_LIST, NULL, {360.0f, 220.0f, 1200.0f,
        PICKER_ROWS * UI_ROW_H}, true, &list->picker_row, NULL, 0,
        (int)gd->library.count, 1, (const char *const *)list->picker_rows, 0,
        picker_moved, picker_chosen, NULL};
    if (list->picker_row >= PICKER_ROWS)
        list->picker_list.scroll = list->picker_row - PICKER_ROWS + 1;
    ui_init(&list->picker, &list->picker_list, 1, list);
    list->picker.on_back = picker_back;
    ui_init(&list->picker_root, NULL, 0, list);
    list->picker_root.modal = &list->picker;
    list->picker_title = sfText_create();
    sfText_setFont(list->picker_title, gd->main_font);
    sfText_setCharacterSize(list->picker_title, 45);
    sfText_setOutlineThickness(list->picker_title, 3);
    snprintf(title, sizeof(title), "Song for %s   (Enter: choose, Esc:"
        " cancel)", lb->display_name);
    sfText_setUnicodeString(list->picker_title, utf8_to_utf32(title));
    sfText_setPosition(list->picker_title, (sfVector2f){360.0f, 140.0f});
}

/* Its rows and title, if it's open: also when the list goes away. */
void song_picker_discard(level_list_t *list)
{
    if (list->picker_for < 0)
        return;
    for (size_t i = 0; i <= list->gd->library.count; i++)
        free(list->picker_rows[i]);
    free(list->picker_rows);
    list->picker_rows = NULL;
    sfText_destroy(list->picker_title);
    list->picker_title = NULL;
    list->picker_for = -1;
    list->picker_closing = false;
}

/* After the frame's events: what a callback closed is freed here. */
void song_picker_finish(level_list_t *list, gd_t *gd)
{
    if (!list->picker_closing)
        return;
    song_picker_discard(list);
    music_menu(gd);                          /* the preview ends (4.6) */
}

void song_picker_draw(level_list_t *list, gd_t *gd)
{
    if (list->picker_for < 0)
        return;
    ui_draw(gd, &list->picker_root);
    draw_text(gd, list->picker_title);
}

/* The picker has every event while it's open. */
void song_picker_event(level_list_t *list, gd_t *gd)
{
    ui_event_t ev;

    if (ui_from_sf(gd, gd->event, &ev))
        ui_event(&list->picker_root, &ev, ui_now_ms());
}

void song_picker_update(level_list_t *list, gd_t *gd)
{
    music_update(gd);
    if (list->picker_for >= 0)
        ui_update(&list->picker_root, ui_now_ms());
}
