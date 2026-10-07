/*
** ALEXNEX PROJECT, 2026
** level_chunks
** File description:
** the level's static geometry: chunks of vertices, built once (9.0, 9.2)
*/

#include "mygd.h"
#include "sim/hitbox.h"
#include "sim/modes.h"

#define VERTS_PER_OBJECT 6        /* two triangles; a slope's second is empty */

static draw_layer_t layer_of(const object_t *o)
{
    if (OBJ_CATEGORY[o->type] == CAT_HARM)
        return LAYER_HAZARD;
    if (OBJ_CATEGORY[o->type] == CAT_INTERACTIVE)
        return LAYER_INTERACTIVE;
    return LAYER_BLOCK;
}

/* One row per mode, like MODES[] (FEATURES 6.1): a new mode adds its own. */
static const sfColor PORTAL_COLORS[MODE_COUNT] = {
    [MODE_CUBE] = {0, 230, 230, 255},
    [MODE_SHIP] = {255, 120, 220, 255},
    [MODE_UFO] = {255, 150, 40, 255},
    [MODE_WAVE] = {110, 80, 255, 255},
    [MODE_BALL] = {255, 60, 60, 255},
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

/*
** A mode portal has its mode's colour; a gravity portal is yellow when it
** sends the player up and blue when it brings it back down, as in GD.
*/
static sfColor object_color(const object_t *o)
{
    if (o->type == OBJ_GRAVITY)
        return o->portal_gravity < 0 ? (sfColor){255, 220, 40, 255}
            : (sfColor){60, 120, 255, 255};
    if (o->type == OBJ_PAD || o->type == OBJ_ORB)
        return LAUNCH_COLORS[o->launch];
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

int object_layer(const object_t *o)
{
    return (int)layer_of(o);
}

static size_t chunk_index(const object_t *o)
{
    rect_t d = object_drawn_bounds(o);
    double x = d.x < 0.0 ? 0.0 : d.x;

    return (size_t)(x / CHUNK_W);
}

/* One pass to size every chunk's layers, so each is allocated exactly once. */
static void count_objects(level_t *lv, size_t *counts)
{
    for (size_t i = 0; i < lv->sim.lvl.nb_objects; i++) {
        const object_t *o = &lv->sim.lvl.objects[i];

        counts[chunk_index(o) * LAYER_COUNT + layer_of(o)] += 1;
    }
}

static void fill_chunk_bounds(render_chunk_t *ch, const object_t *o)
{
    rect_t d = object_drawn_bounds(o);

    ch->left = fminf(ch->left, (float)d.x);
    ch->right = fmaxf(ch->right, (float)(d.x + d.w));
}

static void build_vertices(level_t *lv, gd_t *gd, size_t *counts,
    sfVertex **buf)
{
    size_t *filled = sim_xcalloc(lv->nb_chunks * LAYER_COUNT, sizeof(size_t));

    for (size_t i = 0; i < lv->sim.lvl.nb_objects; i++) {
        const object_t *o = &lv->sim.lvl.objects[i];
        size_t k = chunk_index(o) * LAYER_COUNT + layer_of(o);

        append_object(buf[k] + filled[k] * VERTS_PER_OBJECT, o,
            gd->atlas_rect[o->type], object_color(o));
        filled[k] += 1;
        fill_chunk_bounds(&lv->chunks[chunk_index(o)], o);
    }
    (void)counts;
    free(filled);
}

static void upload_one(render_chunk_t *ch, int l, sfVertex *v, size_t n)
{
    ch->layer[l] = sfVertexBuffer_create((unsigned int)n, sfTriangles,
        sfVertexBufferStatic);
    if (ch->layer[l] != NULL) {
        sfVertexBuffer_update(ch->layer[l], v, (unsigned int)n, 0);
        return;
    }
    ch->array[l] = sfVertexArray_create();   /* no vertex buffers: same data */
    sfVertexArray_setPrimitiveType(ch->array[l], sfTriangles);
    for (size_t i = 0; i < n; i++)
        sfVertexArray_append(ch->array[l], v[i]);
}

static void upload(level_t *lv, size_t *counts, sfVertex **buf)
{
    for (size_t c = 0; c < lv->nb_chunks; c++)
        for (int l = 0; l < LAYER_COUNT; l++)
            if (counts[c * LAYER_COUNT + l] > 0)
                upload_one(&lv->chunks[c], l, buf[c * LAYER_COUNT + l],
                    counts[c * LAYER_COUNT + l] * VERTS_PER_OBJECT);
}

/*
** Objects never move (Principle 1), so their vertices are computed once per
** level and live on the GPU from then on. The vertex arrays are kept as a
** fallback for drivers without vertex buffers.
*/
void level_build_chunks(level_t *lv, gd_t *gd)
{
    double right = 0.0;
    size_t *counts = NULL;
    sfVertex **buf = NULL;

    for (size_t i = 0; i < lv->sim.lvl.nb_objects; i++) {
        rect_t d = object_drawn_bounds(&lv->sim.lvl.objects[i]);

        right = fmax(right, d.x + d.w);
    }
    lv->nb_chunks = (size_t)(right / CHUNK_W) + 1;
    lv->chunks = sim_xcalloc(lv->nb_chunks, sizeof(render_chunk_t));
    for (size_t c = 0; c < lv->nb_chunks; c++)
        lv->chunks[c] = (render_chunk_t){INFINITY, -INFINITY, {NULL}, {NULL}};
    counts = sim_xcalloc(lv->nb_chunks * LAYER_COUNT, sizeof(size_t));
    count_objects(lv, counts);
    buf = sim_xcalloc(lv->nb_chunks * LAYER_COUNT, sizeof(sfVertex *));
    for (size_t k = 0; k < lv->nb_chunks * LAYER_COUNT; k++)
        if (counts[k] > 0)
            buf[k] = sim_xcalloc(counts[k] * VERTS_PER_OBJECT,
                sizeof(sfVertex));
    build_vertices(lv, gd, counts, buf);
    upload(lv, counts, buf);
    for (size_t k = 0; k < lv->nb_chunks * LAYER_COUNT; k++)
        free(buf[k]);
    free(buf);
    free(counts);
}

void level_free_chunks(level_t *lv)
{
    for (size_t c = 0; c < lv->nb_chunks; c++)
        for (int l = 0; l < LAYER_COUNT; l++) {
            if (lv->chunks[c].layer[l] != NULL)
                sfVertexBuffer_destroy(lv->chunks[c].layer[l]);
            if (lv->chunks[c].array[l] != NULL)
                sfVertexArray_destroy(lv->chunks[c].array[l]);
        }
    free(lv->chunks);
    lv->chunks = NULL;
    lv->nb_chunks = 0;
}

/* Layer by layer, only the chunks the view touches: a few draw calls (9.0). */
void render_objects(gd_t *gd, level_t *lv, float cam_x)
{
    sfRenderStates rs = {sfBlendAlpha, sfTransform_Identity, gd->atlas, NULL};

    for (int l = 0; l < LAYER_COUNT; l++)
        for (size_t c = 0; c < lv->nb_chunks; c++) {
            render_chunk_t *ch = &lv->chunks[c];

            if (ch->right < cam_x || ch->left > cam_x + VIEW_W)
                continue;
            if (ch->layer[l] != NULL)
                draw_vertex_buffer(gd, ch->layer[l], &rs);
            else if (ch->array[l] != NULL)
                draw_vertex_array(gd, ch->array[l], &rs);
        }
}
