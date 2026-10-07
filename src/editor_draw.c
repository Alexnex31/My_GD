/*
** ALEXNEX PROJECT, 2026
** editor_draw
** File description:
** the editor on screen: grid, limits, objects, palette, toolbar (FEATURES 11.2, 11.4)
*/

#include "editor.h"
#include "sim/hitbox.h"
#include "sim/modes.h"

static const sfColor CANVAS = {24, 26, 40, 255};
static const sfColor UNDERGROUND = {12, 13, 20, 255};
static const sfColor GRID = {255, 255, 255, 22};
static const sfColor GRID_STRONG = {255, 255, 255, 60};
static const sfColor GROUND_LINE = {255, 255, 255, 220};
static const sfColor END_LINE = {80, 220, 120, 255};
static const sfColor KILL_LINE = {255, 70, 70, 255};
static const sfColor PANEL_FILL = {18, 20, 32, 255};
static const sfColor PICKED = {60, 110, 200, 255};

typedef struct span {         /* what the canvas shows, world coordinates */
    double left;
    double right;
    double top;
    double bottom;
} span_t;

static span_t visible(const editor_t *ed)
{
    double w = ED_CANVAS_W * ed->zoom / 2.0;
    double h = ED_CANVAS_H * ed->zoom / 2.0;

    return (span_t){ed->cam.x - w, ed->cam.x + w, ed->cam.y - h,
        ed->cam.y + h};
}

static void fill(gd_t *gd, editor_t *ed, sfFloatRect r, sfColor c,
    sfColor outline)
{
    sfRectangleShape_setPosition(ed->box, (sfVector2f){r.left, r.top});
    sfRectangleShape_setSize(ed->box, (sfVector2f){r.width, r.height});
    sfRectangleShape_setFillColor(ed->box, c);
    sfRectangleShape_setOutlineColor(ed->box, outline);
    sfRectangleShape_setOutlineThickness(ed->box, outline.a > 0 ? 3.0f : 0.0f);
    draw_rect(gd, ed->box);
}

static void line(editor_t *ed, double x0, double y0, double x1, double y1,
    sfColor c)
{
    sfVertexArray_append(ed->lines, (sfVertex){{(float)x0, (float)y0}, c,
        {0.0f, 0.0f}});
    sfVertexArray_append(ed->lines, (sfVertex){{(float)x1, (float)y1}, c,
        {0.0f, 0.0f}});
}

/*
** Only the lines in view. Zoomed out, lines closer than 12 screen pixels
** would be a grey wash: the step doubles until they aren't. Every fourth
** line of the 50 px grid is brighter: two blocks (11.4).
*/
static void grid_lines(editor_t *ed, span_t v)
{
    double step = ed->grid;

    while (step / ed->zoom < 12.0)
        step *= 2.0;
    for (double x = floor(v.left / step) * step; x <= v.right; x += step)
        line(ed, x, v.top, x, v.bottom,
            fmod(fabs(x), 200.0) == 0.0 ? GRID_STRONG : GRID);
    for (double y = floor(v.top / step) * step; y <= v.bottom; y += step)
        line(ed, v.left, y, v.right, y,
            fmod(fabs(y - GROUND_Y), 200.0) == 0.0 ? GRID_STRONG : GRID);
}

/* The ground, where the level ends, and the flipped player's kill ceiling. */
static void limit_lines(editor_t *ed, span_t v)
{
    if (!ed->measured) {
        ed_level_measure(&ed->lv, &ed->end_x, &ed->kill_y);
        ed->measured = true;
    }
    line(ed, v.left, GROUND_Y, v.right, GROUND_Y, GROUND_LINE);
    line(ed, ed->end_x, v.top, ed->end_x, v.bottom, END_LINE);
    line(ed, v.left, ed->kill_y, v.right, ed->kill_y, KILL_LINE);
}

/* The game's own vertices for each object in view, layer by layer (11.4). */
static void draw_objects(editor_t *ed, gd_t *gd, span_t v)
{
    sfRenderStates rs = {sfBlendAlpha, sfTransform_Identity, gd->atlas, NULL};
    sfVertex six[6];

    for (int l = 0; l < LAYER_COUNT; l++)
        sfVertexArray_clear(ed->objects[l]);
    for (size_t i = 0; i < ed->lv.count; i++) {
        const object_t *o = &ed->lv.objects[i].obj;
        rect_t d = object_drawn_bounds(o);

        if (d.x > v.right || d.x + d.w < v.left || d.y > v.bottom
            || d.y + d.h < v.top)
            continue;
        object_vertices(gd, o, six);
        for (int k = 0; k < 6; k++)
            sfVertexArray_append(ed->objects[object_layer(o)], six[k]);
    }
    for (int l = 0; l < LAYER_COUNT; l++)
        draw_vertex_array(gd, ed->objects[l], &rs);
}

