/*
** ALEXNEX PROJECT, 2026
** debug_overlay
** File description:
** F3: the player's shapes, the hitboxes and what the last tick did (9.6)
*/

#include "mygd.h"
#include "sim/modes.h"

#define SHAPE_SQUARE (sfColor){255, 60, 60, 255}      /* the rigid square */
#define SHAPE_CIRCLE (sfColor){255, 160, 0, 255}      /* the circle       */
#define SHAPE_INNER  (sfColor){80, 140, 255, 255}     /* the inner box    */
#define ZONE_ON      (sfColor){255, 255, 255, 200}
#define ZONE_OFF     (sfColor){255, 255, 255, 70}
#define NEUTRAL      (sfColor){60, 120, 255, 255}
#define HARM         (sfColor){255, 40, 40, 255}
#define INTERACTIVE  (sfColor){255, 230, 40, 255}
#define SPENT        (sfColor){140, 140, 140, 255}
#define BOUNDS       (sfColor){0, 220, 220, 255}
#define CIRCLE_STEPS 24

static void line(sfVertexArray *va, vec2_t a, vec2_t b, sfColor c)
{
    sfVertex v = {{(float)a.x, (float)a.y}, c, {0.0f, 0.0f}};

    sfVertexArray_append(va, v);
    v.position = (sfVector2f){(float)b.x, (float)b.y};
    sfVertexArray_append(va, v);
}

static void box(sfVertexArray *va, vec2_t c, double half, sfColor col)
{
    vec2_t p[4] = {{c.x - half, c.y - half}, {c.x + half, c.y - half},
        {c.x + half, c.y + half}, {c.x - half, c.y + half}};

    for (int i = 0; i < 4; i++)
        line(va, p[i], p[(i + 1) % 4], col);
}

static void circle(sfVertexArray *va, vec2_t c, double r, sfColor col)
{
    double step = 2.0 * M_PI / CIRCLE_STEPS;

    for (int i = 0; i < CIRCLE_STEPS; i++)
        line(va, (vec2_t){c.x + r * cos(i * step), c.y + r * sin(i * step)},
            (vec2_t){c.x + r * cos((i + 1) * step),
                c.y + r * sin((i + 1) * step)}, col);
}

static void rect_outline(sfVertexArray *va, rect_t r, sfColor col)
{
    vec2_t p[4] = {{r.x, r.y}, {r.x + r.w, r.y},
        {r.x + r.w, r.y + r.h}, {r.x, r.y + r.h}};

    for (int i = 0; i < 4; i++)
        line(va, p[i], p[(i + 1) % 4], col);
}

/* The three shapes of 4.3, each in its own colour, all centred on pos. */
static void draw_player_shapes(sfVertexArray *va, const level_t *lv)
{
    const player_t *p = &lv->sim.st.player;
    const mode_ops_t *m = &MODES[p->mode];
    rect_t zone;
    double y_line = 0.0;

    box(va, p->pos, m->half, SHAPE_SQUARE);
    circle(va, p->pos, m->half, SHAPE_CIRCLE);
    box(va, p->pos, m->inner_half, SHAPE_INNER);
    sim_jump_zone(&lv->sim, &zone, &y_line);
    rect_outline(va, zone, p->can_jump ? ZONE_ON : ZONE_OFF);
    line(va, (vec2_t){zone.x, y_line}, (vec2_t){zone.x + zone.w, y_line},
        p->can_jump ? ZONE_ON : ZONE_OFF);   /* where the circle rule stops */
}

static sfColor object_color(const level_t *lv, size_t i)
{
    const object_t *o = &lv->sim.lvl.objects[i];

    if (OBJ_CATEGORY[o->type] == CAT_HARM)
        return HARM;
    if (OBJ_CATEGORY[o->type] != CAT_INTERACTIVE)
        return NEUTRAL;
    return is_spent(&lv->sim.st, i) ? SPENT : INTERACTIVE;
}

