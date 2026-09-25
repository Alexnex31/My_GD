/*
** ALEXNEX PROJECT, 2026
** tests/test_hitbox.c
** File description:
** hitbox building: rotation, grid, winding, faces, axes (G.2)
*/

#include <math.h>

#include "sim/geom.h"
#include "sim/hitbox.h"
#include "test.h"

static object_t make(obj_type_t type, rect_t rect, double rotation)
{
    object_t o = {.type = type, .rect = rect, .rotation = rotation};

    hitbox_for_object(&o);
    return o;
}

static int count_faces(const hitbox_t *h, face_kind_t kind)
{
    int n = 0;

    for (int i = 0; i < h->nverts; i++)
        n += h->face_kind[i] == kind;
    return n;
}

static bool on_grid(const hitbox_t *h)
{
    for (int i = 0; i < h->nverts; i++)
        if (h->verts[i].x != round(h->verts[i].x * 1024.0) / 1024.0
            || h->verts[i].y != round(h->verts[i].y * 1024.0) / 1024.0)
            return false;
    return true;
}

static bool clockwise(const hitbox_t *h)
{
    double area = 0.0;

    for (int i = 0; i < h->nverts; i++)
        area += cross(h->verts[i], h->verts[(i + 1) % h->nverts]);
    return area > 0.0;
}

