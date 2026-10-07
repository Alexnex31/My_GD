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

/* The title, then one row per property something selected has. */
static int rows(const editor_t *ed)
{
    int n = 1;

    for (int p = 0; p < ED_PROP_COUNT; p++)
        n += ed_prop_applies(&ed->lv, (ed_prop_t)p);
    return n;
}

bool editor_panel_holds(editor_t *ed, sfVector2f at)
{
    if (ed_selected(&ed->lv) == 0)
        return false;
    return at.x >= PANEL_X && at.x < PANEL_X + PANEL_W && at.y >= PANEL_Y
        && at.y < PANEL_Y + ROW_H * (float)rows(ed) + 10.0f;
}

/* The minus and the plus of the row clicked: one step, one command. */
void editor_panel_click(editor_t *ed, sfVector2f at)
{
    int row = (int)((at.y - PANEL_Y) / ROW_H);
    int dir = 0;
    int n = 1;

    if (at.x >= MINUS_X && at.x < MINUS_X + BUTTON)
        dir = -1;
    if (at.x >= PLUS_X && at.x < PLUS_X + BUTTON)
        dir = 1;
    if (dir == 0)
        return;
    for (int p = 0; p < ED_PROP_COUNT; p++) {
        if (!ed_prop_applies(&ed->lv, (ed_prop_t)p))
            continue;
        if (n == row)
            ed_prop_step(&ed->lv, &ed->hist, (ed_prop_t)p, dir);
        n += 1;
    }
}

/* What the one selected object is, as its line would begin. */
static void title(editor_t *ed, char *out, size_t size)
{
    const ed_object_t *one = NULL;
    size_t n = ed_selected(&ed->lv);

    for (size_t i = 0; i < ed->lv.count; i++)
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

static void draw_row(editor_t *ed, gd_t *gd, ed_prop_t p, float y)
{
    char value[64];

    ed_prop_text(&ed->lv, p, value, sizeof(value));
    editor_label(ed, gd, ed_prop_name(p), PANEL_X + 14.0f, y + 10.0f, 22,
        (sfColor){180, 190, 210, 255});
    editor_fill(gd, ed, (sfFloatRect){MINUS_X, y + 5.0f, BUTTON, BUTTON},
        BUTTON_FILL, sfTransparent);
    editor_label(ed, gd, "-", MINUS_X + 13.0f, y + 6.0f, 26, sfWhite);
    editor_label(ed, gd, value, MINUS_X + BUTTON + 16.0f, y + 10.0f, 22,
        sfWhite);
    editor_fill(gd, ed, (sfFloatRect){PLUS_X, y + 5.0f, BUTTON, BUTTON},
        BUTTON_FILL, sfTransparent);
    editor_label(ed, gd, "+", PLUS_X + 11.0f, y + 6.0f, 26, sfWhite);
}

void editor_panel_draw(editor_t *ed, gd_t *gd)
{
    char text[96];
    float y = PANEL_Y + ROW_H;

    if (ed_selected(&ed->lv) == 0)
        return;
    editor_fill(gd, ed, (sfFloatRect){PANEL_X, PANEL_Y, PANEL_W,
        ROW_H * (float)rows(ed) + 10.0f}, PANEL, (sfColor){90, 255, 140, 255});
    title(ed, text, sizeof(text));
    editor_label(ed, gd, text, PANEL_X + 14.0f, PANEL_Y + 12.0f, 22, sfWhite);
    for (int p = 0; p < ED_PROP_COUNT; p++) {
        if (!ed_prop_applies(&ed->lv, (ed_prop_t)p))
            continue;
        draw_row(ed, gd, (ed_prop_t)p, y);
        y += ROW_H;
    }
}
