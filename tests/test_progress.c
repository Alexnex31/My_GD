/*
** ALEXNEX PROJECT, 2026
** tests/test_progress.c
** File description:
** the progress store: ids, fields kept, atomic save (6.2, 6.4)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/progress.h"
#include "test.h"

#define TMP_STORE "save/test_progress.txt"

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
    progress_load(&p, TMP_STORE);
    CHECK(progress_get(&p, "nope") == NULL);
    progress_free(&p);
}

/* A missing file is an empty store, and get creates the entry it is asked for. */
static void test_empty_store(void)
{
    progress_t p;
    progress_entry_t *e = NULL;

    remove(TMP_STORE);
    CHECK(progress_load(&p, TMP_STORE) == 0);
    CHECK(p.count == 0);
    e = progress_get(&p, "7");
    CHECK(e != NULL && e->attempts == 0 && e->best == 0.0f);
    CHECK(progress_get(&p, "7") == e);        /* the same entry, not a second */
    CHECK(p.count == 1);
    progress_free(&p);
}

static void test_round_trip(void)
{
    progress_t p;
    progress_entry_t *e = NULL;

    remove(TMP_STORE);
    progress_load(&p, TMP_STORE);
    e = progress_get(&p, "10280");
    e->attempts = 33;
    e->best = 47.83f;
    e->practice_best = 81.20f;
    e->level_hash = 0x9f2c41d07ab35e11ULL;
    progress_get(&p, "2")->attempts = 5;
    CHECK(progress_save(&p) == 0);
    progress_free(&p);
    progress_load(&p, TMP_STORE);
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

    write_file(TMP_STORE, "7 attempts=3 coins=2 best=12.50 medal=gold\n");
    progress_load(&p, TMP_STORE);
    CHECK(p.count == 1);
    CHECK(progress_get(&p, "7")->attempts == 3);
    CHECK(progress_save(&p) == 0);
    progress_free(&p);
    progress_load(&p, TMP_STORE);
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

    write_file(TMP_STORE, "# a comment\n"
        "\n"
        "notanid attempts=9\n"
        "7 attempts=abc best=12.5\n"          /* attempts unreadable, best fine */
        "../evil attempts=1\n"
        "2 attempts=4\n");
    progress_load(&p, TMP_STORE);
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

    remove(TMP_STORE);
    progress_load(&p, TMP_STORE);
    progress_get(&p, "3")->best = 100.0f;
    CHECK(progress_save(&p) == 0);
    CHECK(progress_save(&p) == 0);
    CHECK(file_exists(TMP_STORE));
    CHECK(!file_exists(TMP_STORE ".tmp"));
    progress_free(&p);
    remove(TMP_STORE);
}

void test_progress(void)
{
    test_ids();
    test_empty_store();
    test_round_trip();
    test_unknown_fields_survive();
    test_bad_lines();
    test_save_is_clean();
}
