/*
** ALEXNEX PROJECT, 2026
** tests/test_circle.c
** File description:
** the circle's sweeps, the half-plane clipper and the saw disc (G.4, G.6, G.10)
*/

#include <math.h>

#include "sim/geom.h"
#include "sim/hitbox.h"
#include "sim/sweep.h"
#include "test.h"

#define R 50.0

static hitbox_t shape(obj_type_t type, rect_t rect, double deg)
{
    object_t o = {.type = type, .rect = rect, .rotation = deg};

    hitbox_for_object(&o);
    return o.hitbox;
}

static bool no_face(const hitbox_t *h, int i, double g)
{
    (void)h;
    (void)i;
    (void)g;
    return false;
}

static bool tilted_only(const hitbox_t *h, int i, double g)
{
    (void)g;
    return h->face_kind[i] == FACE_TILTED;
}

static void test_distance(void)
{
    hitbox_t b = shape(OBJ_BLOCK, (rect_t){1000.0, 700.0, 100.0, 100.0}, 0.0);

    CHECK(poly_distance((vec2_t){1050.0, 600.0}, &b) == 100.0);
    CHECK(poly_distance((vec2_t){1050.0, 750.0}, &b) == 0.0);        /* inside */
    CHECK(poly_distance((vec2_t){1050.0, 700.0}, &b) == 0.0);        /* on the edge */
    CHECK(fabs(poly_distance((vec2_t){900.0, 600.0}, &b)
        - sqrt(2.0) * 100.0) < 1e-9);                                /* corner */
}

static void test_overlap_circle(void)
{
    hitbox_t b = shape(OBJ_BLOCK, (rect_t){1000.0, 700.0, 100.0, 100.0}, 0.0);

    CHECK(!overlap_circle_poly((vec2_t){1050.0, 649.0}, R, &b));
    CHECK(!overlap_circle_poly((vec2_t){1050.0, 650.0}, R, &b));     /* touching */
    CHECK(overlap_circle_poly((vec2_t){1050.0, 651.0}, R, &b));
    CHECK(overlap_circle_poly((vec2_t){1050.0, 750.0}, R, &b));      /* inside */
}

/* The circle rests 50 px from a 45 deg face, whatever the angle (4.4). */
static void test_land_on_slope(void)
{
    hitbox_t s = shape(OBJ_SLOPE, (rect_t){1000.0, 750.0, 100.0, 100.0}, 0.0);
    contact_t c = {0};
    vec2_t end;
    int tilted = 0;

    CHECK(sweep_circle_poly((vec2_t){1050.0, 600.0}, R, (vec2_t){0.0, 300.0},
        &s, NULL, NULL, 1.0, &c));
    CHECK(fabs(c.normal.x + sqrt(0.5)) < 1e-9);
    CHECK(fabs(c.normal.y + sqrt(0.5)) < 1e-9);
    end = (vec2_t){1050.0, 600.0 + 300.0 * c.t};
    for (int i = 0; i < s.nverts; i++)
        if (s.face_kind[i] == FACE_TILTED)
            tilted = i;
    CHECK(fabs(dot(s.face_n[tilted], end) - (s.face_off[tilted] + R)) < 1e-9);
}

/* Falling next to a block, the circle rolls onto its corner (G.4). */
static void test_vertex_contact(void)
{
    hitbox_t b = shape(OBJ_BLOCK, (rect_t){1000.0, 700.0, 100.0, 100.0}, 0.0);
    contact_t c = {0};

    CHECK(sweep_circle_poly((vec2_t){960.0, 600.0}, R, (vec2_t){0.0, 300.0},
        &b, NULL, NULL, 1.0, &c));
    CHECK(fabs(c.t - 7.0 / 30.0) < 1e-9);
    CHECK(fabs(c.normal.x + 0.8) < 1e-9);
    CHECK(fabs(c.normal.y + 0.6) < 1e-9);
    CHECK(fabs(vlen(c.normal) - 1.0) < 1e-12);
}

/* Too far to the side: neither the face nor the corner is reached. */
static void test_misses(void)
{
    hitbox_t b = shape(OBJ_BLOCK, (rect_t){1000.0, 700.0, 100.0, 100.0}, 0.0);
    contact_t c = {0};

    CHECK(!sweep_circle_poly((vec2_t){800.0, 600.0}, R, (vec2_t){0.0, 300.0},
        &b, NULL, NULL, 1.0, &c));
    CHECK(sweep_circle_touch((vec2_t){800.0, 600.0}, R, (vec2_t){0.0, 300.0},
        &b) == INFINITY);
}

