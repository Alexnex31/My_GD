/*
** ALEXNEX PROJECT, 2026
** tests/test_edit.c
** File description:
** the editor's edits: undo and redo, the selection, moving, copies (FEATURES 11.5 to 11.7)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor/ed_select.h"
#include "sim/alloc.h"
#include "test.h"

static ed_history_t H;        /* big: not on the stack */
static unsigned int SEED;

static unsigned int rnd(unsigned int n)
{
    SEED = SEED * 1664525u + 1013904223u;
    return (SEED >> 8) % n;
}

static int add(ed_level_t *lv, const char *line)
{
    object_t o;

    level_object_from_line(line, &o);
    return ed_do_add(lv, &H, &o, NULL);
}

/* The level as the editor holds it: ids, order, shapes, kept fields. */
static char *state(const ed_level_t *lv)
{
    size_t cap = 128 * (lv->count + 1);
    char *s = sim_xcalloc(cap, 1);
    size_t n = 0;

    for (size_t i = 0; i < lv->count; i++) {
        const ed_object_t *o = &lv->objects[i];

        n += (size_t)snprintf(s + n, cap - n, "%d %d %.17g %.17g %.17g "
            "%.17g %.17g %s\n", o->id, (int)o->obj.type, o->obj.rect.x,
            o->obj.rect.y, o->obj.rotation, o->obj.hitbox.aabb.x,
            o->obj.hitbox.aabb.y, o->extra != NULL ? o->extra : "");
    }
    return s;
}

static bool is_state(const ed_level_t *lv, const char *want)
{
    char *now = state(lv);
    bool same = strcmp(now, want) == 0;

    free(now);
    return same;
}

static void begin(ed_level_t *lv)
{
    ed_history_free(&H);
    H = (ed_history_t){0};
    ed_level_new(lv, "1");
}

/* Each kind of edit, taken back and done again. */
static void test_undo_redo(void)
{
    ed_level_t lv;
    char *s[4];
    int a;
    int b;
    ed_object_t before;

    begin(&lv);
    CHECK(!ed_undo(&lv, &H) && !ed_redo(&lv, &H));
    s[0] = state(&lv);
    ed_gesture(&H);
    a = add(&lv, "block 100 750 2");
    ed_gesture(&H);
    b = add(&lv, "spike 300 750 2 group=3");
    s[1] = state(&lv);
    ed_gesture(&H);
    CHECK(ed_do_remove(&lv, &H, a) && !ed_do_remove(&lv, &H, a));
    s[2] = state(&lv);
    ed_gesture(&H);
    before = *ed_find(&lv, b);
    ed_find(&lv, b)->obj.rect.x = 425.0;
    ed_do_change(&lv, &H, &b, &before.obj, 1);
    s[3] = state(&lv);
    CHECK(H.nb_undo == 4);
    CHECK(ed_undo(&lv, &H) && is_state(&lv, s[2]));
    CHECK(ed_undo(&lv, &H) && is_state(&lv, s[1]));    /* a is back, first */
    CHECK(lv.objects[0].id == a && lv.objects[1].id == b);
    CHECK(ed_undo(&lv, &H) && ed_undo(&lv, &H) && is_state(&lv, s[0]));
    CHECK(!ed_undo(&lv, &H) && lv.count == 0);
    CHECK(ed_redo(&lv, &H) && ed_redo(&lv, &H) && is_state(&lv, s[1]));
    CHECK(ed_find(&lv, b) != NULL && ed_find(&lv, a) != NULL);  /* same ids */
    CHECK(ed_redo(&lv, &H) && ed_redo(&lv, &H) && is_state(&lv, s[3]));
    CHECK(!ed_redo(&lv, &H));
    CHECK(add(&lv, "block 900 750 2") > b);            /* never an old id */
    for (int i = 0; i < 4; i++)
        free(s[i]);
    ed_level_free(&lv);
}

