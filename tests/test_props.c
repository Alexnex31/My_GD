/*
** ALEXNEX PROJECT, 2026
** tests/test_props.c
** File description:
** start positions as editor objects, the properties, turning (FEATURES 11.4, 11.8)
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor/ed_props.h"
#include "sim/hitbox.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

static ed_history_t H;

static void begin(ed_level_t *lv)
{
    ed_history_free(&H);
    H = (ed_history_t){0};
    ed_level_new(lv, "1");
}

static int add(ed_level_t *lv, const char *line)
{
    object_t o;

    level_object_from_line(line, &o);
    ed_gesture(&H);
    return ed_do_add(lv, &H, &o, NULL);
}

static bool saves_as(const ed_level_t *lv, const char *want)
{
    char *text = ed_level_text(lv);
    bool same = strcmp(text, want) == 0;

    if (!same)
        printf("saved:\n%s", text);
    free(text);
    return same;
}

static bool prop_is(const ed_level_t *lv, ed_prop_t p, const char *want)
{
    char text[64];

    ed_prop_text(lv, p, text, sizeof(text));
    return strcmp(text, want) == 0;
}

static const ed_entry_t *start_entry(const ed_entry_t *e, int n)
{
    for (int i = 0; i < n; i++)
        if (e[i].start)
            return &e[i];
    return NULL;
}

/*
** A start position is selected, moved, copied and undone like an object,
** is saved as its own line, and is no object of the level the game plays.
*/
static void test_start_objects(void)
{
    ed_entry_t e[ED_MAX_ENTRIES];
    int n = ed_entries(e, ED_MAX_ENTRIES);
    ed_level_t lv;
    level_doc_t doc;
    sim_t s;
    int block;
    int start;

    begin(&lv);
    block = add(&lv, "block 1200 700 2");
    ed_gesture(&H);
    CHECK(start_entry(e, n) != NULL);
    CHECK(ed_do_place(&lv, &H, start_entry(e, n), (vec2_t){1230.0, 760.0},
        50.0));
    CHECK(!ed_do_place(&lv, &H, start_entry(e, n), (vec2_t){1240.0, 770.0},
        50.0));                                        /* the same cell */
    CHECK(lv.count == 2 && ed_starts(&lv) == 1);
    start = lv.objects[1].id;
    CHECK(lv.objects[1].start.pos.x == 1250.0);        /* the cell's middle */
    CHECK(lv.objects[1].start.pos.y == 800.0);
    CHECK(lv.objects[1].start.mode == MODE_CUBE);
    CHECK(saves_as(&lv, "name LEVEL 1\nstart 1250 800 cube\n\n"
        "block 1200 700 2\n"));
    CHECK(ed_hit(&lv, (vec2_t){1260.0, 760.0}) == start);  /* over the block */
    CHECK(ed_hit(&lv, (vec2_t){1260.0, 720.0}) == block);
    ed_level_doc(&lv, &doc);
    CHECK(doc.count == 1 && doc.nb_starts == 1);
    sim_init(&s, &doc, &lv.hdr, lv.id);
    CHECK(s.lvl.nb_objects == 1 && s.lvl.nb_starts == 1);
    CHECK(s.lvl.hdr.start.pos.x == PLAYER_SPAWN_X);    /* the real one stays */
    sim_free(&s);
    ed_doc_free(&doc);
    ed_select_click(&lv, start, false);
    ed_gesture(&H);
    CHECK(ed_nudge(&lv, &H, 200.0, -100.0));
    CHECK(ed_find(&lv, start)->start.pos.x == 1450.0);
    CHECK(saves_as(&lv, "name LEVEL 1\nstart 1450 700 cube\n\n"
        "block 1200 700 2\n"));
    ed_gesture(&H);
    CHECK(ed_duplicate(&lv, &H, 50.0) && ed_starts(&lv) == 2);
    CHECK(lv.objects[2].start.pos.x == 1500.0);
    CHECK(ed_undo(&lv, &H) && ed_undo(&lv, &H));
    CHECK(ed_find(&lv, start)->start.pos.x == 1250.0 && ed_starts(&lv) == 1);
    CHECK(lv.hdr.start.pos.x == PLAYER_SPAWN_X);
    ed_level_free(&lv);
}

