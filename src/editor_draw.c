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
static const sfColor SELECTED = {90, 255, 140, 255};

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

void editor_fill(gd_t *gd, editor_t *ed, sfFloatRect r, sfColor c,
    sfColor outline)
{
    sfRectangleShape_setPosition(ed->box, (sfVector2f){r.left, r.top});
    sfRectangleShape_setSize(ed->box, (sfVector2f){r.width, r.height});
    sfRectangleShape_setFillColor(ed->box, c);
    sfRectangleShape_setOutlineColor(ed->box, outline);
    sfRectangleShape_setOutlineThickness(ed->box, outline.a > 0 ? 3.0f : 0.0f);
    draw_rect(gd, ed->box);
}

void editor_line(editor_t *ed, double x0, double y0, double x1, double y1,
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
        editor_line(ed, x, v.top, x, v.bottom,
            fmod(fabs(x), 200.0) == 0.0 ? GRID_STRONG : GRID);
    for (double y = floor(v.top / step) * step; y <= v.bottom; y += step)
        editor_line(ed, v.left, y, v.right, y,
            fmod(fabs(y - GROUND_Y), 200.0) == 0.0 ? GRID_STRONG : GRID);
}

/* The ground, where the level ends, and the flipped player's kill ceiling. */
static void limit_lines(editor_t *ed, span_t v)
{
    editor_line(ed, v.left, GROUND_Y, v.right, GROUND_Y, GROUND_LINE);
    editor_line(ed, ed->end_x, v.top, ed->end_x, v.bottom, END_LINE);
    editor_line(ed, v.left, ed->kill_y, v.right, ed->kill_y, KILL_LINE);
}

/* The objects in view, of any layer: their indices, in the list's order. */
static size_t in_view(editor_t *ed, span_t v)
{
    size_t n = 0;

    if (ed->drawn_cap < ed->lv.count + 1) {
        free(ed->drawn);
        ed->drawn_cap = (ed->lv.count + 1) * 2;
        ed->drawn = sim_xcalloc(ed->drawn_cap, sizeof(size_t));
    }
    for (size_t i = 0; i < ed->lv.count; i++) {
        rect_t d = object_drawn_bounds(&ed->lv.objects[i].obj);

        if (ed->lv.objects[i].is_start || d.x > v.right
            || d.x + d.w < v.left || d.y > v.bottom || d.y + d.h < v.top)
            continue;
        ed->drawn[n++] = i;
    }
    return n;
}

/*
** The game's own vertices for each object in view, in the game's order:
** Z layer, Z order, then the later placed on top (11.15). Those of other
** editor layers than the one worked in are faint.
*/
static void draw_objects(editor_t *ed, gd_t *gd, span_t v)
{
    sfRenderStates rs = {sfBlendAlpha, sfTransform_Identity, gd->atlas, NULL};
    size_t n = in_view(ed, v);
    sfVertex six[6];

    ed_sort_drawn(&ed->lv, ed->drawn, n);
    sfVertexArray_clear(ed->objects);
    for (size_t k = 0; k < n; k++) {
        const ed_object_t *o = &ed->lv.objects[ed->drawn[k]];
        bool faint = !ed_in_layer(&ed->lv, o);

        object_vertices(gd, &o->obj, six);
        for (int j = 0; j < 6; j++) {
            six[j].color.a = faint ? six[j].color.a / 5 : six[j].color.a;
            sfVertexArray_append(ed->objects, six[j]);
        }
    }
    draw_vertex_array(gd, ed->objects, &rs);
}

/* A selected object's own rect, turned as it is, in lines over everything. */
static void outline(editor_t *ed, const object_t *o)
{
    vec2_t c = {o->rect.x + o->rect.w / 2.0, o->rect.y + o->rect.h / 2.0};
    vec2_t p[4] = {{o->rect.x, o->rect.y}, {o->rect.x + o->rect.w, o->rect.y},
        {o->rect.x + o->rect.w, o->rect.y + o->rect.h},
        {o->rect.x, o->rect.y + o->rect.h}};
    double rad = o->rotation * M_PI / 180.0;

    for (int i = 0; i < 4; i++)
        p[i] = (vec2_t){c.x + (p[i].x - c.x) * cos(rad) - (p[i].y - c.y)
            * sin(rad), c.y + (p[i].x - c.x) * sin(rad) + (p[i].y - c.y)
            * cos(rad)};
    for (int i = 0; i < 4; i++)
        editor_line(ed, p[i].x, p[i].y, p[(i + 1) % 4].x, p[(i + 1) % 4].y, SELECTED);
}