/* A stroke is one step; a new edit forgets what was undone; 256 are kept. */
static void test_gestures(void)
{
    ed_entry_t e[ED_MAX_ENTRIES];
    ed_level_t lv;

    ed_entries(e, ED_MAX_ENTRIES);
    begin(&lv);
    ed_gesture(&H);
    for (int i = 0; i < 5; i++)
        CHECK(ed_do_place(&lv, &H, &e[0], (vec2_t){100.0 * i, 700.0}, 50.0));
    CHECK(!ed_do_place(&lv, &H, &e[0], (vec2_t){110.0, 710.0}, 50.0));
    CHECK(lv.count == 5 && H.nb_undo == 1);
    ed_gesture(&H);
    for (int i = 0; i < 3; i++)
        ed_do_remove(&lv, &H, lv.objects[0].id);
    CHECK(lv.count == 2 && H.nb_undo == 2);
    CHECK(ed_undo(&lv, &H) && lv.count == 5);
    CHECK(lv.objects[0].obj.rect.x == 0.0 && lv.objects[4].obj.rect.x == 400.0);
    CHECK(ed_undo(&lv, &H) && lv.count == 0 && H.nb_redo == 2);
    ed_gesture(&H);
    add(&lv, "block 0 0 2");
    CHECK(H.nb_redo == 0 && !ed_redo(&lv, &H) && lv.count == 1);
    for (int i = 0; i < ED_HISTORY_MAX + 40; i++) {
        ed_gesture(&H);
        add(&lv, "spike 0 0 2");
    }
    CHECK(H.nb_undo == ED_HISTORY_MAX);
    while (ed_undo(&lv, &H));
    CHECK(lv.count == 41 && H.nb_redo == ED_HISTORY_MAX);
    ed_level_free(&lv);
}

static void random_edit(ed_level_t *lv, const ed_entry_t *e, int n)
{
    unsigned int what = rnd(10);
    vec2_t at = {(double)rnd(40) * 25.0, 400.0 + (double)rnd(16) * 25.0};

    if (rnd(4) != 0)
        ed_gesture(&H);
    if (what < 4 || lv->count == 0)
        ed_do_place(lv, &H, &e[rnd((unsigned int)n)], at, 25.0);
    else if (what < 6)
        ed_do_remove(lv, &H, lv->objects[rnd((unsigned int)lv->count)].id);
    else {
        ed_select_none(lv);
        for (unsigned int k = rnd(4) + 1; k > 0; k--)
            lv->objects[rnd((unsigned int)lv->count)].selected = true;
    }
    if (what == 6)
        ed_nudge(lv, &H, (double)rnd(9) * 12.5 - 50.0, (double)rnd(5) - 2.0);
    if (what == 7)
        ed_delete_selected(lv, &H);
    if (what == 8)
        ed_duplicate(lv, &H, 50.0);
    if (what == 9) {
        ed_clip_t clip = {0};

        ed_copy(lv, &clip);
        ed_paste(lv, &H, &clip, at, 50.0);
        ed_clip_free(&clip);
    }
}

/*
** FEATURES 11.7's test: random edits, then every undo gives exactly the
** level there was at that depth, down to the start, and every redo too.
*/
static void random_run(unsigned int seed)
{
    char *snap[ED_HISTORY_MAX + 1] = {NULL};
    ed_entry_t e[ED_MAX_ENTRIES];
    int n = ed_entries(e, ED_MAX_ENTRIES);
    ed_level_t lv;
    bool ok = true;

    SEED = seed;
    begin(&lv);
    add(&lv, "block 0 750 2 group=7");
    snap[1] = state(&lv);
    snap[0] = sim_xstrdup("");
    for (int i = 0; i < 400 && H.nb_undo < ED_HISTORY_MAX; i++) {
        random_edit(&lv, e, n);
        free(snap[H.nb_undo]);
        snap[H.nb_undo] = state(&lv);
    }
    CHECK(H.nb_undo > 100);
    while (ed_undo(&lv, &H))
        ok = ok && is_state(&lv, snap[H.nb_undo]);
    CHECK(ok && lv.count == 0);
    while (ed_redo(&lv, &H))
        ok = ok && is_state(&lv, snap[H.nb_undo]);
    CHECK(ok && lv.count > 0);
    for (size_t i = 0; i <= ED_HISTORY_MAX; i++)
        free(snap[i]);
    ed_level_free(&lv);
}

static void test_random(void)
{
    for (unsigned int seed = 1; seed <= 6; seed++)
        random_run(seed);
}