static bool same_rect(rect_t a, rect_t b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static void test_block_flat(void)
{
    rect_t r = {1000.0, 700.0, 100.0, 100.0};
    object_t o = make(OBJ_BLOCK, r, 0.0);

    CHECK(o.hitbox.kind == SHAPE_POLY);
    CHECK(o.hitbox.nverts == 4);
    CHECK(o.hitbox.naxes == 0);
    CHECK(same_rect(o.hitbox.aabb, r));
    CHECK(count_faces(&o.hitbox, FACE_HORIZONTAL) == 2);
    CHECK(count_faces(&o.hitbox, FACE_VERTICAL) == 2);
    CHECK(count_faces(&o.hitbox, FACE_TILTED) == 0);
    CHECK(clockwise(&o.hitbox));
    CHECK(on_grid(&o.hitbox));
}

/* Quarter turns must stay bit-exact: level 5's 100 px gap depends on it. */
static void test_block_quarter_turns(void)
{
    rect_t r = {1000.0, 700.0, 100.0, 100.0};

    for (double deg = 90.0; deg <= 270.0; deg += 90.0) {
        object_t o = make(OBJ_BLOCK, r, deg);

        CHECK(same_rect(o.hitbox.aabb, r));
        CHECK(o.hitbox.naxes == 0);
        CHECK(count_faces(&o.hitbox, FACE_TILTED) == 0);
        CHECK(clockwise(&o.hitbox));
    }
}

static void test_block_tilted(void)
{
    object_t o = make(OBJ_BLOCK, (rect_t){1000.0, 700.0, 100.0, 100.0}, 30.0);

    CHECK(o.hitbox.naxes == 2);
    CHECK(count_faces(&o.hitbox, FACE_TILTED) == 4);
    CHECK(clockwise(&o.hitbox));
    CHECK(fabs(vlen(o.hitbox.axes[0]) - 1.0) < 1e-12);
    CHECK(fabs(vlen(o.hitbox.axes[1]) - 1.0) < 1e-12);
    CHECK(o.hitbox.aabb.w > 100.0 && o.hitbox.aabb.w < 141.5);
}

static void test_slope(void)
{
    object_t o = make(OBJ_SLOPE, (rect_t){1000.0, 750.0, 100.0, 100.0}, 0.0);
    int tilted = -1;

    CHECK(o.hitbox.nverts == 3);
    CHECK(o.hitbox.naxes == 1);
    CHECK(count_faces(&o.hitbox, FACE_HORIZONTAL) == 1);
    CHECK(count_faces(&o.hitbox, FACE_VERTICAL) == 1);
    CHECK(count_faces(&o.hitbox, FACE_TILTED) == 1);
    CHECK(clockwise(&o.hitbox));
    for (int i = 0; i < o.hitbox.nverts; i++)
        if (o.hitbox.face_kind[i] == FACE_TILTED)
            tilted = i;
    CHECK(tilted >= 0);
    CHECK(fabs(o.hitbox.face_n[tilted].x + sqrt(0.5)) < 1e-12);
    CHECK(fabs(o.hitbox.face_n[tilted].y + sqrt(0.5)) < 1e-12);
}

/* A slope turned upside down has a horizontal top, like a block's (4.3). */
static void test_slope_upside_down(void)
{
    object_t o = make(OBJ_SLOPE, (rect_t){1000.0, 750.0, 100.0, 100.0}, 180.0);
    face_t f = {0};
    bool found = false;

    for (int i = 0; i < o.hitbox.nverts; i++)
        if (up_facing_horizontal_face(&o.hitbox, i, 1.0, &f))
            found = true;
    CHECK(found);
    CHECK(f.y == 750.0);
    CHECK(f.x0 == 1000.0 && f.x1 == 1100.0);
}

static void test_up_facing_face(void)
{
    object_t o = make(OBJ_BLOCK, (rect_t){1000.0, 700.0, 100.0, 100.0}, 0.0);
    face_t up = {0};
    face_t down = {0};
    int nb_up = 0;
    int nb_down = 0;

    for (int i = 0; i < o.hitbox.nverts; i++) {
        nb_up += up_facing_horizontal_face(&o.hitbox, i, 1.0, &up);
        nb_down += up_facing_horizontal_face(&o.hitbox, i, -1.0, &down);
    }
    CHECK(nb_up == 1 && nb_down == 1);
    CHECK(up.y == 700.0 && up.x0 == 1000.0 && up.x1 == 1100.0);
    CHECK(down.y == 800.0 && down.x0 == 1000.0 && down.x1 == 1100.0);
}

static void test_spike(void)
{
    object_t o = make(OBJ_SPIKE, (rect_t){1000.0, 750.0, 100.0, 100.0}, 0.0);

    CHECK(same_rect(o.hitbox.aabb, (rect_t){1030.0, 770.0, 40.0, 80.0}));
    CHECK(count_faces(&o.hitbox, FACE_TILTED) == 0);
    CHECK(clockwise(&o.hitbox));
}

/* A ceiling spike is the same box, mirrored around the sprite's center. */
static void test_spike_rotated(void)
{
    object_t o = make(OBJ_SPIKE, (rect_t){1000.0, 0.0, 100.0, 100.0}, 180.0);

    CHECK(same_rect(o.hitbox.aabb, (rect_t){1030.0, 0.0, 40.0, 80.0}));
    CHECK(o.hitbox.naxes == 0);
}

static void test_axes_and_projections(void)
{
    object_t o = make(OBJ_BLOCK, (rect_t){0.0, 0.0, 100.0, 100.0}, 45.0);

    CHECK(o.hitbox.naxes == 2);
    for (int k = 0; k < o.hitbox.naxes; k++) {
        double lo = dot(o.hitbox.verts[0], o.hitbox.axes[k]);
        double hi = lo;

        for (int i = 1; i < o.hitbox.nverts; i++) {
            lo = fmin(lo, dot(o.hitbox.verts[i], o.hitbox.axes[k]));
            hi = fmax(hi, dot(o.hitbox.verts[i], o.hitbox.axes[k]));
        }
        CHECK(o.hitbox.axis_lo[k] == lo && o.hitbox.axis_hi[k] == hi);
    }
    CHECK(hitbox_lo(&o.hitbox, 0) == o.hitbox.aabb.y);
    CHECK(hitbox_hi(&o.hitbox, 1) == o.hitbox.aabb.x + o.hitbox.aabb.w);
    CHECK(hitbox_axis(&o.hitbox, 0).y == 1.0);
    CHECK(hitbox_axis(&o.hitbox, 1).x == 1.0);
}

static void test_circle_shape(void)
{
    hitbox_t h = {0};

    hitbox_build_circle(&h, (rect_t){1000.0, 700.0, 100.0, 100.0}, 42.0);
    CHECK(h.kind == SHAPE_CIRCLE);
    CHECK(h.center.x == 1050.0 && h.center.y == 750.0);
    CHECK(h.radius == 42.0);
    CHECK(same_rect(h.aabb, (rect_t){1008.0, 708.0, 84.0, 84.0}));
}

static void test_rect_overlap(void)
{
    rect_t a = {0.0, 0.0, 100.0, 100.0};

    CHECK(rect_overlap(a, (rect_t){50.0, 50.0, 100.0, 100.0}));
    CHECK(!rect_overlap(a, (rect_t){100.0, 0.0, 100.0, 100.0}));   /* touching */
    CHECK(!rect_overlap(a, (rect_t){0.0, 100.0, 100.0, 100.0}));
    CHECK(rect_overlap(a, (rect_t){10.0, 10.0, 10.0, 10.0}));      /* contained */
}

void test_hitbox(void)
{
    test_block_flat();
    test_block_quarter_turns();
    test_block_tilted();
    test_slope();
    test_slope_upside_down();
    test_up_facing_face();
    test_spike();
    test_spike_rotated();
    test_axes_and_projections();
    test_circle_shape();
    test_rect_overlap();
}