/* Through the vertices, not the AABB: a rotated shape is not its box (4.2). */
static void draw_hitboxes(sfVertexArray *va, const level_t *lv, float cam_x)
{
    const level_data_t *lvl = &lv->sim.lvl;

    for (size_t i = 0; i < lvl->nb_objects; i++) {
        const hitbox_t *h = &lvl->objects[i].hitbox;
        sfColor col = object_color(lv, i);

        if (h->aabb.x > cam_x + VIEW_W)
            break;
        if (h->aabb.x + h->aabb.w < cam_x)
            continue;
        for (int v = 0; v < h->nverts; v++)
            line(va, h->verts[v], h->verts[(v + 1) % h->nverts], col);
    }
}

static void draw_bounds(sfVertexArray *va, const level_t *lv, float cam_x)
{
    const ship_bounds_t *b = &lv->sim.st.bounds;
    double x0 = cam_x;
    double x1 = cam_x + VIEW_W;

    line(va, (vec2_t){x0, lv->sim.lvl.kill_y},
        (vec2_t){x1, lv->sim.lvl.kill_y}, HARM);
    if (!b->active)
        return;
    line(va, (vec2_t){x0, b->top}, (vec2_t){x1, b->top}, BOUNDS);
    line(va, (vec2_t){x0, b->bottom}, (vec2_t){x1, b->bottom}, BOUNDS);
}

static sfColor event_color(debug_kind_t kind)
{
    if (kind == DBG_LAND)
        return sfWhite;
    if (kind == DBG_HEAD)
        return (sfColor){255, 160, 0, 255};
    if (kind == DBG_PASS)
        return (sfColor){160, 160, 160, 255};
    if (kind == DBG_STEP)
        return (sfColor){60, 255, 60, 255};
    return HARM;
}

/* What the last tick did, as arrows along the contact normals (9.6). */
static void draw_tick_events(sfVertexArray *va, const level_t *lv)
{
    for (int i = 0; i < lv->sim.nb_dbg; i++) {
        const debug_event_t *e = &lv->sim.dbg[i];
        vec2_t tip = {e->pos.x + e->normal.x * 70.0,
            e->pos.y + e->normal.y * 70.0};

        if (e->normal.x == 0.0 && e->normal.y == 0.0)
            tip = (vec2_t){e->pos.x, e->pos.y - 70.0};
        line(va, e->pos, tip, event_color(e->kind));
    }
}

static void draw_numbers(gd_t *gd, level_t *lv, float ms)
{
    const player_t *p = &lv->sim.st.player;
    char text[512];

    snprintf(text, sizeof(text),
        "tick %ld  x %.2f  y %.2f\nvx %.4f  vy %.4f  rise %.4f\n"
        "grounded %d  can_jump %d  mode %s\n%.2f%%  %.2f ms",
        lv->sim.st.tick, p->pos.x, p->pos.y, p->vx, p->vy, p->surface_rise,
        p->grounded, p->can_jump, MODES[p->mode].name, sim_percent(&lv->sim),
        ms);
    sfText_setString(lv->hud_text, text);
    sfText_setCharacterSize(lv->hud_text, 26);
    sfText_setPosition(lv->hud_text, (sfVector2f){40.0f, 120.0f});
    sfRenderWindow_drawText(gd->w, lv->hud_text, NULL);
    sfText_setCharacterSize(lv->hud_text, 40);
}

void render_debug_overlay(gd_t *gd, level_t *lv, vec2_t cam)
{
    static sfVertexArray *va = NULL;
    static sfClock *frame = NULL;
    float ms = 0.0f;

    if (va == NULL) {
        va = sfVertexArray_create();
        frame = sfClock_create();
    }
    ms = sfClock_restart(frame).microseconds / 1000.0f;
    sfVertexArray_clear(va);
    sfVertexArray_setPrimitiveType(va, sfLines);
    draw_hitboxes(va, lv, (float)cam.x);
    draw_bounds(va, lv, (float)cam.x);
    draw_player_shapes(va, lv);
    draw_tick_events(va, lv);
    sfRenderWindow_drawVertexArray(gd->w, va, NULL);
    sfRenderWindow_setView(gd->w, gd->ui_view);
    draw_numbers(gd, lv, ms);
    sfRenderWindow_setView(gd->w, gd->level_view);
}