/* Opened, the file's start lines are objects, in the file's order. */
static void test_start_lines(void)
{
    const char *text = "name S\nstart 900 300 ship up\nstart 400 800 ball 2\n"
        "\nblock 100 750 2\n";
    ed_level_t lv;

    ed_level_from_text(&lv, text, strlen(text), "5");
    CHECK(lv.count == 3 && ed_starts(&lv) == 2 && !lv.dirty);
    CHECK(lv.objects[1].is_start && lv.objects[1].start.mode == MODE_SHIP);
    CHECK(lv.objects[1].start.gravity_dir == -1);
    CHECK(lv.objects[1].obj.rect.x == 850.0 && lv.objects[1].obj.rect.w == 100.0);
    CHECK(lv.objects[2].start.speed == 2.0);
    CHECK(saves_as(&lv, text));
    ed_remove(&lv, lv.objects[1].id);
    CHECK(saves_as(&lv, "name S\nstart 400 800 ball 2\n\nblock 100 750 2\n"));
    ed_level_free(&lv);
}

/* Which rows the panel shows, and what they say. */
static void test_applies(void)
{
    const char *text = "start 900 300 ship up\nblock 100 750 2\n"
        "portal 500 650 2 ufo\npad 700 750 2 red\nspike 900 750 2 rot=90\n";
    ed_level_t lv;

    ed_level_from_text(&lv, text, strlen(text), "5");
    CHECK(!ed_prop_applies(&lv, ED_PROP_TURN));        /* nothing selected */
    ed_select_click(&lv, lv.objects[0].id, false);     /* the block */
    CHECK(ed_prop_applies(&lv, ED_PROP_TURN) && ed_prop_applies(&lv, ED_PROP_W));
    CHECK(!ed_prop_applies(&lv, ED_PROP_KIND));
    CHECK(!ed_prop_applies(&lv, ED_PROP_SPEED));
    CHECK(prop_is(&lv, ED_PROP_TURN, "0") && prop_is(&lv, ED_PROP_W, "2"));
    ed_select_click(&lv, lv.objects[3].id, true);      /* and the spike */
    CHECK(prop_is(&lv, ED_PROP_TURN, "mixed") && prop_is(&lv, ED_PROP_H, "2"));
    ed_select_click(&lv, lv.objects[1].id, false);     /* the portal */
    CHECK(prop_is(&lv, ED_PROP_KIND, "ufo"));
    ed_select_click(&lv, lv.objects[2].id, true);
    CHECK(prop_is(&lv, ED_PROP_KIND, "mixed"));
    ed_select_click(&lv, lv.objects[4].id, false);     /* the start */
    CHECK(!ed_prop_applies(&lv, ED_PROP_TURN) && !ed_prop_applies(&lv, ED_PROP_W));
    CHECK(prop_is(&lv, ED_PROP_KIND, "ship"));
    CHECK(prop_is(&lv, ED_PROP_GRAVITY, "up") && prop_is(&lv, ED_PROP_SPEED, "x1"));
    ed_level_free(&lv);
}

/* Turn and size: steps, limits, the hitbox with them, one undo each. */
static void test_shape(void)
{
    ed_level_t lv;
    int a;

    begin(&lv);
    a = add(&lv, "block 100 700 2");
    ed_select_click(&lv, a, false);
    ed_gesture(&H);
    CHECK(ed_prop_step(&lv, &H, ED_PROP_TURN, -1));
    CHECK(ed_find(&lv, a)->obj.rotation == 345.0 && H.nb_undo == 2);
    CHECK(ed_prop_step(&lv, &H, ED_PROP_TURN, 1));
    CHECK(ed_find(&lv, a)->obj.rotation == 0.0 && H.nb_undo == 3);
    CHECK(ed_prop_step(&lv, &H, ED_PROP_W, 1) && ed_prop_step(&lv, &H,
        ED_PROP_W, 1));
    CHECK(ed_find(&lv, a)->obj.rect.w == 200.0);
    CHECK(ed_find(&lv, a)->obj.rect.x == 100.0);       /* the corner stays */
    CHECK(ed_find(&lv, a)->obj.hitbox.aabb.w == 200.0);
    CHECK(ed_hit(&lv, (vec2_t){290.0, 750.0}) == a);
    CHECK(ed_prop_step(&lv, &H, ED_PROP_H, -1));
    CHECK(!ed_prop_step(&lv, &H, ED_PROP_H, -1));      /* one unit: no less */
    CHECK(ed_find(&lv, a)->obj.rect.h == 50.0 && H.nb_undo == 6);
    CHECK(!ed_prop_step(&lv, &H, ED_PROP_KIND, 1) && H.nb_undo == 6);
    CHECK(saves_as(&lv, "name LEVEL 1\n\nblock 100 700 2 w=4 h=1\n"));
    CHECK(ed_undo(&lv, &H) && ed_find(&lv, a)->obj.rect.h == 100.0);
    CHECK(ed_find(&lv, a)->obj.hitbox.aabb.h == 100.0);
    ed_level_free(&lv);
}

