/*
** ALEXNEX PROJECT, 2026
** tests/test_progress.c
** File description:
** the progress store: ids, fields kept, atomic save (6.2, 6.4)
*/

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sim/progress.h"
#include "sim/save_file.h"
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

static char *slurp(const char *path)
{
    static char buf[16384];
    FILE *f = fopen(path, "r");

    buf[0] = '\0';
    if (f == NULL)
        return buf;
    buf[fread(buf, 1, sizeof(buf) - 1, f)] = '\0';
    fclose(f);
    return buf;
}

/* What a folder holds, "." and ".." aside. */
static int nb_files(const char *dir)
{
    DIR *d = opendir(dir);
    int n = 0;

    if (d == NULL)
        return -1;
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d))
        n += e->d_name[0] != '.';
    closedir(d);
    return n;
}

static int count(const char *text, const char *what)
{
    int n = 0;

    for (const char *at = strstr(text, what); at != NULL;
        at = strstr(at + 1, what))
        n += 1;
    return n;
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

/* The save leaves nothing behind, and rewriting twice is stable. */
static void test_save_is_clean(void)
{
    progress_t p;

    remove(store);
    progress_load(&p, store);
    progress_get(&p, "3")->best = 100.0f;
    CHECK(progress_save(&p) == 0);
    CHECK(progress_save(&p) == 0);
    CHECK(file_exists(store));
    CHECK(nb_files(tmp_dir) == 1);
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

/* A store that can't be read is never saved over (6.4). */
static void test_unreadable(void)
{
    progress_t p;

    mkdir(store, 0755);                      /* a folder where the file goes */
    CHECK(progress_load(&p, store) == -1 && p.unreadable && p.count == 0);
    progress_get(&p, "7")->attempts = 1;
    CHECK(progress_save(&p) == -1);
    CHECK(progress_reset(&p) == -1);
    progress_free(&p);
    rmdir(store);
    if (geteuid() == 0)
        return;                              /* root reads a mode 000 file */
    write_file(store, "7 attempts=3 best=50.00\n");
    chmod(store, 0);
    CHECK(progress_load(&p, store) == -1 && p.unreadable);
    progress_get(&p, "2")->attempts = 1;
    CHECK(progress_save(&p) == -1);
    progress_free(&p);
    chmod(store, 0644);
    CHECK(strcmp(slurp(store), "7 attempts=3 best=50.00\n") == 0);
    remove(store);
}

/* Comments, a newer build's ids and bare words all come back (6.4). */
static void test_kept_lines(void)
{
    progress_t p;
    char *text = NULL;

    write_file(store, "# my note\n"
        "o12345 attempts=3\n"
        "1234567890123456789 attempts=1\n"
        "7 attempts=2 verified\r\n"
        "  \n");
    CHECK(progress_load(&p, store) == 0 && p.count == 1 && p.nb_kept == 3);
    CHECK(progress_find(&p, "7")->attempts == 2);
    CHECK(progress_save(&p) == 0);
    progress_free(&p);
    text = slurp(store);
    CHECK(strstr(text, "# my note\n") != NULL);
    CHECK(strstr(text, "o12345 attempts=3\n") != NULL);
    CHECK(strstr(text, "1234567890123456789 attempts=1\n") != NULL);
    CHECK(strstr(text, "7 attempts=2 best=0.00 verified\n") != NULL);
    remove(store);
}

/* Numbers out of range are dropped: never a nan best, a negative count. */
static void test_bad_values(void)
{
    progress_t p;
    char *text = NULL;

    write_file(store, "1 best=nan practice_best=inf attempts=-5\n"
        "2 best=-3 practice_best=101 attempts=3000000000\n"
        "3 attempts=abc best=12.5 hash=-1\n"
        "4 attempts=+4 hash= 0x12\n");
    progress_load(&p, store);
    CHECK(progress_find(&p, "1")->best == 0.0f);
    CHECK(progress_find(&p, "1")->practice_best == 0.0f);
    CHECK(progress_find(&p, "1")->attempts == 0);
    CHECK(progress_find(&p, "2")->best == 0.0f);
    CHECK(progress_find(&p, "2")->practice_best == 0.0f);
    CHECK(progress_find(&p, "2")->attempts == 0);
    CHECK(progress_find(&p, "3")->best == 12.5f);
    CHECK(progress_find(&p, "3")->level_hash == 0);
    CHECK(progress_find(&p, "4")->attempts == 0);
    CHECK(progress_find(&p, "3")->extra == NULL);   /* not kept twice */
    CHECK(progress_save(&p) == 0);
    progress_free(&p);
    text = slurp(store);
    CHECK(count(text, "attempts=") == 2      /* 1 and 2 had nothing left */ && strstr(text, "abc") == NULL);
    remove(store);
}

/* A line is whole however long: its end is never another level's line. */
static void test_long_line(void)
{
    progress_t p;
    char *text = calloc(8192, 1);
    char note[3001];

    memset(note, 'a', sizeof(note) - 1);
    note[sizeof(note) - 1] = '\0';
    snprintf(text, 8192, "7 attempts=1 x=%s best=50 y=345\n2 attempts=4\n",
        note);
    write_file(store, text);
    progress_load(&p, store);
    CHECK(p.count == 2 && p.nb_kept == 0);
    CHECK(progress_find(&p, "7")->best == 50.0f);
    CHECK(progress_find(&p, "7")->extra != NULL
        && strlen(progress_find(&p, "7")->extra) == 2 + strlen(note) + 6);
    CHECK(progress_find(&p, "345") == NULL);
    progress_free(&p);
    free(text);
    remove(store);
}

/* A NUL byte ends a line: what's around it still loads. */
static void test_nul_byte(void)
{
    progress_t p;
    FILE *f = fopen(store, "w");

    if (f == NULL)
        return;
    fwrite("7 attempts=3\0\0\0\n2 attempts=4\n", 1, 29, f);
    fclose(f);
    CHECK(progress_load(&p, store) == 0 && p.count == 2);
    CHECK(progress_find(&p, "7")->attempts == 3);
    CHECK(progress_find(&p, "2")->attempts == 4);
    progress_free(&p);
    remove(store);
}

/* Reading a write-only stream sets its error flag, as a lost write would. */
static void write_then_fail(FILE *f, const void *data)
{
    fputs(data, f);
    fgetc(f);
}

static void write_text(FILE *f, const void *data)
{
    fputs(data, f);
}

/* A write that failed somewhere is never renamed over the old file. */
static void test_write_error(void)
{
    write_file(store, "old\n");
    CHECK(save_atomic(store, write_then_fail, "new\n") == -1);
    CHECK(strcmp(slurp(store), "old\n") == 0);
    CHECK(nb_files(tmp_dir) == 1);
    remove(store);
}

/* A symlinked save stays a symlink, and a file keeps its permissions. */
static void test_link_and_mode(void)
{
    char target[96];
    struct stat st;

    snprintf(target, sizeof(target), "%s/real.txt", tmp_dir);
    write_file(target, "old\n");
    chmod(target, 0600);
    CHECK(symlink("real.txt", store) == 0);
    CHECK(save_atomic(store, write_text, "new\n") == 0);
    CHECK(lstat(store, &st) == 0 && S_ISLNK(st.st_mode));
    CHECK(strcmp(slurp(target), "new\n") == 0);
    CHECK(stat(target, &st) == 0 && (st.st_mode & 0777) == 0600);
    CHECK(nb_files(tmp_dir) == 2);
    remove(store);
    remove(target);
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
    test_unreadable();
    test_kept_lines();
    test_bad_values();
    test_long_line();
    test_nul_byte();
    test_write_error();
    test_link_and_mode();
    CHECK(rmdir(tmp_dir) == 0);               /* empty: every test cleaned up */
}
