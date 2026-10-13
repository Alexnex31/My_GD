/*
** ALEXNEX PROJECT, 2026
** tests/test_saw.c
** File description:
** saws: a disc that kills on any contact (FEATURES 10.6)
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor/ed_props.h"
#include "sim/bot.h"
#include "sim/internal.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

#define NONE ((input_t){false, false})
#define FAR "block 30000 750 2\n"             /* a level long enough  */

static void load(sim_t *s, const char *text)
{
    sim_load_mem(s, text, strlen(text), "1", NULL);
}

/* The disc: the rect's center, 0.42 of its width, whatever its turn. */
static void test_shape(void)
{
    sim_t s;
    const hitbox_t *h;

    load(&s, "saw 1500 750 2\nsaw 2500 700 2 rot=45\nsaw 3500 400 4\n"
        "saw 5000.5 300.25 2 w=3 h=1\n");
    CHECK(s.lvl.nb_objects == 4 && s.lvl.skipped_lines == 0);
    h = &s.lvl.objects[0].hitbox;
    CHECK(s.lvl.objects[0].type == OBJ_SAW);
    CHECK(OBJ_CATEGORY[OBJ_SAW] == CAT_HARM);
    CHECK(h->kind == SHAPE_CIRCLE && h->nverts == 0);
    CHECK(h->center.x == 1550.0 && h->center.y == 800.0 && h->radius == 42.0);
    CHECK(h->aabb.x == 1508.0 && h->aabb.y == 758.0);
    CHECK(h->aabb.w == 84.0 && h->aabb.h == 84.0);
    h = &s.lvl.objects[1].hitbox;                /* turned: the same disc */
    CHECK(h->kind == SHAPE_CIRCLE && h->center.x == 2550.0
        && h->center.y == 750.0 && h->radius == 42.0);
    h = &s.lvl.objects[2].hitbox;                /* size 4: 200 px wide */
    CHECK(h->center.x == 3600.0 && h->center.y == 500.0 && h->radius == 84.0);
    h = &s.lvl.objects[3].hitbox;                /* by its width, not its height */
    CHECK(h->center.x == 5075.5 && h->center.y == 325.25 && h->radius == 63.0);
    CHECK(s.lvl.reach >= 168.0);                 /* the broadphase knows it */
    sim_free(&s);
}

/* Ticks until the player dies; -1 if it reaches the end or `max` first. */
static long run_to_death(sim_t *s, long max, vec2_t *before)
{
    for (long t = 0; t < max && !s->st.complete; t++) {
        *before = s->st.player.pos;
        sim_tick(s, NONE);
        if (!s->st.player.alive)
            return t;
    }
    return -1;
}

/* Run into on the ground: death on the tick the square's side enters it. */
static void test_head_on(void)
{
    sim_t s;
    vec2_t before = {0.0, 0.0};
    double step = PER_TICK(SCROLL_SPEED);

    load(&s, "saw 1500 750 2\n" FAR);
    CHECK(run_to_death(&s, 2000, &before) > 0);
    CHECK(before.x + PLAYER_HALF <= 1508.0);             /* not a tick early */
    CHECK(before.x + PLAYER_HALF + step > 1508.0);       /* nor one late */
    sim_free(&s);
    load(&s, "start_speed 4\nsaw 3000 800 1\n" FAR);     /* 50 px wide, fast */
    CHECK(run_to_death(&s, 2000, &before) > 0);
    CHECK(before.x + PLAYER_HALF <= 3004.0);
    sim_free(&s);
}

/*
** A saw hanging over the ground. The cube's top is at 750: a disc whose
** lowest point is exactly there only touches, and touching isn't a contact
** (PLAN 3.0); a hair lower, it kills.
*/
static void test_passing_under(void)
{
    sim_t s;
    vec2_t before = {0.0, 0.0};

    load(&s, "saw 1500 658 2\n" FAR);            /* center 708, bottom 750 */
    CHECK(run_to_death(&s, 600, &before) == -1);
    CHECK(s.st.player.pos.x > 1700.0);
    sim_free(&s);
    load(&s, "saw 1500 658.5 2\n" FAR);
    CHECK(run_to_death(&s, 600, &before) > 0);
    CHECK(before.x < 1600.0);
    sim_free(&s);
}