/* Kinds go round what the tables hold; a start's speed stops at its ends. */
static void test_kinds(void)
{
    ed_level_t lv;
    int portal;
    int pad;
    int orb;
    int gravity;
    int steps = 0;

    begin(&lv);
    portal = add(&lv, "portal 500 650 2 cube");
    pad = add(&lv, "pad 700 750 2 yellow");
    orb = add(&lv, "orb 900 650 2 yellow");
    gravity = add(&lv, "gravity 1100 650 2 up");
    ed_select_click(&lv, portal, false);
    for (int m = 1; m <= MODE_COUNT; m++) {
        CHECK(ed_prop_step(&lv, &H, ED_PROP_KIND, 1));
        CHECK((int)ed_find(&lv, portal)->obj.portal_mode == m % MODE_COUNT);
    }
    CHECK(ed_prop_step(&lv, &H, ED_PROP_KIND, -1));
    CHECK(prop_is(&lv, ED_PROP_KIND, MODES[MODE_COUNT - 1].name));
    ed_select_click(&lv, pad, false);
    do {
        ed_prop_step(&lv, &H, ED_PROP_KIND, 1);
        CHECK(LAUNCHES[ed_find(&lv, pad)->obj.launch].pad_v != 0.0);
        steps += 1;
    } while (ed_find(&lv, pad)->obj.launch != LAUNCH_YELLOW && steps < 20);
    CHECK(steps == 4);
    ed_select_click(&lv, orb, false);
    steps = 0;
    do {
        ed_prop_step(&lv, &H, ED_PROP_KIND, -1);
        steps += 1;
    } while (ed_find(&lv, orb)->obj.launch != LAUNCH_YELLOW && steps < 20);
    CHECK(steps == 6);
    ed_select_click(&lv, gravity, false);
    CHECK(ed_prop_step(&lv, &H, ED_PROP_KIND, 1) && prop_is(&lv, ED_PROP_KIND,
        "down"));
    ed_select_all(&lv);
    CHECK(ed_prop_step(&lv, &H, ED_PROP_KIND, 1));     /* each its own next */
    CHECK(saves_as(&lv, "name LEVEL 1\n\nportal 500 650 2 cube\n"
        "pad 700 750 2 pink\norb 900 650 2 pink\ngravity 1100 650 2 up\n"));
    CHECK(ed_undo(&lv, &H) && ed_find(&lv, gravity)->obj.portal_gravity == 1);
    ed_level_free(&lv);
}

static void test_start_props(void)
{
    const char *text = "start 900 300 cube\n";
    ed_level_t lv;
    ed_object_t *st;

    begin(&lv);
    ed_level_free(&lv);
    ed_level_from_text(&lv, text, strlen(text), "5");
    st = &lv.objects[0];
    ed_select_all(&lv);
    CHECK(ed_prop_step(&lv, &H, ED_PROP_KIND, 1) && st->start.mode == MODE_SHIP);
    CHECK(ed_prop_step(&lv, &H, ED_PROP_GRAVITY, 1));
    CHECK(st->start.gravity_dir == -1);
    CHECK(ed_prop_step(&lv, &H, ED_PROP_SPEED, -1));
    CHECK(st->start.speed == 0.5);
    CHECK(!ed_prop_step(&lv, &H, ED_PROP_SPEED, -1) && H.nb_undo == 3);
    for (int i = 0; i < 4; i++)
        CHECK(ed_prop_step(&lv, &H, ED_PROP_SPEED, 1));
    CHECK(st->start.speed == 4.0);
    CHECK(!ed_prop_step(&lv, &H, ED_PROP_SPEED, 1));
    CHECK(!ed_prop_step(&lv, &H, ED_PROP_TURN, 1));    /* nothing to turn */
    CHECK(st->start.pos.x == 900.0 && st->start.pos.y == 300.0);
    CHECK(saves_as(&lv, "name 5\nstart 900 300 ship up 4\n"));
    while (ed_undo(&lv, &H));
    CHECK(saves_as(&lv, "name 5\nstart 900 300 cube\n"));
    ed_level_free(&lv);
}