static void test_select(void)
{
    ed_level_t lv;
    int a;
    int b;
    int c;

    begin(&lv);
    a = add(&lv, "block 100 700 2");
    b = add(&lv, "spike 300 700 2");
    c = add(&lv, "block 1000 700 2 w=8 h=1 rot=90");   /* drawn 1175..1225 */
    CHECK(ed_selected(&lv) == 0);
    ed_select_click(&lv, a, false);
    ed_select_click(&lv, b, false);
    CHECK(ed_selected(&lv) == 1 && ed_find(&lv, b)->selected);
    ed_select_click(&lv, a, true);
    CHECK(ed_selected(&lv) == 2);
    ed_select_click(&lv, a, true);                     /* shift: toggles */
    ed_select_click(&lv, -1, true);                    /* on nothing: kept */
    CHECK(ed_selected(&lv) == 1 && !ed_find(&lv, a)->selected);
    ed_select_click(&lv, -1, false);
    CHECK(ed_selected(&lv) == 0);
    ed_select_box(&lv, (rect_t){150.0, 650.0, 200.0, 100.0}, false);
    CHECK(ed_selected(&lv) == 2 && !ed_find(&lv, c)->selected);
    ed_select_box(&lv, (rect_t){1000.0, 700.0, 100.0, 50.0}, false);
    CHECK(ed_selected(&lv) == 0);            /* its rect, not where it's drawn */
    ed_select_box(&lv, (rect_t){1180.0, 600.0, 10.0, 10.0}, false);
    CHECK(ed_selected(&lv) == 1 && ed_find(&lv, c)->selected);
    ed_select_box(&lv, (rect_t){0.0, 0.0, 250.0, 2000.0}, true);
    CHECK(ed_selected(&lv) == 2 && ed_find(&lv, a)->selected);
    ed_select_all(&lv);
    CHECK(ed_selected(&lv) == 3);
    ed_select_click(&lv, b, true);
    ed_gesture(&H);
    CHECK(ed_delete_selected(&lv, &H) && lv.count == 1 && H.nb_undo == 2);
    CHECK(!ed_delete_selected(&lv, &H));
    CHECK(ed_undo(&lv, &H) && lv.count == 3);          /* both, in one step */
    CHECK(lv.objects[0].id == a && lv.objects[2].id == c);
    CHECK(ed_selected(&lv) == 0);
    ed_level_free(&lv);
}

/* A drag: from where it was grabbed, the travel snapped, one command. */
static void test_move(void)
{
    ed_level_t lv;
    ed_grab_t g;
    int a;
    int b;
    int c;

    CHECK(ed_snap_delta(24.0, 50.0) == 0.0 && ed_snap_delta(26.0, 50.0) == 50.0);
    CHECK(ed_snap_delta(-26.0, 50.0) == -50.0);
    CHECK(ed_snap_delta(-24.0, 50.0) == 0.0 && ed_snap_delta(140.0, 25.0) == 150.0);
    begin(&lv);
    a = add(&lv, "block 100 700 2");
    b = add(&lv, "spike 312.5 700 2");                 /* off the grid */
    c = add(&lv, "block 900 700 2");
    ed_select_click(&lv, a, false);
    ed_select_click(&lv, b, true);
    ed_gesture(&H);
    ed_grab(&lv, &g);
    CHECK(g.count == 2);
    ed_grab_move(&lv, &g, 50.0, 0.0);
    ed_grab_move(&lv, &g, 150.0, -50.0);               /* not added up */
    CHECK(ed_find(&lv, a)->obj.rect.x == 250.0);
    CHECK(ed_find(&lv, b)->obj.rect.x == 462.5);       /* its offset kept */
    CHECK(ed_find(&lv, b)->obj.rect.y == 650.0);
    CHECK(ed_find(&lv, a)->obj.hitbox.aabb.x == 250.0);
    CHECK(ed_find(&lv, c)->obj.rect.x == 900.0);
    CHECK(ed_hit(&lv, (vec2_t){260.0, 660.0}) == a);
    CHECK(ed_grab_drop(&lv, &H, &g) && H.nb_undo == 2 && g.ids == NULL);
    CHECK(ed_undo(&lv, &H));
    CHECK(ed_find(&lv, a)->obj.rect.x == 100.0);
    CHECK(ed_find(&lv, b)->obj.hitbox.aabb.y > 700.0);
    CHECK(ed_redo(&lv, &H) && ed_find(&lv, b)->obj.rect.x == 462.5);
    ed_grab(&lv, &g);
    ed_grab_move(&lv, &g, 500.0, 500.0);
    ed_grab_cancel(&lv, &g);
    CHECK(ed_find(&lv, a)->obj.rect.x == 250.0 && H.nb_undo == 2);
    ed_grab(&lv, &g);
    ed_grab_move(&lv, &g, 50.0, 0.0);
    ed_grab_move(&lv, &g, 0.0, 0.0);
    CHECK(!ed_grab_drop(&lv, &H, &g) && H.nb_undo == 2);   /* back: no step */
    CHECK(ed_nudge(&lv, &H, 0.0, 1.0) && H.nb_undo == 3);
    CHECK(ed_find(&lv, a)->obj.rect.y == 651.0);
    ed_select_none(&lv);
    CHECK(!ed_nudge(&lv, &H, 50.0, 0.0) && H.nb_undo == 3);
    ed_level_free(&lv);
}

