/*
** ALEXNEX PROJECT, 2026
** tests/test_editor.c
** File description:
** the editor's level: ids, open and save, snapping, what a click hits (FEATURES 11)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor/ed_level.h"
#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

static const ed_entry_t *find_entry(const ed_entry_t *e, int n,
    const char *label)
{
    for (int i = 0; i < n; i++)
        if (strcmp(e[i].label, label) == 0)
            return &e[i];
    return NULL;
}

/* A new level: its id, a name, the defaults, nothing to save yet. */
static void test_new(void)
{
    ed_level_t lv;
    char *text;

    ed_level_new(&lv, "13");
    CHECK(strcmp(lv.id, "13") == 0 && strcmp(lv.hdr.name, "LEVEL 13") == 0);
    CHECK(lv.count == 0 && !lv.dirty && lv.hdr.start.mode == MODE_CUBE);
    text = ed_level_text(&lv);
    CHECK(strcmp(text, "name LEVEL 13\n") == 0);
    free(text);
    ed_level_free(&lv);
}

/* Ids are never given twice, and still find their object after a removal. */
static void test_ids(void)
{
    ed_level_t lv;
    object_t o;
    int a;
    int b;
    int c;

    ed_level_new(&lv, "1");
    level_object_from_line("block 100 750 2", &o);
    a = ed_add(&lv, &o, NULL);
    level_object_from_line("spike 200 750 2", &o);
    b = ed_add(&lv, &o, " group=2");
    level_object_from_line("block 300 750 2", &o);
    c = ed_add(&lv, &o, NULL);
    CHECK(a != b && b != c && lv.count == 3 && lv.dirty);
    CHECK(ed_find(&lv, b)->obj.type == OBJ_SPIKE);
    CHECK(strcmp(ed_find(&lv, b)->extra, " group=2") == 0);
    CHECK(ed_remove(&lv, a) && lv.count == 2);
    CHECK(ed_find(&lv, a) == NULL && !ed_remove(&lv, a));
    CHECK(ed_find(&lv, b)->obj.rect.x == 200.0);       /* it moved in the array */
    CHECK(ed_find(&lv, c)->obj.rect.x == 300.0);
    CHECK(ed_add(&lv, &o, NULL) > c);                  /* never a's id again */
    ed_level_free(&lv);
}

/* Opened and saved untouched, a level is the text the writer gives it. */
static void test_open_and_save(void)
{
    const char *text = "name Mine\nstart 900 300 ship up\n\n"
        "block 100 750 2 group=4\nportal 500 650 2 ufo\npad 700 750 2 red\n";
    ed_level_t lv;
    ed_level_t back;
    char *saved;

    ed_level_from_text(&lv, text, strlen(text), "5");
    CHECK(lv.count == 3 && lv.nb_starts == 1 && !lv.dirty);
    CHECK(strcmp(lv.hdr.name, "Mine") == 0);
    saved = ed_level_text(&lv);
    CHECK(strcmp(saved, text) == 0);
    free(saved);
    ed_remove(&lv, lv.objects[1].id);
    CHECK(lv.dirty);
    CHECK(ed_level_save(&lv, "/tmp/990077.gd") == 0 && !lv.dirty);
    CHECK(ed_level_open(&back, "/tmp/990077.gd") == 0);
    CHECK(back.count == 2 && back.nb_starts == 1 && !back.dirty);
    CHECK(strcmp(back.id, "990077") == 0);
    ed_level_free(&back);
    CHECK(ed_level_open(&back, "/tmp/990078.gd") != 0);       /* no such file */
    CHECK(ed_level_open(&back, "/tmp/not_digits.gd") != 0);   /* not an id */
    remove("/tmp/990077.gd");
    ed_level_free(&lv);
}

/* The cell under the mouse: floor, on both sides of zero. */
static void test_snap(void)
{
    CHECK(ed_snap(0.0, 50.0) == 0.0 && ed_snap(49.9, 50.0) == 0.0);
    CHECK(ed_snap(50.0, 50.0) == 50.0 && ed_snap(1234.0, 50.0) == 1200.0);
    CHECK(ed_snap(-0.1, 50.0) == -50.0 && ed_snap(-50.0, 50.0) == -50.0);
    CHECK(ed_snap(1234.0, 25.0) == 1225.0);
}

/* A click hits what is drawn on top, in the object's own turned frame. */
static void test_hit(void)
{
    ed_level_t lv;
    object_t o;
    int block;
    int spike;
    int tilted;

    ed_level_new(&lv, "1");
    level_object_from_line("block 100 700 2", &o);
    block = ed_add(&lv, &o, NULL);
    level_object_from_line("spike 100 700 2", &o);
    spike = ed_add(&lv, &o, NULL);
    level_object_from_line("block 1000 700 2 w=8 h=1 rot=45", &o);
    tilted = ed_add(&lv, &o, NULL);
    CHECK(ed_hit(&lv, (vec2_t){150.0, 750.0}) == spike);   /* hazards on top */
    CHECK(ed_hit(&lv, (vec2_t){50.0, 750.0}) == -1);
    CHECK(ed_hit(&lv, (vec2_t){100.0, 700.0}) == spike);   /* the edge counts */
    CHECK(ed_hit(&lv, (vec2_t){200.0, 750.0}) == -1);      /* the far one doesn't */
    ed_remove(&lv, spike);
    CHECK(ed_hit(&lv, (vec2_t){150.0, 750.0}) == block);
    CHECK(ed_hit(&lv, (vec2_t){1200.0, 725.0}) == tilted); /* its center */
    CHECK(ed_hit(&lv, (vec2_t){1010.0, 725.0}) == -1);     /* its unturned end */
    CHECK(ed_hit(&lv, (vec2_t){1300.0, 825.0}) == tilted); /* along the 45 deg */
    ed_level_free(&lv);
}

