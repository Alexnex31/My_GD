/*
** ALEXNEX PROJECT, 2026
** level_chunks
** File description:
** the level's objects: their vertices and draw order, built once (9.2, 11.15)
*/

#include <string.h>

#include "mygd.h"
#include "sim/hitbox.h"
#include "sim/modes.h"
#include "sim/speed.h"

#define VERTS_PER_OBJECT 6        /* two triangles; a slope's second is empty */
#define SAW_SPIN 1.5              /* degrees per tick: one turn a second    */

/* What turns on screen is drawn from its turned self each frame. */
static bool spins(const object_t *o)
{
    return o->type == OBJ_SAW;
}

/* One row per mode, like MODES[] (FEATURES 6.1): a new mode adds its own. */
static const sfColor PORTAL_COLORS[MODE_COUNT] = {
    [MODE_CUBE] = {0, 230, 230, 255},
    [MODE_SHIP] = {255, 120, 220, 255},
    [MODE_UFO] = {255, 150, 40, 255},
    [MODE_WAVE] = {110, 80, 255, 255},
    [MODE_BALL] = {255, 60, 60, 255},
    [MODE_ROBOT] = {235, 235, 235, 255},
    [MODE_SWING] = {255, 220, 60, 255},
    [MODE_SPIDER] = {170, 40, 200, 255},
};

/* Pads and orbs wear their own colour, GD's (FEATURES 10.1, 10.2). */
static const sfColor LAUNCH_COLORS[LAUNCH_KIND_COUNT] = {
    [LAUNCH_YELLOW] = {255, 220, 40, 255},
    [LAUNCH_PINK] = {255, 120, 220, 255},
    [LAUNCH_RED] = {255, 60, 60, 255},
    [LAUNCH_BLUE] = {60, 120, 255, 255},
    [LAUNCH_GREEN] = {60, 220, 90, 255},
    [LAUNCH_BLACK] = {70, 70, 70, 255},
};

/* A speed portal's colour, slowest first: GD's (FEATURES 10.3). */
static sfColor speed_color(double name)
{
    static const sfColor COLORS[SPEED_COUNT] = {{255, 150, 40, 255},
        {60, 150, 255, 255}, {60, 220, 90, 255}, {255, 120, 220, 255},
        {255, 60, 60, 255}};

    for (int i = 0; i < SPEED_COUNT; i++)
        if (SPEED_NAMES[i] == name)
            return COLORS[i];
    return sfWhite;
}

/*
** A mode portal has its mode's colour; a gravity portal is yellow when it
** sends the player up and blue when it brings it back down, and a size
** portal pink when it makes it mini and green when it gives its size back,
** as in GD.
*/
static sfColor object_color(const object_t *o)
{
    if (o->type == OBJ_GRAVITY)
        return o->portal_gravity < 0 ? (sfColor){255, 220, 40, 255}
            : (sfColor){60, 120, 255, 255};
    if (o->type == OBJ_PAD || o->type == OBJ_ORB)
        return LAUNCH_COLORS[o->launch];
    if (o->type == OBJ_SPEED)
        return speed_color(o->speed);
    if (o->type == OBJ_MINI)
        return o->portal_mini ? (sfColor){255, 90, 200, 255}
            : (sfColor){80, 230, 90, 255};
    if (o->type != OBJ_PORTAL)
        return sfWhite;
    return PORTAL_COLORS[o->portal_mode];
}

/*
** What an object is drawn in: its rect, except a pad and an orb, which are
** drawn as their hitbox, the plate and the small square (4.2). Still turned
** around the rect's center, so a pad at rot=180 hangs from its cell's top.
*/
static rect_t drawn_rect(const object_t *o)
{
    if (o->type == OBJ_PAD || o->type == OBJ_ORB)
        return object_local_box(o);
    return o->rect;
}

