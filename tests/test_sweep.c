/*
** ALEXNEX PROJECT, 2026
** tests/test_sweep.c
** File description:
** the square's sweeps: contacts, touches, overlaps, half-planes (4.3, G.3, G.5)
*/

#include <math.h>

#include "sim/geom.h"
#include "sim/hitbox.h"
#include "sim/sweep.h"
#include "test.h"

#define HALF 50.0

static hitbox_t block(double x, double y, double w, double h, double deg)
{
    object_t o = {.type = OBJ_BLOCK, .rect = {x, y, w, h}, .rotation = deg};

    hitbox_for_object(&o);
    return o.hitbox;
}

static void test_land_on_top(void)
{
    hitbox_t b = block(1000.0, 700.0, 100.0, 100.0, 0.0);
    contact_t c = {0};

    CHECK(sweep_box_poly((vec2_t){1050.0, 600.0}, HALF, (vec2_t){0.0, 200.0},
        &b, &c));
    CHECK(c.t == 0.25);                      /* bottom 650 reaches 700 */
    CHECK(c.flat);
    CHECK(c.normal.x == 0.0 && c.normal.y == -1.0);
    CHECK(c.offset == -700.0);
    /* settle (4.4) puts the square exactly on the face */
    CHECK((c.offset + HALF) * c.normal.y == 650.0);
}

/* Resting on a face and sliding along it is not a contact (3.0). */
static void test_slide_on_top(void)
{
    hitbox_t b = block(1000.0, 700.0, 100.0, 100.0, 0.0);
    contact_t c = {0};

    CHECK(!sweep_box_poly((vec2_t){1050.0, 650.0}, HALF, (vec2_t){10.0, 0.0},
        &b, &c));
    CHECK(sweep_box_touch((vec2_t){1050.0, 650.0}, HALF, (vec2_t){10.0, 0.0},
        &b) == INFINITY);
}

/* The player's top exactly under a block: level 5's 100 px gap. */
static void test_exact_gap(void)
{
    hitbox_t b = block(1000.0, 650.0, 100.0, 100.0, 0.0);
    contact_t c = {0};

    CHECK(!sweep_box_poly((vec2_t){900.0, 800.0}, HALF, (vec2_t){200.0, 0.0},
        &b, &c));
}

static void test_run_into_side(void)
{
    hitbox_t b = block(1000.0, 700.0, 100.0, 100.0, 0.0);
    contact_t c = {0};

    CHECK(sweep_box_poly((vec2_t){900.0, 750.0}, HALF, (vec2_t){200.0, 0.0},
        &b, &c));
    CHECK(c.t == 0.25);                      /* right edge 950 reaches 1000 */
    CHECK(!c.flat);
    CHECK(c.normal.x == -1.0 && c.normal.y == 0.0);
}

/* Reaching a corner on both axes at once lands: the y axis is tested first. */
static void test_corner_tie(void)
{
    hitbox_t b = block(1000.0, 700.0, 100.0, 100.0, 0.0);
    contact_t c = {0};

    CHECK(sweep_box_poly((vec2_t){900.0, 600.0}, HALF, (vec2_t){100.0, 100.0},
        &b, &c));
    CHECK(c.t == 0.5);
    CHECK(c.flat);
    CHECK(c.normal.y == -1.0);
}

static void test_moving_away(void)
{
    hitbox_t b = block(1000.0, 700.0, 100.0, 100.0, 0.0);
    contact_t c = {0};

    CHECK(!sweep_box_poly((vec2_t){1050.0, 650.0}, HALF, (vec2_t){0.0, -100.0},
        &b, &c));
}

static void test_touch_and_tunneling(void)
{
    object_t s = {.type = OBJ_SPIKE, .rect = {1000.0, 750.0, 100.0, 100.0}};
    hitbox_t b = block(1000.0, 700.0, 100.0, 100.0, 0.0);

    hitbox_for_object(&s);
    /* the spike box is 1030..1070: crossed in one 200 px move, not skipped */
    CHECK(sweep_box_touch((vec2_t){900.0, 750.0}, HALF, (vec2_t){200.0, 0.0},
        &s.hitbox) == 0.4);
    CHECK(sweep_box_touch((vec2_t){900.0, 750.0}, HALF, (vec2_t){10.0, 0.0},
        &s.hitbox) == INFINITY);
    /* a touch sweep ignores which way the face looks */
    CHECK(sweep_box_touch((vec2_t){1050.0, 600.0}, HALF, (vec2_t){0.0, 200.0},
        &b) == 0.25);
}