/* The palette is the tables: every type, every mode, every colour. */
static void test_entries(void)
{
    ed_entry_t e[ED_MAX_ENTRIES];
    int n = ed_entries(e, ED_MAX_ENTRIES);
    int pads = 0;
    int orbs = 0;
    object_t o;
    char line[96];

    CHECK(n == 3 + MODE_COUNT + 2 + 4 + 6);
    for (int m = 0; m < MODE_COUNT; m++) {
        snprintf(line, sizeof(line), "portal %s", MODES[m].name);
        CHECK(find_entry(e, n, line) != NULL);
    }
    for (int i = 0; i < n; i++) {
        pads += e[i].type == OBJ_PAD;
        orbs += e[i].type == OBJ_ORB;
        snprintf(line, sizeof(line), "%s 0 0 2%s%s", obj_type_name(e[i].type),
            e[i].word != NULL ? " " : "", e[i].word != NULL ? e[i].word : "");
        CHECK(level_object_from_line(line, &o) == 0);  /* each one loads */
    }
    CHECK(pads == 4 && orbs == 6);
    CHECK(find_entry(e, n, "pad green") == NULL);
    CHECK(find_entry(e, n, "gravity up") != NULL);
    CHECK(ed_entries(e, 2) == 2);
}

/* Placing: the loader's own object, in the cell clicked, never twice. */
static void test_place(void)
{
    ed_entry_t e[ED_MAX_ENTRIES];
    int n = ed_entries(e, ED_MAX_ENTRIES);
    ed_level_t lv;
    object_t same;
    char *text;

    ed_level_new(&lv, "9");
    CHECK(ed_place(&lv, find_entry(e, n, "block"),
        (vec2_t){1234.0, 777.0}, 50.0));
    CHECK(lv.count == 1 && lv.dirty);
    CHECK(lv.objects[0].obj.rect.x == 1200.0);
    CHECK(lv.objects[0].obj.rect.y == 750.0);
    CHECK(!ed_place(&lv, find_entry(e, n, "block"),
        (vec2_t){1201.0, 751.0}, 50.0));               /* the same cell */
    CHECK(ed_place(&lv, find_entry(e, n, "spike"),
        (vec2_t){1201.0, 751.0}, 50.0));               /* another object */
    CHECK(ed_place(&lv, find_entry(e, n, "portal ship"),
        (vec2_t){2010.0, 660.0}, 50.0));
    CHECK(ed_place(&lv, find_entry(e, n, "orb green"),
        (vec2_t){-30.0, 510.0}, 25.0));
    level_object_from_line("portal 2000 650 2 ship", &same);
    CHECK(lv.objects[2].obj.rect.x == same.rect.x);
    CHECK(lv.objects[2].obj.rect.h == same.rect.h);
    CHECK(lv.objects[2].obj.hitbox.aabb.y == same.hitbox.aabb.y);
    text = ed_level_text(&lv);
    CHECK(strcmp(text, "name LEVEL 9\n\norb -50 500 2 green\n"
        "block 1200 750 2\nspike 1200 750 2\nportal 2000 650 2 ship\n") == 0);
    free(text);
    ed_level_free(&lv);
}

/* What the editor saves, the game plays: a document, then its file. */
static void test_plays(void)
{
    ed_entry_t e[ED_MAX_ENTRIES];
    int n = ed_entries(e, ED_MAX_ENTRIES);
    ed_level_t lv;
    level_doc_t doc;
    sim_t s;

    ed_level_new(&lv, "9");
    ed_place(&lv, find_entry(e, n, "spike"), (vec2_t){1500.0, 760.0}, 50.0);
    ed_level_doc(&lv, &doc);
    sim_init(&s, &doc, &lv.hdr, lv.id);
    ed_doc_free(&doc);
    CHECK(s.lvl.nb_objects == 1);
    for (int i = 0; i < 600 && s.st.player.alive; i++)
        sim_tick(&s, (input_t){false, false});
    CHECK(!s.st.player.alive && s.st.player.pos.x > 1400.0);   /* the spike */
    sim_free(&s);
    ed_level_free(&lv);
}

static void test_next_id(void)
{
    static const char *const ids[] = {"1", "12", "7", "10280"};
    char out[LEVEL_ID_MAX + 1];

    ed_next_id(ids, 3, out, sizeof(out));
    CHECK(strcmp(out, "13") == 0);
    ed_next_id(ids, 4, out, sizeof(out));
    CHECK(strcmp(out, "10281") == 0);
    ed_next_id(NULL, 0, out, sizeof(out));
    CHECK(strcmp(out, "1") == 0);
}

void test_editor(void)
{
    test_new();
    test_ids();
    test_open_and_save();
    test_snap();
    test_hit();
    test_entries();
    test_place();
    test_plays();
    test_next_id();
}