/* One object turns where it is; several turn as one piece, on the grid. */
static void test_rotate(void)
{
    ed_level_t lv;
    int a;
    int b;

    begin(&lv);
    a = add(&lv, "spike 0 0 2");
    b = add(&lv, "block 100 0 2");
    ed_select_click(&lv, a, false);
    CHECK(ed_rotate(&lv, &H, 90.0, 50.0));
    CHECK(ed_find(&lv, a)->obj.rotation == 90.0);
    CHECK(ed_find(&lv, a)->obj.rect.x == 0.0 && ed_find(&lv, a)->obj.rect.y == 0.0);
    CHECK(ed_find(&lv, a)->obj.hitbox.aabb.x > 0.0);   /* the spike's box, turned */
    CHECK(ed_rotate(&lv, &H, -90.0, 50.0));
    CHECK(ed_find(&lv, a)->obj.rotation == 0.0 && H.nb_undo == 4);
    ed_select_all(&lv);
    CHECK(ed_rotate(&lv, &H, 90.0, 50.0) && H.nb_undo == 5);
    CHECK(ed_find(&lv, a)->obj.rect.x == 50.0);        /* a row became a column */
    CHECK(ed_find(&lv, a)->obj.rect.y == -50.0);
    CHECK(ed_find(&lv, b)->obj.rect.x == 50.0);
    CHECK(ed_find(&lv, b)->obj.rect.y == 50.0);
    CHECK(ed_find(&lv, a)->obj.rotation == 90.0);
    for (int i = 0; i < 3; i++)
        ed_rotate(&lv, &H, 90.0, 50.0);
    CHECK(ed_find(&lv, a)->obj.rect.x == 0.0 && ed_find(&lv, a)->obj.rect.y == 0.0);
    CHECK(ed_find(&lv, b)->obj.rect.x == 100.0 && ed_find(&lv, b)->obj.rect.y == 0.0);
    CHECK(ed_find(&lv, a)->obj.rotation == 0.0);
    CHECK(ed_undo(&lv, &H) && ed_find(&lv, b)->obj.rect.y == -50.0);
    ed_select_none(&lv);
    CHECK(!ed_rotate(&lv, &H, 90.0, 50.0));
    ed_level_free(&lv);
}

static bool at(ed_level_t *lv, int id, double x, double y)
{
    const rect_t *r = &ed_find(lv, id)->obj.rect;

    return r->x == x && r->y == y && !signbit(r->x) && !signbit(r->y);
}

static void turn(ed_level_t *lv, double deg, int times)
{
    for (int i = 0; i < times; i++)
        ed_rotate(lv, &H, deg, 50.0);
}