static void test_clipboard(void)
{
    ed_clip_t clip = {0};
    ed_level_t lv;
    ed_level_t other;
    object_t o;
    char *text;
    int a;

    begin(&lv);
    CHECK(!ed_copy(&lv, &clip) && !ed_paste(&lv, &H, &clip,
        (vec2_t){0.0, 0.0}, 50.0));
    a = add(&lv, "block 100 700 2");
    level_object_from_line("orb 325 600 2 green", &o);
    ed_do_add(&lv, &H, &o, " group=9");
    add(&lv, "block 900 700 2");
    ed_select_box(&lv, (rect_t){0.0, 0.0, 500.0, 2000.0}, false);
    CHECK(ed_copy(&lv, &clip) && clip.count == 2);
    CHECK(clip.corner.x == 100.0 && clip.corner.y == 600.0);
    ed_select_none(&lv);
    CHECK(!ed_copy(&lv, &clip) && clip.count == 2);    /* kept */
    ed_gesture(&H);
    CHECK(ed_paste(&lv, &H, &clip, (vec2_t){2030.0, 420.0}, 50.0));
    CHECK(lv.count == 5 && H.nb_undo == 2 && ed_selected(&lv) == 2);
    CHECK(lv.objects[3].selected && lv.objects[3].obj.rect.x == 2000.0);
    CHECK(lv.objects[3].obj.rect.y == 500.0 && lv.objects[3].id != a);
    CHECK(lv.objects[4].obj.rect.x == 2225.0);         /* 25 off, as it was */
    CHECK(lv.objects[4].obj.rect.y == 400.0);
    CHECK(lv.objects[4].obj.hitbox.aabb.x > 2225.0);
    CHECK(strcmp(lv.objects[4].extra, " group=9") == 0);
    ed_gesture(&H);
    CHECK(ed_duplicate(&lv, &H, 50.0) && lv.count == 7);
    CHECK(ed_selected(&lv) == 2 && lv.objects[5].selected);
    CHECK(lv.objects[5].obj.rect.x == 2050.0 && lv.objects[5].obj.rect.y == 500.0);
    CHECK(ed_undo(&lv, &H) && lv.count == 5);          /* one step each */
    CHECK(ed_undo(&lv, &H) && lv.count == 3);
    ed_select_none(&lv);
    CHECK(!ed_duplicate(&lv, &H, 50.0));
    ed_select_click(&lv, lv.objects[1].id, false);     /* the orb alone */
    CHECK(ed_copy(&lv, &clip) && clip.count == 1 && clip.corner.x == 325.0);
    CHECK(ed_paste(&lv, &H, &clip, (vec2_t){1010.0, 10.0}, 50.0));
    CHECK(lv.objects[3].obj.rect.x == 1025.0);         /* still 25 off */
    CHECK(lv.objects[3].obj.rect.y == 0.0);
    ed_level_new(&other, "2");                         /* into another level */
    CHECK(ed_paste(&other, &H, &clip, (vec2_t){0.0, 0.0}, 50.0));
    text = ed_level_text(&other);
    CHECK(strcmp(text, "name LEVEL 2\n\n"
        "orb 25 0 2 green group=9\n") == 0);
    free(text);
    ed_level_free(&other);
    ed_clip_free(&clip);
    ed_level_free(&lv);
}

void test_edit(void)
{
    test_undo_redo();
    test_gestures();
    test_random();
    test_select();
    test_move();
    test_clipboard();
    ed_history_free(&H);
}