/*
** Two triangles for one object, its own rect rotated, from the atlas (9.2).
** A slope is its hitbox's triangle (bottom left, top right, bottom right,
** 4.2), the texture cut along the same diagonal; its second triangle is a
** single point, which draws nothing and keeps every object 6 vertices.
*/
static void append_object(sfVertex *v, const object_t *o, sfFloatRect tex,
    sfColor col)
{
    static const int quad[6] = {0, 1, 2, 0, 2, 3};
    static const int slope[6] = {3, 1, 2, 2, 2, 2};
    const int *order = o->type == OBJ_SLOPE ? slope : quad;
    vec2_t c = {o->rect.x + o->rect.w / 2.0, o->rect.y + o->rect.h / 2.0};
    rect_t r = drawn_rect(o);
    vec2_t p[4] = {{r.x, r.y}, {r.x + r.w, r.y}, {r.x + r.w, r.y + r.h},
        {r.x, r.y + r.h}};
    sfVector2f t[4] = {{tex.left, tex.top}, {tex.left + tex.width, tex.top},
        {tex.left + tex.width, tex.top + tex.height},
        {tex.left, tex.top + tex.height}};
    double rad = o->rotation * M_PI / 180.0;
    double cs = cos(rad);
    double sn = sin(rad);

    for (int i = 0; i < 4; i++)
        p[i] = (vec2_t){c.x + (p[i].x - c.x) * cs - (p[i].y - c.y) * sn,
            c.y + (p[i].x - c.x) * sn + (p[i].y - c.y) * cs};
    for (int k = 0; k < 6; k++)
        v[k] = (sfVertex){{(float)p[order[k]].x, (float)p[order[k]].y}, col,
            t[order[k]]};
}

/* The same six vertices, for what draws objects that still change: the
** editor's canvas shows exactly what the game will (FEATURES 11.4). */
void object_vertices(gd_t *gd, const object_t *o, sfVertex *v)
{
    append_object(v, o, gd->atlas_rect[o->type], object_color(o));
}

static const object_t *g_sorting;

static int by_draw_order(const void *a, const void *b)
{
    const object_t *oa = &g_sorting[*(const size_t *)a];
    const object_t *ob = &g_sorting[*(const size_t *)b];

    return object_draw_cmp(oa, oa->line, ob, ob->line);
}

static int by_rank(const void *a, const void *b)
{
    size_t ra = *(const size_t *)a;
    size_t rb = *(const size_t *)b;

    return (ra > rb) - (ra < rb);
}

static size_t chunk_index(const object_t *o)
{
    rect_t d = object_drawn_bounds(o);
    double x = d.x < 0.0 ? 0.0 : d.x;

    return (size_t)(x / CHUNK_W);
}

/* Each chunk sized first, so its list is allocated once; filled by rank,
** so it is already in the draw order. */
static void build_chunks(draw_order_t *d, const level_data_t *lvl)
{
    size_t *filled = sim_xcalloc(d->nb_chunks, sizeof(size_t));

    for (size_t i = 0; i < lvl->nb_objects; i++)
        d->chunks[chunk_index(&lvl->objects[i])].count += 1;
    for (size_t c = 0; c < d->nb_chunks; c++)
        d->chunks[c].ranks = sim_xcalloc(d->chunks[c].count + 1,
            sizeof(size_t));
    for (size_t k = 0; k < lvl->nb_objects; k++) {
        const object_t *o = &lvl->objects[d->by_rank[k]];
        render_chunk_t *ch = &d->chunks[chunk_index(o)];
        rect_t b = object_drawn_bounds(o);

        ch->ranks[filled[ch - d->chunks]++] = k;
        ch->left = fminf(ch->left, (float)b.x);
        ch->right = fmaxf(ch->right, (float)(b.x + b.w));
    }
    free(filled);
}

