/*
** ALEXNEX PROJECT, 2026
** tests/test_options.c
** File description:
** the options screen's rules: rebinding, window sizes, progress reset (FEATURES 5)
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sim/progress.h"
#include "test.h"
#include "ui/options_rules.h"

static const binding_t SPACE = {BIND_KEY, KEY_Space};
static const binding_t UP = {BIND_KEY, KEY_Up};
static const binding_t W = {BIND_KEY, KEY_W};
static const binding_t R = {BIND_KEY, KEY_R};
static const binding_t NONE = {BIND_NONE, 0};

static void defaults(settings_t *s, binding_t slots[NB_SLOTS])
{
    settings_defaults(s);
    slots_from_settings(s, slots);
}

static void test_slots_round_trip(void)
{
    settings_t s;
    binding_t slots[NB_SLOTS];

    defaults(&s, slots);
    CHECK(binding_equal(slots[0], SPACE) && binding_equal(slots[1], UP));
    CHECK(slots[4].kind == BIND_NONE && slots[5].kind == BIND_NONE);
    CHECK(binding_equal(slots[SLOT_RESTART], R));
    slots[1] = NONE;
    slots[5] = W;                            /* a gap: packed on the way back */
    slots_to_settings(&s, slots);
    CHECK(s.jump_bindings.count == 4);
    CHECK(binding_equal(s.jump_bindings.items[1], (binding_t){BIND_MOUSE,
        MOUSE_Left}));
    CHECK(binding_equal(s.jump_bindings.items[3], W));
}

/* 5.5: assign, swap, clear, and never leave nothing to jump with. */
static void test_rebind(void)
{
    settings_t s;
    binding_t slots[NB_SLOTS];
    int other = 0;

    defaults(&s, slots);
    CHECK(rebind(slots, 4, W, &other) == REBIND_SET && other == -1);
    CHECK(binding_equal(slots[4], W));
    CHECK(rebind(slots, 4, W, &other) == REBIND_SAME);
    CHECK(rebind(slots, SLOT_RESTART, SPACE, &other) == REBIND_SWAPPED);
    CHECK(other == 0);
    CHECK(binding_equal(slots[SLOT_RESTART], SPACE));
    CHECK(binding_equal(slots[0], R));        /* R moved to Jump 1 */
    CHECK(rebind(slots, 1, NONE, &other) == REBIND_CLEARED);
    CHECK(slots[1].kind == BIND_NONE);
    for (int i = 0; i < BINDINGS_MAX; i++)
        slots[i] = NONE;
    slots[2] = UP;
    CHECK(rebind(slots, 2, NONE, &other) == REBIND_REFUSED);   /* the last */
    CHECK(binding_equal(slots[2], UP));
    slots[SLOT_CHECKPOINT] = NONE;
    CHECK(rebind(slots, SLOT_CHECKPOINT, UP, &other) == REBIND_REFUSED);
    CHECK(binding_equal(slots[2], UP) && other == -1);    /* nothing moved */
    CHECK(rebind(slots, 3, UP, &other) == REBIND_SWAPPED);    /* jump to jump */
    CHECK(binding_equal(slots[3], UP) && slots[2].kind == BIND_NONE);
}

static void test_slot_names(void)
{
    char name[32];

    slot_name(1, name, sizeof(name));
    CHECK(strcmp(name, "Jump 2") == 0);
    slot_name(SLOT_RESTART, name, sizeof(name));
    CHECK(strcmp(name, "Restart") == 0);
    slot_name(SLOT_REMOVE_CHECKPOINT, name, sizeof(name));
    CHECK(strcmp(name, "Remove checkpoint") == 0);
}

/* Only the sizes that fit; a size from the file is offered too (5.2). */
static void test_window_sizes(void)
{
    window_sizes_t ws;

    window_sizes_list(&ws, 1920, 1080, 1280, 720);
    CHECK(ws.count == 3 && ws.current == 0);
    CHECK(strcmp(ws.labels[2], "1920x1080") == 0);
    window_sizes_list(&ws, 1600, 900, 1600, 900);
    CHECK(ws.count == 2 && ws.current == 1);
    window_sizes_list(&ws, 2560, 1440, 1000, 700);
    CHECK(ws.count == 4 && ws.current == 0 && ws.w[0] == 1000);
    window_sizes_list(&ws, 1024, 600, 1024, 600);
    CHECK(ws.count == 1 && ws.w[0] == 1024);
}

static char *slurp(const char *path)
{
    static char buf[1024];
    FILE *f = fopen(path, "r");
    size_t n = 0;

    buf[0] = '\0';
    if (f == NULL)
        return NULL;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

/* The old file goes to .bak, the store comes back empty (5.6). */
static void test_progress_reset(void)
{
    char dir[] = "/tmp/my_gd_resetXXXXXX";
    char path[64];
    char bak[72];
    progress_t p;
    char *text = NULL;

    if (mkdtemp(dir) == NULL) {
        CHECK(!"mkdtemp");
        return;
    }
    snprintf(path, sizeof(path), "%s/progress.txt", dir);
    snprintf(bak, sizeof(bak), "%s.bak", path);
    progress_load(&p, path);
    CHECK(progress_reset(&p) == 0);          /* nothing to back up: fine */
    CHECK(slurp(bak) == NULL);
    progress_get(&p, "3")->attempts = 12;
    snprintf(progress_get(&p, "3")->song, sizeof(p.entries[0].song), "%s",
        "x.ogg");
    progress_save(&p);
    CHECK(progress_reset(&p) == 0);
    CHECK(p.count == 0 && strcmp(p.path, path) == 0);
    text = slurp(bak);
    CHECK(text != NULL && strstr(text, "3 attempts=12") != NULL);
    CHECK(text != NULL && strstr(text, "song=x.ogg") != NULL);
    text = slurp(path);
    CHECK(text != NULL && text[0] == '\0');
    progress_free(&p);
    progress_load(&p, path);
    CHECK(p.count == 0);
    progress_free(&p);
    remove(path);
    remove(bak);
    rmdir(dir);
}

/* Saving clears `dirty` (5.7). */
static void test_dirty(void)
{
    char dir[] = "/tmp/my_gd_dirtyXXXXXX";
    char path[64];
    settings_t s;

    if (mkdtemp(dir) == NULL) {
        CHECK(!"mkdtemp");
        return;
    }
    snprintf(path, sizeof(path), "%s/settings.txt", dir);
    settings_load(&s, path, NULL);
    CHECK(!s.dirty);
    s.dirty = true;
    CHECK(settings_save(&s) == 0 && !s.dirty);
    settings_free(&s);
    remove(path);
    rmdir(dir);
}

void test_options(void)
{
    test_slots_round_trip();
    test_rebind();
    test_slot_names();
    test_window_sizes();
    test_progress_reset();
    test_dirty();
}