/* A piece turns back to where it was, on the grid, its shape kept (11.4). */
static void test_rotate_back(void)
{
    ed_level_t lv;
    rect_t ra;
    rect_t rb;
    int a;
    int b;

    begin(&lv);
    a = add(&lv, "block 1000 600 2");          /* 3 half cells wide, 4 high */
    b = add(&lv, "block 1050 700 2");
    ed_select_all(&lv);
    turn(&lv, 90.0, 1);
    CHECK(at(&lv, a, 1100.0, 600.0) && at(&lv, b, 1000.0, 650.0));
    turn(&lv, -90.0, 1);
    CHECK(at(&lv, a, 1000.0, 600.0) && at(&lv, b, 1050.0, 700.0));
    turn(&lv, 90.0, 4);
    CHECK(at(&lv, a, 1000.0, 600.0) && at(&lv, b, 1050.0, 700.0));
    turn(&lv, -90.0, 4);
    CHECK(at(&lv, a, 1000.0, 600.0) && at(&lv, b, 1050.0, 700.0));
    turn(&lv, 180.0, 2);
    CHECK(at(&lv, a, 1000.0, 600.0) && at(&lv, b, 1050.0, 700.0));
    ed_level_free(&lv);
    begin(&lv);
    a = add(&lv, "block 0 0 2");               /* across y = 0 and x = 0 */
    b = add(&lv, "block 50 100 2");
    ed_select_all(&lv);
    turn(&lv, 90.0, 1);
    ra = object_drawn_bounds(&ed_find(&lv, a)->obj);
    rb = object_drawn_bounds(&ed_find(&lv, b)->obj);
    CHECK(rb.x + rb.w == ra.x && ra.y + 50.0 == rb.y);  /* no gap, no overlap */
    turn(&lv, -90.0, 1);
    CHECK(at(&lv, a, 0.0, 0.0) && at(&lv, b, 50.0, 100.0));
    turn(&lv, 90.0, 4);
    CHECK(at(&lv, a, 0.0, 0.0) && at(&lv, b, 50.0, 100.0));
    ed_level_free(&lv);
    begin(&lv);
    a = add(&lv, "block 1000 600 2 w=1");      /* 50 x 100 */
    b = add(&lv, "block 1050 600 2");          /* beside it */
    ed_select_all(&lv);
    turn(&lv, 90.0, 1);
    ra = object_drawn_bounds(&ed_find(&lv, a)->obj);
    rb = object_drawn_bounds(&ed_find(&lv, b)->obj);
    CHECK(ra.x == 1050.0 && ra.y == 550.0 && ra.w == 100.0 && ra.h == 50.0);
    CHECK(rb.x == 1050.0 && rb.y == 600.0);   /* a column: a on top of b */
    turn(&lv, -90.0, 1);
    CHECK(at(&lv, a, 1000.0, 600.0) && at(&lv, b, 1050.0, 600.0));
    ed_find(&lv, a)->obj.rect.x += 1.0;        /* off the grid by Shift+arrow */
    ed_refresh(ed_find(&lv, a));
    turn(&lv, 90.0, 1);
    ra = object_drawn_bounds(&ed_find(&lv, a)->obj);
    rb = object_drawn_bounds(&ed_find(&lv, b)->obj);
    CHECK(ra.x == rb.x && ra.y + ra.h == rb.y + 1.0);   /* the pixel turned */
    turn(&lv, -90.0, 1);
    CHECK(at(&lv, a, 1001.0, 600.0) && at(&lv, b, 1050.0, 600.0));
    ed_level_free(&lv);
}

static unsigned int rnd(unsigned int *seed, unsigned int n)
{
    *seed = *seed * 1664525u + 1013904223u;
    return (*seed >> 8) % n;
}

/* Every object of the piece as it is drawn, on the 50 px grid. */
static bool on_grid(const ed_level_t *lv)
{
    for (size_t i = 0; i < lv->count; i++) {
        rect_t r = object_drawn_bounds(&lv->objects[i].obj);

        if (fmod(r.x, 50.0) != 0.0 || fmod(r.y, 50.0) != 0.0)
            return false;
    }
    return true;
}

static bool same_rects(const ed_level_t *lv, const rect_t *was)
{
    for (size_t i = 0; i < lv->count; i++)
        if (lv->objects[i].obj.rect.x != was[i].x
            || lv->objects[i].obj.rect.y != was[i].y)
            return false;
    return true;
}

/* Random pieces of blocks of any size, some turned, on both sides of 0. */
static void test_rotate_random(void)
{
    static const double TURNS[] = {90.0, -90.0, 180.0};
    rect_t was[6];
    char line[96];
    ed_level_t lv;
    unsigned int seed = 7;
    bool grid;
    int on = 0;

    for (int run = 0; run < 300; run++) {
        int n = 2 + (int)rnd(&seed, 5);
        double deg = TURNS[rnd(&seed, 3)];

        begin(&lv);
        for (int i = 0; i < n; i++) {
            snprintf(line, sizeof(line), "block %d %d 2 w=%u h=%u rot=%u",
                ((int)rnd(&seed, 12) - 6) * 50, ((int)rnd(&seed, 12) - 6) * 50,
                1 + rnd(&seed, 4), 1 + rnd(&seed, 4), 90 * rnd(&seed, 4));
            add(&lv, line);
            was[i] = lv.objects[i].obj.rect;
        }
        ed_select_all(&lv);
        grid = on_grid(&lv);       /* a 50 x 100 turned a quarter isn't */
        turn(&lv, deg, 1);
        CHECK(on_grid(&lv) == grid);
        on += grid;
        turn(&lv, -deg, 1);
        CHECK(same_rects(&lv, was));
        turn(&lv, deg, deg == 180.0 ? 2 : 4);
        CHECK(same_rects(&lv, was));
        ed_level_free(&lv);
    }
    CHECK(on > 30);                    /* enough pieces were on the grid */
}

void test_props(void)
{
    test_start_objects();
    test_start_lines();
    test_applies();
    test_shape();
    test_kinds();
    test_start_props();
    test_rotate();
    test_rotate_back();
    test_rotate_random();
    ed_history_free(&H);
}