/*
** Past the corner. The disc's center is 30 px above the cube's top and its
** x range crosses the cube's: their boxes overlap, but the disc only dips
** under y = 750 between x = cx - 29.4 and cx + 29.4, so the cube dies on its
** top side, well after the boxes met, and not at all once the disc is a
** radius above.
*/
static void test_round_not_square(void)
{
    sim_t s;
    vec2_t before = {0.0, 0.0};
    double reach = sqrt(42.0 * 42.0 - 30.0 * 30.0);      /* 29.39 */

    load(&s, "saw 1500 670 2\n" FAR);            /* center (1550, 720) */
    CHECK(run_to_death(&s, 600, &before) > 0);
    CHECK(before.x + PLAYER_HALF > 1508.0 + 5.0);        /* the box was passed */
    CHECK(before.x + PLAYER_HALF <= 1550.0 - reach);
    CHECK(before.x + PLAYER_HALF + PER_TICK(SCROLL_SPEED) > 1550.0 - reach);
    sim_free(&s);
}

static bool replays(sim_t *s, const bot_result_t *r)
{
    bool prev = false;

    sim_reset(s);
    for (long t = 0; t < r->ticks; t++) {
        sim_tick(s, (input_t){r->inputs[t], r->inputs[t] && !prev});
        prev = r->inputs[t];
        if (!s->st.player.alive)
            return false;
    }
    return s->st.complete;
}

/* One saw is jumped; a column of them is not; a ship threads two. */
static void test_saw_bot(void)
{
    sim_t s;
    bot_result_t r;

    load(&s, "saw 1500 750 2\nsaw 2600 750 2\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && r.attempts > 1 && replays(&s, &r));
    bot_result_free(&r);
    sim_free(&s);
    load(&s, "saw 1500 750 2\nsaw 1500 650 2\nsaw 1500 550 2\n"
        "saw 1500 450 2\nsaw 1500 350 2\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_NO_PATH && r.furthest < 100.0f);
    bot_result_free(&r);
    sim_free(&s);
    load(&s, "start_gamemode ship\nsaw 2000 600 4\nsaw 2000 0 4\n"
        "saw 3200 350 4\nsaw 3200 -250 4\n");
    r = bot_solve(&s, BOT_MAX_ATTEMPTS);
    CHECK(r.verdict == BOT_FOUND && r.attempts > 1 && replays(&s, &r));
    bot_result_free(&r);
    sim_free(&s);
}

static ed_entry_t *find_entry(ed_entry_t *e, int n, const char *label)
{
    for (int i = 0; i < n; i++)
        if (strcmp(e[i].label, label) == 0)
            return &e[i];
    return NULL;
}

/*
** In the editor: placed from the palette, saved as its line, and kept round,
** since its hitbox is a disc: the width and the height change together.
*/
static void test_saw_editor(void)
{
    ed_entry_t e[ED_MAX_ENTRIES];
    int n = ed_entries(e, ED_MAX_ENTRIES);
    ed_history_t h = {0};
    ed_level_t lv;
    ed_object_t *o;
    char *text;

    ed_level_new(&lv, "1");
    CHECK(find_entry(e, n, "saw") != NULL);
    CHECK(ed_place(&lv, find_entry(e, n, "saw"), (vec2_t){1520.0, 760.0},
        50.0));
    o = &lv.objects[0];
    CHECK(o->obj.type == OBJ_SAW && o->obj.hitbox.kind == SHAPE_CIRCLE);
    CHECK(o->obj.rect.x == 1500.0 && o->obj.hitbox.radius == 42.0);
    CHECK(ed_hit(&lv, (vec2_t){1505.0, 755.0}) == o->id);    /* by its rect */
    text = ed_level_text(&lv);
    CHECK(strstr(text, "saw 1500 750 2\n") != NULL);
    free(text);
    ed_select_all(&lv);
    CHECK(ed_prop_applies(&lv, ED_PROP_W) && !ed_prop_applies(&lv,
        ED_PROP_KIND));
    ed_gesture(&h);
    CHECK(ed_prop_step(&lv, &h, ED_PROP_W, 1));
    o = &lv.objects[0];
    CHECK(o->obj.rect.w == 150.0 && o->obj.rect.h == 150.0);
    CHECK(o->obj.hitbox.radius == 63.0 && o->obj.hitbox.center.x == 1575.0);
    ed_gesture(&h);
    CHECK(ed_prop_step(&lv, &h, ED_PROP_H, -1));
    o = &lv.objects[0];
    CHECK(o->obj.rect.w == 100.0 && o->obj.rect.h == 100.0);
    CHECK(ed_undo(&lv, &h) && lv.objects[0].obj.rect.h == 150.0
        && lv.objects[0].obj.hitbox.radius == 63.0);
    ed_history_free(&h);
    ed_level_free(&lv);
}

void test_saw(void)
{
    test_shape();
    test_head_on();
    test_passing_under();
    test_round_not_square();
    test_saw_bot();
    test_saw_editor();
}