/*
** Objects never move (Principle 1): their vertices are computed once per
** level, and their place in the draw order too. A frame only copies those
** of the objects in view, in that order.
*/
void level_build_chunks(level_t *lv, gd_t *gd)
{
    const level_data_t *lvl = &lv->sim.lvl;
    draw_order_t *d = &lv->draw;
    double right = 0.0;

    *d = (draw_order_t){0};
    d->by_rank = sim_xcalloc(lvl->nb_objects + 1, sizeof(size_t));
    d->verts = sim_xcalloc(lvl->nb_objects * VERTS_PER_OBJECT + 1,
        sizeof(sfVertex));
    d->visible = sim_xcalloc(lvl->nb_objects + 1, sizeof(size_t));
    for (size_t i = 0; i < lvl->nb_objects; i++) {
        const object_t *o = &lvl->objects[i];
        rect_t b = object_drawn_bounds(o);

        d->by_rank[i] = i;
        append_object(d->verts + i * VERTS_PER_OBJECT, o,
            gd->atlas_rect[o->type], object_color(o));
        right = fmax(right, b.x + b.w);
    }
    g_sorting = lvl->objects;
    qsort(d->by_rank, lvl->nb_objects, sizeof(size_t), by_draw_order);
    d->nb_chunks = (size_t)(right / CHUNK_W) + 1;
    d->chunks = sim_xcalloc(d->nb_chunks, sizeof(render_chunk_t));
    for (size_t c = 0; c < d->nb_chunks; c++)
        d->chunks[c] = (render_chunk_t){INFINITY, -INFINITY, NULL, 0};
    build_chunks(d, lvl);
}

void level_free_chunks(level_t *lv)
{
    draw_order_t *d = &lv->draw;

    for (size_t c = 0; c < d->nb_chunks; c++)
        free(d->chunks[c].ranks);
    free(d->chunks);
    free(d->by_rank);
    free(d->verts);
    free(d->visible);
    free(d->frame);
    *d = (draw_order_t){0};
}

/* The objects of the chunks in view, in the draw order. */
static void gather(level_t *lv, float cam_x)
{
    draw_order_t *d = &lv->draw;
    const object_t *objs = lv->sim.lvl.objects;

    d->nb_visible = 0;
    for (size_t c = 0; c < d->nb_chunks; c++) {
        const render_chunk_t *ch = &d->chunks[c];

        if (ch->right < cam_x || ch->left > cam_x + VIEW_W)
            continue;
        memcpy(d->visible + d->nb_visible, ch->ranks,
            ch->count * sizeof(size_t));
        d->nb_visible += ch->count;
    }
    qsort(d->visible, d->nb_visible, sizeof(size_t), by_rank);
    d->nb_behind = 0;
    while (d->nb_behind < d->nb_visible
        && objs[d->by_rank[d->visible[d->nb_behind]]].z_layer < Z_T1)
        d->nb_behind += 1;
}

/*
** A saw turns by how long the attempt has run. Decoration only: the sim's
** disc doesn't turn, and never reads this (FEATURES 10.6).
*/
static void put_one(level_t *lv, gd_t *gd, size_t index, sfVertex *out)
{
    object_t o = lv->sim.lvl.objects[index];

    if (!spins(&o)) {
        memcpy(out, lv->draw.verts + index * VERTS_PER_OBJECT,
            VERTS_PER_OBJECT * sizeof(sfVertex));
        return;
    }
    o.rotation += fmod((double)lv->sim.st.tick * SAW_SPIN, 360.0);
    append_object(out, &o, gd->atlas_rect[o.type], object_color(&o));
}

/*
** Behind the player: finds what is in view first. In front: what the
** behind call found, so it comes after it in the same frame.
*/
void render_objects(gd_t *gd, level_t *lv, float cam_x, bool front)
{
    sfRenderStates rs = {sfBlendAlpha, sfTransform_Identity, gd->atlas, NULL};
    draw_order_t *d = &lv->draw;
    size_t from = front ? d->nb_behind : 0;
    size_t to;

    if (!front)
        gather(lv, cam_x);
    to = front ? d->nb_visible : d->nb_behind;
    if (to == from)
        return;
    if ((to - from) * VERTS_PER_OBJECT > d->frame_cap) {
        free(d->frame);
        d->frame_cap = (to - from) * VERTS_PER_OBJECT * 2;
        d->frame = sim_xcalloc(d->frame_cap, sizeof(sfVertex));
    }
    for (size_t k = from; k < to; k++)
        put_one(lv, gd, d->by_rank[d->visible[k]],
            d->frame + (k - from) * VERTS_PER_OBJECT);
    draw_triangles(gd, d->frame, (to - from) * VERTS_PER_OBJECT, &rs);
}