/* Where an attempt begins: the player's own icon, faint. */
static void draw_start(editor_t *ed, gd_t *gd, const level_start_t *st,
    sfColor tint)
{
    sfTexture *tex = st->mode == MODE_SHIP || st->mode == MODE_UFO
        ? gd->res->ship_icon : gd->res->player_icon;
    sfVector2u size = sfTexture_getSize(tex);
    float side = (float)fmax(2.0 * MODES[st->mode].half, 60.0);

    sfSprite_setTexture(ed->marker, tex, sfTrue);
    sfSprite_setOrigin(ed->marker, (sfVector2f){size.x / 2.0f, size.y / 2.0f});
    sfSprite_setScale(ed->marker, (sfVector2f){side / (float)size.x,
        side / (float)size.y * (float)st->gravity_dir});
    sfSprite_setPosition(ed->marker, (sfVector2f){(float)st->pos.x,
        (float)st->pos.y});
    sfSprite_setColor(ed->marker, tint);
    draw_sprite(gd, ed->marker, NULL);
}

static void draw_canvas(editor_t *ed, gd_t *gd)
{
    span_t v = visible(ed);
    vec2_t at = editor_mouse_world(ed, gd);

    if (v.bottom > GROUND_Y)
        fill(gd, ed, (sfFloatRect){(float)v.left, (float)GROUND_Y,
            (float)(v.right - v.left), (float)(v.bottom - GROUND_Y)},
            UNDERGROUND, sfTransparent);
    sfVertexArray_clear(ed->lines);
    grid_lines(ed, v);
    limit_lines(ed, v);
    draw_vertex_array(gd, ed->lines, NULL);
    draw_objects(ed, gd, v);
    draw_start(ed, gd, &ed->lv.hdr.start, (sfColor){255, 255, 255, 150});
    for (size_t i = 0; i < ed->lv.nb_starts; i++)
        draw_start(ed, gd, &ed->lv.starts[i], (sfColor){120, 220, 255, 150});
    if (editor_mouse_on_canvas(gd) && !ed->panning)
        fill(gd, ed, (sfFloatRect){(float)ed_snap(at.x, ed->grid),
            (float)ed_snap(at.y, ed->grid), 100.0f, 100.0f},
            (sfColor){255, 255, 255, 25}, (sfColor){255, 255, 255, 160});
}

static void label(editor_t *ed, gd_t *gd, const char *s, float x, float y,
    unsigned int size, sfColor c)
{
    sfText_setString(ed->text, s);
    sfText_setCharacterSize(ed->text, size);
    sfText_setFillColor(ed->text, c);
    sfText_setPosition(ed->text, (sfVector2f){x, y});
    draw_text(gd, ed->text);
}

/* One row per thing that can be placed; the first nine have a key. */
static void draw_palette(editor_t *ed, gd_t *gd)
{
    char row[48];

    fill(gd, ed, (sfFloatRect){0.0f, ED_TOOLBAR_H, ED_PALETTE_W,
        VIEW_H - ED_TOOLBAR_H}, PANEL_FILL, sfTransparent);
    for (int i = 0; i < ed->nb_entries; i++) {
        float y = ED_TOOLBAR_H + 10.0f + 40.0f * (float)i;

        if (i == ed->entry)
            fill(gd, ed, (sfFloatRect){6.0f, y, ED_PALETTE_W - 12.0f, 36.0f},
                PICKED, sfTransparent);
        if (i < 9)
            snprintf(row, sizeof(row), "%d  %s", i + 1, ed->entries[i].label);
        else
            snprintf(row, sizeof(row), "    %s", ed->entries[i].label);
        label(ed, gd, row, 14.0f, y + 6.0f, 20, sfWhite);
    }
}

static void draw_toolbar(editor_t *ed, gd_t *gd)
{
    char line1[256];

    fill(gd, ed, (sfFloatRect){0.0f, 0.0f, VIEW_W, ED_TOOLBAR_H}, PANEL_FILL,
        sfTransparent);
    snprintf(line1, sizeof(line1),
        "%s%s   -   level %s   -   %zu objects   -   Grid %.0f   -   Zoom %.0f%%",
        ed->lv.hdr.name, ed->lv.dirty ? " *" : "", ed->lv.id, ed->lv.count,
        ed->grid, 100.0f / ed->zoom);
    label(ed, gd, line1, 20.0f, 8.0f, 28, sfWhite);
    label(ed, gd, "Left: place   Right: delete   Wheel: scroll   Ctrl+wheel: "
        "zoom   Middle or Space+drag: pan   G: grid   Ctrl+S: save   Esc: "
        "leave", 20.0f, 48.0f, 18, (sfColor){180, 190, 210, 255});
    if (ui_now_ms() < ed->notice_until_ms)
        label(ed, gd, ed->notice, ED_PALETTE_W + 30.0f, VIEW_H - 60.0f, 30,
            (sfColor){255, 230, 120, 255});
}

void editor_draw(editor_t *ed, gd_t *gd)
{
    sfRenderWindow_setView(gd->w, gd->ui_view);
    fill(gd, ed, (sfFloatRect){ED_PALETTE_W, ED_TOOLBAR_H, ED_CANVAS_W,
        ED_CANVAS_H}, CANVAS, sfTransparent);
    editor_apply_camera(ed, gd);
    sfRenderWindow_setView(gd->w, ed->view);
    draw_canvas(ed, gd);
    sfRenderWindow_setView(gd->w, gd->ui_view);
    draw_palette(ed, gd);
    draw_toolbar(ed, gd);
}
