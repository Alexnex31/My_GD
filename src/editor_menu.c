/*
** ALEXNEX PROJECT, 2026
** editor_menu
** File description:
** the editor's scene: pick a level or name a new one, then edit it (FEATURES 11)
*/

#include "editor.h"

char **fill_names_list(void);

static void on_open(void *ctx, widget_t *w)
{
    (void)w;
    ((editor_m_t *)ctx)->want_open = true;
}

static void on_new(void *ctx, widget_t *w)
{
    (void)w;
    ((editor_m_t *)ctx)->want_new = true;
}

static void on_back(void *ctx)
{
    ((editor_m_t *)ctx)->want_back = true;
}

static void free_rows(editor_m_t *m)
{
    for (int i = 0; i < m->nb; i++) {
        free(m->ids[i]);
        free(m->rows[i]);
    }
    free(m->ids);
    free(m->rows);
    m->ids = NULL;
    m->rows = NULL;
    m->nb = 0;
}

/* The levels of levels/, each with the name its header gives it. */
static void build_rows(editor_m_t *m)
{
    level_header_t hdr;
    char path[64];

    free_rows(m);
    m->ids = fill_names_list();
    while (m->ids != NULL && m->ids[m->nb] != NULL && m->nb < ED_PICKER_MAX)
        m->nb += 1;
    m->rows = sim_xcalloc((size_t)m->nb + 1, sizeof(char *));
    for (int i = 0; i < m->nb; i++) {
        m->rows[i] = sim_xcalloc(1, 192);
        snprintf(path, sizeof(path), "levels/%s.gd", m->ids[i]);
        if (level_read_header(path, &hdr, NULL) != 0)
            snprintf(hdr.name, sizeof(hdr.name), "?");
        snprintf(m->rows[i], 192, "%s    %s", m->ids[i], hdr.name);
    }
    if (m->row >= m->nb)
        m->row = m->nb > 0 ? m->nb - 1 : 0;
}

/* The widgets point into the rows, so they are laid out again with them. */
static void layout(editor_m_t *m)
{
    m->widgets[0] = (widget_t){.kind = W_LIST, .label = NULL,
        .bounds = {360.0f, 200.0f, 1200.0f, 10.0f * UI_ROW_H},
        .enabled = m->nb > 0, .value = &m->row, .max = m->nb - 1,
        .choices = (const char *const *)m->rows, .on_activate = on_open};
    m->widgets[1] = (widget_t){.kind = W_TEXT, .label = "New level's name",
        .bounds = {760.0f, 860.0f, 520.0f, 70.0f}, .enabled = true,
        .text = m->name, .text_cap = (int)sizeof(m->name),
        .on_activate = on_new};
    m->widgets[2] = (widget_t){.kind = W_BUTTON, .label = "Create",
        .bounds = {1300.0f, 860.0f, 260.0f, 70.0f}, .enabled = true,
        .on_activate = on_new};
    ui_init(&m->ui, m->widgets, 3, m);
    m->ui.on_back = on_back;
}

editor_m_t *create_editor_menu(gd_t *gd)
{
    editor_m_t *menu = sim_xcalloc(1, sizeof(editor_m_t));

    music_menu(gd);                          /* it plays on (FEATURES 4.10) */
    menu->gd = gd;
    menu->background = sfSprite_create();
    sfSprite_setTexture(menu->background, gd->res->edi_background, sfTrue);
    build_rows(menu);
    layout(menu);
    gd->menu = 'e';
    return menu;
}

void free_editor_menu(editor_m_t *om)
{
    editor_free(om->ed);
    free_rows(om);
    sfSprite_destroy(om->background);
    free(om);
}

void print_editor_menu(editor_m_t *om, gd_t *gd)
{
    if (om->ed != NULL)
        return editor_draw(om->ed, gd);
    sfRenderWindow_drawSprite(gd->w, om->background, NULL);
    ui_update(&om->ui, ui_now_ms());
    ui_draw(gd, &om->ui);
}

/* A new level takes the id after the highest there is (7.2). */
static void open_new(editor_m_t *m, gd_t *gd)
{
    char id[LEVEL_ID_MAX + 1];

    ed_next_id((const char *const *)m->ids, (size_t)m->nb, id, sizeof(id));
    m->ed = editor_open(gd, id, m->name);
    m->name[0] = '\0';
}

/* What the callbacks asked for, once the events are done (PLAN 10.1). */
static void act(editor_m_t **menu, gd_t *gd)
{
    editor_m_t *m = *menu;

    if (m->want_back) {
        free_editor_menu(m);
        *menu = NULL;
        gd->menu = 'm';
        return;
    }
    if (m->want_open && m->nb > 0)
        m->ed = editor_open(gd, m->ids[m->row], NULL);
    else if (m->want_new)
        open_new(m, gd);
    m->want_open = false;
    m->want_new = false;
}

void keyboard_events_editor_menu(editor_m_t **editor_m, gd_t *gd)
{
    editor_m_t *m = *editor_m;
    ui_event_t ev;

    if (m->ed != NULL) {
        editor_events(m->ed, gd);
        if (!m->ed->leave || !sfRenderWindow_isOpen(gd->w))
            return;
        editor_free(m->ed);                  /* back to the list, as it is now */
        m->ed = NULL;
        build_rows(m);
        return layout(m);
    }
    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed)
            return close_window(gd->w);
        if (ui_from_sf(gd, gd->event, &ev))
            ui_event(&m->ui, &ev, ui_now_ms());
    }
    act(editor_m, gd);
}