static void test_overlap(void)
{
    hitbox_t b = block(1000.0, 700.0, 100.0, 100.0, 0.0);
    hitbox_t small = block(1040.0, 740.0, 20.0, 20.0, 0.0);

    CHECK(overlap_box_poly((vec2_t){1050.0, 750.0}, HALF, &b));    /* inside   */
    CHECK(!overlap_box_poly((vec2_t){1050.0, 650.0}, HALF, &b));   /* touching */
    CHECK(!overlap_box_poly((vec2_t){1050.0, 649.0}, HALF, &b));   /* above    */
    CHECK(overlap_box_poly((vec2_t){1050.0, 651.0}, HALF, &b));    /* 1 px in  */
    CHECK(overlap_box_poly((vec2_t){1050.0, 750.0}, HALF, &small));/* contains */
}

/*
** A diamond (block at 45 deg) has no horizontal face: its top is a vertex.
** Landing straight on it is still a y-axis contact, and the square stands
** level on that point. That is intended (4.4).
*/
static void test_apex_is_flat(void)
{
    hitbox_t b = block(1000.0, 700.0, 100.0, 100.0, 45.0);
    contact_t c = {0};

    CHECK(sweep_box_poly((vec2_t){1050.0, 500.0}, HALF, (vec2_t){0.0, 200.0},
        &b, &c));
    CHECK(c.flat);
    CHECK(c.normal.y == -1.0);
    CHECK(fabs(c.offset + b.aabb.y) < 1e-9);       /* the diamond's top y */
}

/* Off to the side, the same fall meets a real tilted face, on its own axis. */
static void test_tilted_face(void)
{
    hitbox_t b = block(1000.0, 700.0, 100.0, 100.0, 45.0);
    contact_t c = {0};

    CHECK(sweep_box_poly((vec2_t){1110.0, 500.0}, HALF, (vec2_t){0.0, 200.0},
        &b, &c));
    CHECK(!c.flat);
    CHECK(fabs(c.normal.x - sqrt(0.5)) < 1e-9);
    CHECK(fabs(c.normal.y + sqrt(0.5)) < 1e-9);
    CHECK(c.t > 0.69 && c.t < 0.70);
}

static void test_ground_plane(void)
{
    contact_t c = {0};

    CHECK(sweep_box_plane((vec2_t){350.0, 700.0}, HALF, (vec2_t){0.0, 200.0},
        GROUND_Y, 1.0, &c));
    CHECK(c.t == 0.5);
    CHECK(c.flat && c.normal.y == -1.0);
    CHECK(c.offset == -GROUND_Y);
    CHECK((c.offset + HALF) * c.normal.y == 800.0);
    /* running along the ground, and moving away from it: no contact */
    CHECK(!sweep_box_plane((vec2_t){350.0, 800.0}, HALF, (vec2_t){4.3275, 0.0},
        GROUND_Y, 1.0, &c));
    CHECK(!sweep_box_plane((vec2_t){350.0, 800.0}, HALF, (vec2_t){0.0, -10.0},
        GROUND_Y, 1.0, &c));
}

static void test_ceiling_plane(void)
{
    contact_t c = {0};

    CHECK(sweep_box_plane((vec2_t){350.0, 60.0}, HALF, (vec2_t){0.0, -40.0},
        0.0, -1.0, &c));
    CHECK(c.t == 0.25);
    CHECK(c.flat && c.normal.y == 1.0);
    CHECK((c.offset + HALF) * c.normal.y == 50.0);
    CHECK(sweep_circle_plane((vec2_t){350.0, 60.0}, HALF, (vec2_t){0.0, -40.0},
        0.0, -1.0, &c));
    CHECK(c.t == 0.25);
}

/* A shape already past a surface meets it at once: it can't be crossed (4.3). */
static void test_plane_from_inside(void)
{
    contact_t c = {0};

    CHECK(sweep_box_plane((vec2_t){350.0, 835.0}, HALF, (vec2_t){0.0, 0.1},
        GROUND_Y, 1.0, &c));
    CHECK(c.t == 0.0);
    CHECK((c.offset + HALF) * c.normal.y == 800.0);
}

void test_sweep(void)
{
    test_land_on_top();
    test_slide_on_top();
    test_exact_gap();
    test_run_into_side();
    test_corner_tie();
    test_moving_away();
    test_touch_and_tunneling();
    test_overlap();
    test_apex_is_flat();
    test_tilted_face();
    test_ground_plane();
    test_ceiling_plane();
    test_plane_from_inside();
}
