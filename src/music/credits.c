/*
** ALEXNEX PROJECT, 2026
** music/credits.c
** File description:
** the Credits screen's lines: the songs, then res/CREDITS.md (FEATURES 4.11)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "music/credits.h"
#include "sim/alloc.h"

typedef struct rows {
    char **items;
    int count;
    int cap;
} rows_t;

static void add(rows_t *r, const char *s, size_t len)
{
    char **bigger = NULL;

    if (r->count + 1 >= r->cap) {
        r->cap = r->cap == 0 ? 32 : r->cap * 2;
        bigger = sim_xcalloc((size_t)r->cap, sizeof(char *));
        if (r->count > 0)
            memcpy(bigger, r->items, (size_t)r->count * sizeof(char *));
        free(r->items);
        r->items = bigger;
    }
    r->items[r->count] = sim_xcalloc(len + 1, 1);
    memcpy(r->items[r->count], s, len);
    r->count += 1;
}

/* Characters, not bytes: a UTF-8 sequence counts once. */
static size_t chars(const char *s, size_t len)
{
    size_t n = 0;

    for (size_t i = 0; i < len; i++)
        n += ((unsigned char)s[i] & 0xC0) != 0x80;
    return n;
}

/* At the last space that keeps it within CREDITS_WRAP, or cut there. */
static void add_wrapped(rows_t *r, const char *s, size_t len)
{
    while (chars(s, len) > CREDITS_WRAP) {
        size_t cut = 0;
        size_t space = 0;

        while (chars(s, cut + 1) <= CREDITS_WRAP)
            cut += 1;
        while (cut < len && ((unsigned char)s[cut] & 0xC0) == 0x80)
            cut -= 1;                        /* never inside a character */
        for (size_t i = 1; i < cut; i++)
            space = s[i] == ' ' ? i : space;
        cut = space > 0 ? space : cut;
        add(r, s, cut);
        s += cut;
        len -= cut;
        while (len > 0 && *s == ' ') {
            s += 1;
            len -= 1;
        }
    }
    if (len > 0)
        add(r, s, len);
}

static void add_songs(rows_t *r, const library_t *lib)
{
    char line[256];

    add(r, "MUSIC", 5);
    if (lib->count == 0)
        add(r, "No song in music/", 17);
    for (size_t i = 0; i < lib->count; i++) {
        const song_t *s = &lib->songs[i];

        snprintf(line, sizeof(line), "%s \xe2\x80\x94 %s (%s)", s->title,
            s->artist, s->license[0] != '\0' ? s->license
            : "license unknown");
        add_wrapped(r, line, strlen(line));
    }
}

/* Each line trimmed, headings without their #, blank lines dropped. */
static void add_markdown(rows_t *r, const char *md)
{
    while (*md != '\0') {
        const char *end = strchr(md, '\n');
        size_t len = end != NULL ? (size_t)(end - md) : strlen(md);
        const char *s = md;

        md += len + (end != NULL);
        while (len > 0 && (*s == '#' || *s == ' ' || *s == '\t')) {
            s += 1;
            len -= 1;
        }
        while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\r'
            || s[len - 1] == '\t'))
            len -= 1;
        if (len > 0)
            add_wrapped(r, s, len);
    }
}

char **credits_rows(const library_t *lib, const char *md, int *count)
{
    static const char NONE[] = "res/CREDITS.md isn't written yet: each "
        "image, sound and font, its author and its license go there";
    rows_t r = {0};

    add_songs(&r, lib);
    add(&r, "", 0);
    add(&r, "IMAGES, SOUNDS, FONT", 20);
    if (md != NULL)
        add_markdown(&r, md);
    else
        add_wrapped(&r, NONE, strlen(NONE));
    *count = r.count;
    return r.items;
}

void credits_free(char **rows)
{
    for (int i = 0; rows != NULL && rows[i] != NULL; i++)
        free(rows[i]);
    free(rows);
}
