/*
** ALEXNEX PROJECT, 2026
** tests/test_draw_order.c
** File description:
** what the renderer draws, in what order, and what flashes (9.2, 11.15)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fx/draw_order.h"
#include "sim/hitbox.h"
#include "sim/sim.h"
#include "test.h"

#define VIEW 1920.0

static unsigned long long g_rng = 88172645463325252ull;

static unsigned long long rnd(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return g_rng;
}

/*
** Blocks, spikes, saws, slopes, orbs and pads anywhere, some far, some
** wide, at any turn, Z layer and Z order.
*/
static size_t random_level(char *buf, size_t size, int n)
{
    static const char *const TYPES[] = {"block", "spike", "saw", "slope",
        "orb", "pad"};
    static const char *const LAYERS[] = {"b5", "b4", "b3", "b2", "b1", "t1",
        "t2", "t3", "t4"};
    size_t len = (size_t)snprintf(buf, size, "name t\n");

    for (int i = 0; i < n; i++) {
        int t = (int)(rnd() % 6);
        long x = (long)(rnd() % 20000) - 3000;

        if (rnd() % 50 == 0)
            x = (long)(rnd() % 9000000);
        len += (size_t)snprintf(buf + len, size - len,
            "%s %ld %ld 1%s w=%d rot=%d zl=%s z=%d\n", TYPES[t], x,
            (long)(rnd() % 800), t >= 4 ? " yellow" : "",
            1 + (int)(rnd() % (rnd() % 10 == 0 ? 60 : 3)), (int)(rnd() % 360),
            LAYERS[rnd() % 9], (int)(rnd() % 5) - 2);
    }
    return len;
}

/* Everything whose drawn bounds meet the view is gathered, in order. */
static void check_view(const draw_index_t *d, const level_data_t *lvl,
    double cam)
{
    size_t want = 0;
    size_t have = 0;

    for (size_t i = 0; i < lvl->nb_objects; i++) {
        rect_t b = object_drawn_bounds(&lvl->objects[i]);

        want += b.x + b.w >= cam && b.x <= cam + VIEW;
    }
    for (size_t k = 0; k < d->nb_visible; k++) {
        const object_t *o = &lvl->objects[d->by_rank[d->visible[k]]];
        rect_t b = object_drawn_bounds(o);

        have += b.x + b.w >= cam && b.x <= cam + VIEW;
        CHECK((o->z_layer < Z_T1) == (k < d->nb_behind));
        if (k > 0) {
            const object_t *p = &lvl->objects[d->by_rank[d->visible[k - 1]]];

            CHECK(object_draw_cmp(p, p->line, o, o->line) < 0);
        }
    }
    CHECK(have == want);
}

/*
** Random levels seen from random places: nothing in view is missed, the
** order is the draw order (never the array's, sorted by x), and the ones
** behind the player come first. An empty level and a view far away too.
*/
static void test_random_views(void)
{
    static char buf[1 << 16];
    sim_t s;
    draw_index_t d;

    for (int round = 0; round < 60; round++) {
        size_t len = random_level(buf, sizeof(buf),
            round == 0 ? 0 : (int)(rnd() % 400));

        sim_load_mem(&s, buf, len, "1", NULL);
        draw_index_build(&d, s.lvl.objects, s.lvl.nb_objects);
        for (int c = 0; c < 20; c++) {
            double cam = c == 0 ? -1e6 : (double)((long)(rnd() % 26000) - 6000);

            draw_index_gather(&d, s.lvl.objects, cam, cam + VIEW);
            check_view(&d, &s.lvl, cam);
        }
        draw_index_free(&d);
        sim_free(&s);
    }
}

/* Ties go to the one placed last, whatever x sorted them to (11.15). */
static void test_placement_wins(void)
{
    const char *text = "orb 900 600 2 yellow\nblock 100 600 2\n"
        "spike 500 600 2 zl=b1\nblock 300 600 2 z=1\n";
    sim_t s;
    draw_index_t d;

    sim_load_mem(&s, text, strlen(text), "1", NULL);
    draw_index_build(&d, s.lvl.objects, s.lvl.nb_objects);
    draw_index_gather(&d, s.lvl.objects, 0.0, VIEW);
    CHECK(d.nb_visible == 4 && d.nb_behind == 1);
    CHECK(s.lvl.objects[d.by_rank[d.visible[0]]].type == OBJ_SPIKE);
    CHECK(s.lvl.objects[d.by_rank[d.visible[1]]].type == OBJ_ORB);
    CHECK(s.lvl.objects[d.by_rank[d.visible[2]]].rect.x == 100.0);
    CHECK(s.lvl.objects[d.by_rank[d.visible[3]]].rect.x == 300.0);
    draw_index_free(&d);
    sim_free(&s);
}

/*
** A start position marks what is behind it spent: never fired, so no
** flash. What fires after does, once.
*/
static void test_flashes(void)
{
    const char *text = "orb 600 600 2 yellow\npad 800 740 2 yellow\n"
        "start 3000 650 cube\nblock 30000 750 2\n";
    sim_t s;
    uint64_t seen[2] = {0};
    size_t out[4];

    sim_load_mem(&s, text, strlen(text), "1", NULL);
    sim_reset_at(&s, &s.lvl.starts[0]);
    CHECK(s.st.spent[0] != 0);
    CHECK(flash_fresh(seen, &s.st, out, 4) == 2);   /* as it was: all flash */
    memset(seen, 0, sizeof(seen));
    flash_forget(seen, &s.st);
    CHECK(flash_fresh(seen, &s.st, out, 4) == 0);
    s.st.spent[0] |= (uint64_t)1 << 2;              /* the block, say, fires */
    CHECK(flash_fresh(seen, &s.st, out, 4) == 1 && out[0] == 2);
    CHECK(flash_fresh(seen, &s.st, out, 4) == 0);
    sim_free(&s);
}

void test_draw_order(void)
{
    test_random_views();
    test_placement_wins();
    test_flashes();
}