static void test_filters(void)
{
    hitbox_t b = shape(OBJ_BLOCK, (rect_t){1000.0, 700.0, 100.0, 100.0}, 0.0);
    hitbox_t s = shape(OBJ_SLOPE, (rect_t){1000.0, 750.0, 100.0, 100.0}, 0.0);
    contact_t c = {0};

    /* a block has no tilted face: the circle's support filter finds nothing */
    CHECK(!sweep_circle_poly((vec2_t){1050.0, 600.0}, R, (vec2_t){0.0, 300.0},
        &b, tilted_only, no_face, 1.0, &c));
    /* the same filter does find the slope's tilted face */
    CHECK(sweep_circle_poly((vec2_t){1050.0, 600.0}, R, (vec2_t){0.0, 300.0},
        &s, tilted_only, no_face, 1.0, &c));
    /* rejecting everything means no contact at all */
    CHECK(!sweep_circle_poly((vec2_t){1050.0, 600.0}, R, (vec2_t){0.0, 300.0},
        &s, no_face, no_face, 1.0, &c));
}

static void test_touch_from_inside(void)
{
    hitbox_t b = shape(OBJ_BLOCK, (rect_t){1000.0, 700.0, 100.0, 100.0}, 0.0);
    contact_t c = {0};

    CHECK(sweep_circle_touch((vec2_t){1050.0, 750.0}, R, (vec2_t){0.0, 10.0},
        &b) == 0.0);
    CHECK(sweep_circle_poly((vec2_t){1050.0, 660.0}, R, (vec2_t){0.0, 10.0},
        &b, NULL, NULL, 1.0, &c));
    CHECK(c.t == 0.0);
    CHECK(c.normal.y == -1.0);               /* pushed out the way it came in */
}

static void test_clip(void)
{
    hitbox_t b = shape(OBJ_BLOCK, (rect_t){1000.0, 700.0, 100.0, 100.0}, 0.0);
    hitbox_t s = shape(OBJ_SLOPE, (rect_t){1000.0, 750.0, 100.0, 100.0}, 0.0);
    vec2_t out[HB_MAX_VERTS + 1];
    int n = clip_half_plane(b.verts, b.nverts, 750.0, 1.0, out);

    CHECK(n == 4);                           /* the lower half of the block */
    for (int i = 0; i < n; i++)
        CHECK(out[i].y >= 750.0);
    CHECK(clip_half_plane(b.verts, b.nverts, 900.0, 1.0, out) == 0);
    CHECK(clip_half_plane(b.verts, b.nverts, 600.0, 1.0, out) == 4);
    n = clip_half_plane(b.verts, b.nverts, 750.0, -1.0, out);
    CHECK(n == 4);                           /* mirrored: the upper half */
    for (int i = 0; i < n; i++)
        CHECK(out[i].y <= 750.0);
    n = clip_half_plane(s.verts, s.nverts, 800.0, 1.0, out);
    CHECK(n == 4);                           /* a triangle cut gives 4 corners */
}

static void test_saw_disc(void)
{
    vec2_t c = {900.0, 750.0};
    vec2_t d = {200.0, 0.0};

    /* head on: the square's side touches the disc 92 px from its center */
    CHECK(fabs(sweep_box_disc(c, R, d, (vec2_t){1100.0, 750.0}, 42.0)
        - 0.54) < 1e-9);
    /* below the square's reach: nothing */
    CHECK(sweep_box_disc(c, R, d, (vec2_t){1100.0, 850.0}, 42.0) == INFINITY);
    /* a corner hit comes before the slab would say so */
    CHECK(fabs(sweep_box_disc(c, R, d, (vec2_t){1060.0, 820.0}, 42.0)
        - 0.36534) < 1e-4);
    /* already touching it */
    CHECK(sweep_box_disc(c, R, d, (vec2_t){920.0, 750.0}, 42.0) == 0.0);
}

void test_circle(void)
{
    test_distance();
    test_overlap_circle();
    test_land_on_slope();
    test_vertex_contact();
    test_misses();
    test_filters();
    test_touch_from_inside();
    test_clip();
    test_saw_disc();
}
