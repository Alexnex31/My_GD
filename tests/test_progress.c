/*
** ALEXNEX PROJECT, 2026
** tests/test_progress.c
** File description:
** the progress store: ids, fields kept, atomic save (6.2, 6.4)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sim/progress.h"
#include "test.h"

/* A folder of its own under /tmp: the player's save/ is never touched. */
static char tmp_dir[] = "/tmp/my_gd_progressXXXXXX";
static char store[64];

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");

    if (f == NULL)
        return;
    fputs(text, f);
    fclose(f);
}

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "r");

    return f == NULL ? false : (fclose(f), true);
}

/* The id is the level file's digits, and nothing else (7.2). */
static void test_ids(void)
{
    progress_t p;

    CHECK(progress_valid_id("1"));
    CHECK(progress_valid_id("10280"));
    CHECK(!progress_valid_id(""));
    CHECK(!progress_valid_id("1a"));
    CHECK(!progress_valid_id("../etc/passwd"));
    CHECK(!progress_valid_id("1234567890123456789"));   /* 19 digits */
    progress_load(&p, store);
    CHECK(progress_get(&p, "nope") == NULL);
    progress_free(&p);
}

/* A missing file is an empty store, and get creates the entry it is asked for. */
static void test_empty_store(void)
{
    progress_t p;
    progress_entry_t *e = NULL;

    remove(store);
    CHECK(progress_load(&p, store) == 0);
    CHECK(p.count == 0);
    e = progress_get(&p, "7");
    CHECK(e != NULL && e->attempts == 0 && e->best == 0.0f);
    CHECK(progress_get(&p, "7") == e);        /* the same entry, not a second */
    CHECK(p.count == 1);
    CHECK(progress_find(&p, "7") == e);
    CHECK(progress_find(&p, "8") == NULL && p.count == 1);   /* never adds */
    CHECK(progress_find(&p, "nope") == NULL);
    progress_free(&p);
}

/* Browsing the list leaves no empty lines, and 99.996 isn't saved as 100. */
static void test_saved_numbers(void)
{
    progress_t p;

    remove(store);
    progress_load(&p, store);
    progress_get(&p, "1");                    /* looked at, never played */
    progress_get(&p, "2")->best = 99.996f;
    progress_get(&p, "3")->best = 100.0f;
    CHECK(progress_save(&p) == 0);
    progress_free(&p);
    progress_load(&p, store);
    CHECK(p.count == 2 && progress_find(&p, "1") == NULL);
    CHECK(progress_find(&p, "2")->best < 100.0f);
    CHECK(progress_find(&p, "3")->best == 100.0f);
    progress_free(&p);
    CHECK(progress_printable(99.996f) < 100.0f);
    CHECK(progress_printable(47.83f) == 47.83f);
    CHECK(progress_printable(100.0f) == 100.0f);
    remove(store);
}

static void test_round_trip(void)
{
    progress_t p;
    progress_entry_t *e = NULL;

    remove(store);
    progress_load(&p, store);
    e = progress_get(&p, "10280");
    e->attempts = 33;
    e->best = 47.83f;
    e->practice_best = 81.20f;
    e->level_hash = 0x9f2c41d07ab35e11ULL;
    progress_get(&p, "2")->attempts = 5;
    CHECK(progress_save(&p) == 0);
    progress_free(&p);
    progress_load(&p, store);
    CHECK(p.count == 2);
    e = progress_get(&p, "10280");
    CHECK(e->attempts == 33);
    CHECK(e->best > 47.82f && e->best < 47.84f);
    CHECK(e->practice_best > 81.19f && e->practice_best < 81.21f);
    CHECK(e->level_hash == 0x9f2c41d07ab35e11ULL);
    CHECK(progress_get(&p, "2")->attempts == 5);
    progress_free(&p);
}

/* A newer build's fields survive an older build's save (6.4). */
static void test_unknown_fields_survive(void)
{
    progress_t p;

    write_file(store, "7 attempts=3 coins=2 best=12.50 medal=gold\n");
    progress_load(&p, store);
    CHECK(p.count == 1);
    CHECK(progress_get(&p, "7")->attempts == 3);
    CHECK(progress_save(&p) == 0);
    progress_free(&p);
    progress_load(&p, store);
    CHECK(progress_get(&p, "7")->extra != NULL);
    CHECK(strstr(progress_get(&p, "7")->extra, "coins=2") != NULL);
    CHECK(strstr(progress_get(&p, "7")->extra, "medal=gold") != NULL);
    CHECK(progress_get(&p, "7")->attempts == 3);
    progress_free(&p);
}

/* A broken line never costs the rest of the store. */
static void test_bad_lines(void)
{
    progress_t p;

    write_file(store, "# a comment\n"
        "\n"
        "notanid attempts=9\n"
        "7 attempts=abc best=12.5\n"          /* attempts unreadable, best fine */
        "../evil attempts=1\n"
        "2 attempts=4\n");
    progress_load(&p, store);
    CHECK(p.count == 2);                      /* only 7 and 2 */
    CHECK(progress_get(&p, "7")->attempts == 0);
    CHECK(progress_get(&p, "7")->best > 12.4f);
    CHECK(progress_get(&p, "2")->attempts == 4);
    progress_free(&p);
}

/* The save leaves no .tmp behind, and rewriting twice is stable. */
static void test_save_is_clean(void)
{
    progress_t p;
    char store_tmp[80];

    snprintf(store_tmp, sizeof(store_tmp), "%s.tmp", store);
    remove(store);
    progress_load(&p, store);
    progress_get(&p, "3")->best = 100.0f;
    CHECK(progress_save(&p) == 0);
    CHECK(progress_save(&p) == 0);
    CHECK(file_exists(store));
    CHECK(!file_exists(store_tmp));
    progress_free(&p);
    remove(store);
}

/* The store's own folder is made on save, wherever the store lives. */
static void test_save_makes_its_folder(void)
{
    progress_t p;
    char dir[80];
    char path[96];

    snprintf(dir, sizeof(dir), "%s/sub", tmp_dir);
    snprintf(path, sizeof(path), "%s/progress.txt", dir);
    progress_load(&p, path);
    progress_get(&p, "4")->attempts = 1;
    CHECK(progress_save(&p) == 0);
    CHECK(file_exists(path));
    progress_free(&p);
    remove(path);
    rmdir(dir);
}

void test_progress(void)
{
    if (mkdtemp(tmp_dir) == NULL) {
        CHECK(!"mkdtemp");
        return;
    }
    snprintf(store, sizeof(store), "%s/progress.txt", tmp_dir);
    test_ids();
    test_empty_store();
    test_round_trip();
    test_saved_numbers();
    test_unknown_fields_survive();
    test_bad_lines();
    test_save_is_clean();
    test_save_makes_its_folder();
    CHECK(rmdir(tmp_dir) == 0);               /* empty: every test cleaned up */
}