/* Its shape again, plain green and see-through, so it shows at any zoom. */
static void tint(editor_t *ed, gd_t *gd, const object_t *o)
{
    sfVertex six[6];

    object_vertices(gd, o, six);
    for (int k = 0; k < 6; k++) {
        six[k].color = (sfColor){90, 255, 140, 90};
        sfVertexArray_append(ed->objects, six[k]);
    }
}

static void draw_selection(editor_t *ed, gd_t *gd)
{
    rect_t box = editor_box(ed, gd);

    sfVertexArray_clear(ed->lines);
    sfVertexArray_clear(ed->objects);
    for (size_t i = 0; i < ed->lv.count; i++)
        if (ed->lv.objects[i].selected) {
            tint(ed, gd, &ed->lv.objects[i].obj);
            outline(ed, &ed->lv.objects[i].obj);
        }
    draw_vertex_array(gd, ed->objects, NULL);
    draw_vertex_array(gd, ed->lines, NULL);
    if (ed->mouse == ED_BOXING)
        editor_fill(gd, ed, (sfFloatRect){(float)box.x, (float)box.y, (float)box.w,
            (float)box.h}, (sfColor){90, 255, 140, 30}, SELECTED);
}

/* Where an attempt begins: the player's own icon, faint. */
static void draw_start(editor_t *ed, gd_t *gd, const level_start_t *st,
    sfColor tint)
{
    sfTexture *tex = icon_texture(gd, st->mode);
    sfVector2u size = sfTexture_getSize(tex);
    const mode_ops_t *m = st->mini ? &MINI_MODES[st->mode] : &MODES[st->mode];
    float side = (float)fmax(2.0 * m->half, st->mini ? 30.0 : 60.0);

    sfSprite_setTexture(ed->marker, tex, sfTrue);
    sfSprite_setOrigin(ed->marker, (sfVector2f){size.x / 2.0f, size.y / 2.0f});
    sfSprite_setScale(ed->marker, (sfVector2f){side / (float)size.x,
        side / (float)size.y * (float)st->gravity_dir});
    sfSprite_setPosition(ed->marker, (sfVector2f){(float)st->pos.x,
        (float)st->pos.y});
    sfSprite_setColor(ed->marker, tint);
    draw_sprite(gd, ed->marker, NULL);
}

/* A start position: the icon it starts as, in a blue frame, so it is never
** taken for the real start or for a block. */
static void draw_start_object(editor_t *ed, gd_t *gd, const ed_object_t *o)
{
    rect_t r = o->obj.rect;

    draw_start(ed, gd, &o->start, (sfColor){255, 255, 255, 200});
    editor_fill(gd, ed, (sfFloatRect){(float)r.x + 3.0f, (float)r.y + 3.0f,
        (float)r.w - 6.0f, (float)r.h - 6.0f}, (sfColor){80, 170, 255, 40},
        (sfColor){80, 170, 255, 255});
}

/*
** Where the last playtest went, until the level is edited: where the jumps
** were and how close the hazards came (11.9).
*/
static void draw_path(editor_t *ed, gd_t *gd)
{
    static const sfColor PATH = {255, 230, 120, 255};
    static const sfColor BOT_PATH = {110, 220, 255, 255};
    static const sfColor STOPPED = {255, 70, 70, 255};
    vec2_t end;

    if (ed->trace_len < 2 || ed->trace_edits != ed->hist.edits)
        return;
    sfVertexArray_clear(ed->lines);
    for (size_t i = 1; i < ed->trace_len; i++)
        editor_line(ed, ed->trace[i - 1].x, ed->trace[i - 1].y, ed->trace[i].x,
            ed->trace[i].y, ed->trace_bot ? BOT_PATH : PATH);
    end = ed->trace[ed->trace_len - 1];
    if (ed->trace_stopped) {                 /* the bot got no further (11.10) */
        editor_line(ed, end.x - 45.0, end.y - 45.0, end.x + 45.0, end.y + 45.0,
            STOPPED);
        editor_line(ed, end.x - 45.0, end.y + 45.0, end.x + 45.0, end.y - 45.0,
            STOPPED);
        editor_line(ed, end.x, end.y - 400.0, end.x, end.y + 400.0, STOPPED);
    }
    draw_vertex_array(gd, ed->lines, NULL);
}

