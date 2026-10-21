/*
** ALEXNEX PROJECT, 2026
** editor_panel
** File description:
** the properties panel: what is selected, a row per property (FEATURES 11.4)
*/

#include "editor.h"

    #define PANEL_W 380.0f
    #define PANEL_X (VIEW_W - PANEL_W - 10.0f)
    #define PANEL_Y (ED_TOOLBAR_H + 10.0f)
    #define ROW_H 50.0f
    #define BUTTON 40.0f
    #define MINUS_X (PANEL_X + 150.0f)
    #define PLUS_X (PANEL_X + PANEL_W - BUTTON - 10.0f)

static const sfColor PANEL = {18, 20, 32, 240};
static const sfColor BUTTON_FILL = {60, 110, 200, 255};

/*
** The selection's rows, found again only when the selection or an edit
** changed them: with thousands selected, a pass per row per frame is
** too much (11.4).
*/
const ed_prop_view_t *editor_props(editor_t *ed)
{
    uint64_t key = ed_selection_key(&ed->lv);

    if (!ed->props_known || key != ed->props_key
        || ed->props_edits != ed->hist.edits) {
        ed_prop_view(&ed->lv, &ed->props);
        ed->props_key = key;
        ed->props_edits = ed->hist.edits;
        ed->props_known = true;
    }
    return &ed->props;
}

/*
** The panel is the selection's properties, shown while something is
** selected. The level's own are its settings window's (11.16).
*/
static bool row_applies(const ed_prop_view_t *v, int k)
{
    return v->applies[k];
}

/* The title, then one row per property that applies. */
static int rows(const ed_prop_view_t *v)
{
    int n = 1;

    for (int k = 0; k < ED_PROP_COUNT; k++)
        n += row_applies(v, k);
    return n;
}

bool editor_panel_holds(editor_t *ed, sfVector2f at)
{
    const ed_prop_view_t *v = editor_props(ed);

    if (v->selected == 0)
        return false;
    return at.x >= PANEL_X && at.x < PANEL_X + PANEL_W && at.y >= PANEL_Y
        && at.y < PANEL_Y + ROW_H * (float)rows(v) + 10.0f;
}

/* The minus and the plus of the row clicked: one step. */
void editor_panel_click(editor_t *ed, gd_t *gd, sfVector2f at)
{
    (void)gd;
    const ed_prop_view_t *v = editor_props(ed);
    int row = (int)((at.y - PANEL_Y) / ROW_H);
    int dir = 0;
    int n = 1;

    if (at.x >= MINUS_X && at.x < MINUS_X + BUTTON)
        dir = -1;
    if (at.x >= PLUS_X && at.x < PLUS_X + BUTTON)
        dir = 1;
    for (int k = 0; k < ED_PROP_COUNT && dir != 0; k++) {
        if (!row_applies(v, k))
            continue;
        if (n == row) {
            ed_prop_step(&ed->lv, &ed->hist, (ed_prop_t)k, dir);
            return;
        }
        n += 1;
    }
}

/* What the one selected object is, as its line would begin. */
static void title(editor_t *ed, char *out, size_t size)
{
    const ed_object_t *one = NULL;
    size_t n = ed->props.selected;

    for (size_t i = 0; i < ed->lv.count && n == 1 && one == NULL; i++)
        if (ed->lv.objects[i].selected)
            one = &ed->lv.objects[i];
    if (n != 1)
        snprintf(out, size, "%zu selected", n);
    else if (one->is_start)
        snprintf(out, size, "start position   %.10g  %.10g",
            one->start.pos.x, one->start.pos.y);
    else
        snprintf(out, size, "%s   %.10g  %.10g", obj_type_name(one->obj.type),
            one->obj.rect.x, one->obj.rect.y);
}

static void draw_row(editor_t *ed, gd_t *gd, int k, float y)
{
    const char *name = ed_prop_name((ed_prop_t)k);
    const char *value = ed->props.text[k];

    editor_label(ed, gd, name, PANEL_X + 14.0f, y + 10.0f, 22,
        (sfColor){180, 190, 210, 255});
    editor_fill(gd, ed, (sfFloatRect){MINUS_X, y + 5.0f, BUTTON, BUTTON},
        BUTTON_FILL, sfTransparent);
    editor_label(ed, gd, "-", MINUS_X + 13.0f, y + 6.0f, 26, sfWhite);
    editor_label_fit(ed, gd, value, (sfVector2f){MINUS_X + BUTTON + 12.0f,
        y + 10.0f}, PLUS_X - MINUS_X - BUTTON - 18.0f, 22, sfWhite);
    editor_fill(gd, ed, (sfFloatRect){PLUS_X, y + 5.0f, BUTTON, BUTTON},
        BUTTON_FILL, sfTransparent);
    editor_label(ed, gd, "+", PLUS_X + 11.0f, y + 6.0f, 26, sfWhite);
}

void editor_panel_draw(editor_t *ed, gd_t *gd)
{
    const ed_prop_view_t *v = editor_props(ed);
    char text[96];
    float y = PANEL_Y + ROW_H;

    if (v->selected == 0)
        return;
    editor_fill(gd, ed, (sfFloatRect){PANEL_X, PANEL_Y, PANEL_W,
        ROW_H * (float)rows(v) + 10.0f}, PANEL, (sfColor){90, 255, 140, 255});
    title(ed, text, sizeof(text));
    editor_label_fit(ed, gd, text, (sfVector2f){PANEL_X + 14.0f,
        PANEL_Y + 12.0f}, PANEL_W - 28.0f, 22, sfWhite);
    for (int k = 0; k < ED_PROP_COUNT; k++) {
        if (!row_applies(v, k))
            continue;
        draw_row(ed, gd, k, y);
        y += ROW_H;
    }
}