static void draw_canvas(editor_t *ed, gd_t *gd)
{
    span_t v = visible(ed);
    vec2_t at = editor_mouse_world(ed, gd);

    editor_measure(ed, gd);
    if (v.bottom > GROUND_Y)
        editor_fill(gd, ed, (sfFloatRect){(float)v.left, (float)GROUND_Y,
            (float)(v.right - v.left), (float)(v.bottom - GROUND_Y)},
            UNDERGROUND, sfTransparent);
    sfVertexArray_clear(ed->lines);
    grid_lines(ed, v);
    limit_lines(ed, v);
    editor_song_lines(ed, gd, v.left, v.right, v.top, v.bottom);
    draw_vertex_array(gd, ed->lines, NULL);
    draw_objects(ed, gd, v);
    draw_start(ed, gd, &ed->lv.hdr.start, (sfColor){255, 255, 255, 150});
    for (size_t i = 0; i < ed->lv.count; i++)
        if (ed->lv.objects[i].is_start)
            draw_start_object(ed, gd, &ed->lv.objects[i]);
    draw_path(ed, gd);
    draw_selection(ed, gd);
    if (editor_mouse_on_canvas(gd) && ed->tool == ED_TOOL_PLACE
        && ed->mouse != ED_PANNING)
        editor_fill(gd, ed, (sfFloatRect){(float)ed_snap(at.x, ed->grid),
            (float)ed_snap(at.y, ed->grid), 100.0f, 100.0f},
            (sfColor){255, 255, 255, 25}, (sfColor){255, 255, 255, 160});
}

/* Names are UTF-8: a level called "Café" shows as it is typed (7.2). */
float editor_label_width(editor_t *ed, const char *s, unsigned int size)
{
    sfFloatRect b;

    sfText_setUnicodeString(ed->text, utf8_to_utf32(s));
    sfText_setCharacterSize(ed->text, size);
    b = sfText_getLocalBounds(ed->text);
    return b.left + b.width;
}

void editor_label(editor_t *ed, gd_t *gd, const char *s, float x, float y,
    unsigned int size, sfColor c)
{
    sfText_setUnicodeString(ed->text, utf8_to_utf32(s));
    sfText_setCharacterSize(ed->text, size);
    sfText_setFillColor(ed->text, c);
    sfText_setPosition(ed->text, (sfVector2f){x, y});
    draw_text(gd, ed->text);
}

/* Cut, a whole character at a time, and "..." put after: it fits in w. */
void editor_label_fit(editor_t *ed, gd_t *gd, const char *s, sfVector2f at,
    float w, unsigned int size, sfColor c)
{
    char buf[400];
    size_t n = 0;

    snprintf(buf, sizeof(buf), "%s", s);
    n = strlen(buf);
    while (n > 0 && editor_label_width(ed, buf, size) > w) {
        do
            n -= 1;
        while (n > 0 && ((unsigned char)buf[n] & 0xC0) == 0x80);
        snprintf(buf + n, sizeof(buf) - n, "%s", "...");
    }
    editor_label(ed, gd, buf, at.x, at.y, size, c);
}

/* A digit key reads as its digit: "1", not "Num1". */
static const char *palette_key(gd_t *gd, int i, char *buf, size_t size)
{
    binding_t b = gd->settings.editor_keys[ED_KEY_ENTRY_1 + i];

    if (b.kind == BIND_KEY && b.mods == 0 && b.code >= KEY_Num0
        && b.code <= KEY_Num9)
        snprintf(buf, size, "%d", b.code - KEY_Num0);
    else
        editor_key_name(gd, (ed_key_t)(ED_KEY_ENTRY_1 + i), buf, size);
    return buf;
}

/* One row per thing that can be placed; the first nine have a key. */
static void draw_palette(editor_t *ed, gd_t *gd)
{
    char row[96];
    char key[BINDING_NAME_MAX];

    editor_fill(gd, ed, (sfFloatRect){0.0f, ED_TOOLBAR_H, ED_PALETTE_W,
        VIEW_H - ED_TOOLBAR_H}, PANEL_FILL, sfTransparent);
    for (int i = 0; i < ed->nb_entries; i++) {
        float y = ED_TOOLBAR_H + 10.0f + ED_PALETTE_ROW * (float)i;

        if (i == ed->entry)
            editor_fill(gd, ed, (sfFloatRect){6.0f, y, ED_PALETTE_W - 12.0f,
                ED_PALETTE_ROW - 3.0f}, PICKED, sfTransparent);
        if (i < 9)
            snprintf(row, sizeof(row), "%s  %s", palette_key(gd, i, key,
                sizeof(key)), ed->entries[i].label);
        else
            snprintf(row, sizeof(row), "    %s", ed->entries[i].label);
        editor_label_fit(ed, gd, row, (sfVector2f){14.0f, y + 3.0f},
            ED_PALETTE_W - 20.0f, 19, sfWhite);
    }
}

/* The tool's mouse, and the keys of what is used most: the rest on F1. */
static void help_line(editor_t *ed, gd_t *gd, char *out, size_t size)
{
    char k[6][BINDING_NAME_MAX];

    editor_key_name(gd, ED_KEY_TOOL, k[0], sizeof(k[0]));
    editor_key_name(gd, ED_KEY_KEYS, k[1], sizeof(k[1]));
    editor_key_name(gd, ED_KEY_UNDO, k[2], sizeof(k[2]));
    editor_key_name(gd, ED_KEY_PLAYTEST, k[3], sizeof(k[3]));
    editor_key_name(gd, ED_KEY_SAVE, k[4], sizeof(k[4]));
    editor_key_name(gd, ED_KEY_DELETE, k[5], sizeof(k[5]));
    if (ed->tool == ED_TOOL_PLACE)
        snprintf(out, size, "PLACE (%s: select)   Left: place   Right: "
            "delete   Wheel: scroll   %s: undo   %s: play   %s: save   "
            "%s: all the keys", k[0], k[2], k[3], k[4], k[1]);
    else
        snprintf(out, size, "SELECT (%s: place)   Click, Shift+click, box   "
            "Drag: move   %s: delete   %s: undo   %s: play   %s: save   "
            "%s: all the keys", k[0], k[5], k[2], k[3], k[4], k[1]);
}

static void draw_toolbar(editor_t *ed, gd_t *gd)
{
    char text[320];
    char layer[16] = "All";

    if (ed->lv.layer != ED_LAYER_ALL)
        snprintf(layer, sizeof(layer), "%d", ed->lv.layer);
    editor_fill(gd, ed, (sfFloatRect){0.0f, 0.0f, VIEW_W, ED_TOOLBAR_H}, PANEL_FILL,
        sfTransparent);
    snprintf(text, sizeof(text), "%s%s   -   level %s   -   %zu objects, "
        "%zu selected   -   Grid %.0f   -   Zoom %.0f%%   -   Layer %s",
        ed->lv.hdr.name, ed->lv.dirty ? " *" : "", ed->lv.id, ed->lv.count,
        editor_props(ed)->selected, ed->grid, 100.0f / ed->zoom, layer);
    editor_label_fit(ed, gd, text, (sfVector2f){20.0f, 8.0f}, VIEW_W - 40.0f,
        28, sfWhite);
    help_line(ed, gd, text, sizeof(text));
    editor_label_fit(ed, gd, text, (sfVector2f){20.0f, 48.0f}, VIEW_W - 40.0f,
        18, (sfColor){180, 190, 210, 255});
    if (ui_now_ms() < ed->notice_until_ms)
        editor_label(ed, gd, ed->notice, ED_PALETTE_W + 30.0f,
            ED_STATUS_Y - 60.0f, 30,
            (sfColor){255, 230, 120, 255});
}

void editor_draw(editor_t *ed, gd_t *gd)
{
    if (ed->play != NULL)
        return level_frame(gd, ed->play);    /* its ticks, then its picture */
    sfRenderWindow_setView(gd->w, gd->ui_view);
    editor_fill(gd, ed, (sfFloatRect){ED_PALETTE_W, ED_TOOLBAR_H, ED_CANVAS_W,
        ED_CANVAS_H}, CANVAS, sfTransparent);
    editor_apply_camera(ed, gd);
    sfRenderWindow_setView(gd->w, ed->view);
    draw_canvas(ed, gd);
    sfRenderWindow_setView(gd->w, gd->ui_view);
    draw_palette(ed, gd);
    draw_toolbar(ed, gd);
    editor_status_draw(ed, gd);
    editor_panel_draw(ed, gd);
    editor_keys_draw(ed, gd);
    editor_dialog_draw(ed, gd);
}
