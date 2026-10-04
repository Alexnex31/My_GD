/*
** ALEXNEX PROJECT, 2026
** sim/settings.c
** File description:
** the settings store: save/settings.txt, one table for every key (FEATURES 2)
*/

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "sim/alloc.h"
#include "sim/save_file.h"
#include "sim/settings.h"

#define LINE_MAX_LEN 1024
#define MSG_LEN 1400

typedef enum setting_type {
    ST_INT,
    ST_BOOL,
    ST_FILE,                      /* a plain file name, no folder            */
    ST_BINDING,
    ST_BINDINGS
} setting_type_t;

typedef struct setting_def {
    const char *key;
    setting_type_t type;
    size_t offset;                /* offsetof(settings_t, field)             */
    int min;
    int max;
    const int *choices;           /* ST_INT: the only values allowed, or NULL */
} setting_def_t;

static const int FPS_CHOICES[] = {0, 60, 120, 144, 240, -1};

#define FIELD(name) offsetof(settings_t, name)

/* A new setting is one line here, one field and one default (2.3). */
static const setting_def_t DEFS[] = {
    {"music_volume", ST_INT, FIELD(music_volume), 0, 100, NULL},
    {"sfx_volume", ST_INT, FIELD(sfx_volume), 0, 100, NULL},
    {"menu_song", ST_FILE, FIELD(menu_song), 0, 0, NULL},
    {"fullscreen", ST_BOOL, FIELD(fullscreen), 0, 1, NULL},
    {"window_width", ST_INT, FIELD(window_width), 640, 7680, NULL},
    {"window_height", ST_INT, FIELD(window_height), 360, 4320, NULL},
    {"vsync", ST_BOOL, FIELD(vsync), 0, 1, NULL},
    {"fps_limit", ST_INT, FIELD(fps_limit), 0, 240, FPS_CHOICES},
    {"show_percent", ST_BOOL, FIELD(show_percent), 0, 1, NULL},
    {"show_progress_bar", ST_BOOL, FIELD(show_progress_bar), 0, 1, NULL},
    {"show_attempts", ST_BOOL, FIELD(show_attempts), 0, 1, NULL},
    {"jump_bindings", ST_BINDINGS, FIELD(jump_bindings), 1, BINDINGS_MAX,
        NULL},
    {"restart_key", ST_BINDING, FIELD(restart_key), 0, 0, NULL},
    {"checkpoint_key", ST_BINDING, FIELD(checkpoint_key), 0, 0, NULL},
    {"remove_checkpoint_key", ST_BINDING, FIELD(remove_checkpoint_key), 0, 0,
        NULL},
    {"audio_offset_ms", ST_INT, FIELD(audio_offset_ms), -300, 300, NULL},
};

#define NB_DEFS (sizeof(DEFS) / sizeof(DEFS[0]))

/* The keys that act in a level, checked against the jump in this order. */
static const size_t ACTION_KEYS[] = {
    FIELD(restart_key), FIELD(checkpoint_key), FIELD(remove_checkpoint_key),
};

#define NB_ACTION_KEYS (sizeof(ACTION_KEYS) / sizeof(ACTION_KEYS[0]))

static const binding_t ESCAPE = {BIND_KEY, KEY_Escape};

void settings_defaults(settings_t *s)
{
    *s = (settings_t){
        .music_volume = 80,
        .sfx_volume = 100,
        .menu_song = "menu_loop.ogg",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true,
        .show_percent = true,
        .show_progress_bar = true,
        .show_attempts = true,
        .jump_bindings = {{{BIND_KEY, KEY_Space}, {BIND_KEY, KEY_Up},
            {BIND_MOUSE, MOUSE_Left}, {BIND_JOY, 0}}, 4},
        .restart_key = {BIND_KEY, KEY_R},
        .checkpoint_key = {BIND_KEY, KEY_Z},
        .remove_checkpoint_key = {BIND_KEY, KEY_X},
    };
}

bool bindings_contain(const bindings_t *b, binding_t one)
{
    for (int i = 0; i < b->count; i++)
        if (binding_equal(b->items[i], one))
            return true;
    return false;
}

static void *field(settings_t *s, const setting_def_t *d)
{
    return (char *)s + d->offset;
}

static const void *cfield(const settings_t *s, const setting_def_t *d)
{
    return (const char *)s + d->offset;
}

static bool parse_int(const setting_def_t *d, const char *v, int *out)
{
    char *end = NULL;
    long n = 0;

    errno = 0;
    n = strtol(v, &end, 10);
    if (end == v || *end != '\0' || errno != 0 || n < d->min || n > d->max)
        return false;
    if (d->choices != NULL) {
        int i = 0;

        while (d->choices[i] >= 0 && d->choices[i] != n)
            i += 1;
        if (d->choices[i] < 0)
            return false;
    }
    *out = (int)n;
    return true;
}

/* A name in music/: no folder, no hidden file, nothing to escape with. */
static bool valid_file_name(const char *v)
{
    size_t n = strlen(v);

    if (n == 0 || n >= sizeof(((settings_t *)0)->menu_song) || v[0] == '.')
        return false;
    for (size_t i = 0; i < n; i++)
        if (v[i] == '/' || v[i] == '\\' || !isprint((unsigned char)v[i]))
            return false;
    return true;
}

/* "" is unbound. Escape is the pause, it can't be bound to anything. */
static bool parse_binding(const char *v, binding_t *out)
{
    binding_t b = {BIND_NONE, 0};

    if (v[0] != '\0' && (!binding_parse(v, &b) || binding_equal(b, ESCAPE)))
        return false;                        /* `out` untouched: the default stays */
    *out = b;
    return true;
}

static char *trim(char *s)
{
    char *end = NULL;

    while (isspace((unsigned char)*s))
        s += 1;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        end -= 1;
    *end = '\0';
    return s;
}

/* "Space, Up ,MouseLeft": 1 to BINDINGS_MAX names, each once, none empty. */
static bool parse_bindings(const char *v, bindings_t *out)
{
    char buf[LINE_MAX_LEN];
    char *tok = buf;
    char *comma = NULL;
    bindings_t b = {0};

    snprintf(buf, sizeof(buf), "%s", v);
    do {
        binding_t one;

        comma = strchr(tok, ',');
        if (comma != NULL)
            *comma = '\0';
        tok = trim(tok);
        if (b.count == BINDINGS_MAX || !parse_binding(tok, &one)
            || one.kind == BIND_NONE || bindings_contain(&b, one))
            return false;
        b.items[b.count++] = one;
        tok = comma + 1;
    } while (comma != NULL);
    *out = b;
    return true;
}

static bool parse_value(settings_t *s, const setting_def_t *d, const char *v)
{
    int n = 0;

    switch (d->type) {
    case ST_INT:
        return parse_int(d, v, field(s, d));
    case ST_BOOL:
        if (!parse_int(d, v, &n))
            return false;
        *(bool *)field(s, d) = n != 0;
        return true;
    case ST_FILE:
        if (!valid_file_name(v))
            return false;
        snprintf(field(s, d), sizeof(s->menu_song), "%s", v);
        return true;
    case ST_BINDING:
        return parse_binding(v, field(s, d));
    case ST_BINDINGS:
        return parse_bindings(v, field(s, d));
    }
    return false;
}

/* The value as the file writes it. */
static void format_value(const settings_t *s, const setting_def_t *d,
    char *buf, size_t size)
{
    const bindings_t *b = cfield(s, d);
    size_t used = 0;

    switch (d->type) {
    case ST_INT:
        snprintf(buf, size, "%d", *(const int *)cfield(s, d));
        return;
    case ST_BOOL:
        snprintf(buf, size, "%d", *(const bool *)cfield(s, d) ? 1 : 0);
        return;
    case ST_FILE:
        snprintf(buf, size, "%s", (const char *)cfield(s, d));
        return;
    case ST_BINDING:
        binding_format(*(const binding_t *)cfield(s, d), buf, size);
        return;
    case ST_BINDINGS:
        buf[0] = '\0';
        for (int i = 0; i < b->count && used + BINDING_NAME_MAX + 1 < size;
            i++) {
            if (i > 0)
                buf[used++] = ',';
            binding_format(b->items[i], buf + used, size - used);
            used += strlen(buf + used);
        }
        return;
    }
}

static void say(sim_log_fn log, const char *msg)
{
    if (log != NULL)
        log(msg);
}

static void keep_extra(settings_t *s, const char *key, const char *value)
{
    size_t len = strlen(key) + strlen(value) + 2;
    char **grown = sim_xcalloc(s->nb_extra + 1, sizeof(char *));

    if (s->extra != NULL)
        memcpy(grown, s->extra, s->nb_extra * sizeof(char *));
    free(s->extra);
    s->extra = grown;
    s->extra[s->nb_extra] = sim_xcalloc(len, 1);
    snprintf(s->extra[s->nb_extra], len, "%s=%s", key, value);
    s->nb_extra += 1;
}

static bool valid_key(const char *k)
{
    if (k[0] == '\0')
        return false;
    for (; *k != '\0'; k++)
        if (!(*k >= 'a' && *k <= 'z') && *k != '_')
            return false;
    return true;
}

static void parse_line(settings_t *s, char *line, int nb, sim_log_fn log)
{
    char msg[MSG_LEN];
    char was[LINE_MAX_LEN];
    char *eq = strchr(line, '=');
    char *key = NULL;
    char *value = NULL;

    if (eq == NULL || (*eq = '\0', !valid_key(key = trim(line)))) {
        snprintf(msg, sizeof(msg), "%s:%d: not key=value, ignored",
            s->path, nb);
        return say(log, msg);
    }
    value = trim(eq + 1);
    for (size_t i = 0; i < NB_DEFS; i++) {
        if (strcmp(key, DEFS[i].key) != 0)
            continue;
        if (parse_value(s, &DEFS[i], value))
            return;
        format_value(s, &DEFS[i], was, sizeof(was));
        snprintf(msg, sizeof(msg), "%s:%d: %s=%s invalid, using %s",
            s->path, nb, key, value, was);
        return say(log, msg);
    }
    keep_extra(s, key, value);               /* a newer build's: kept (2.3) */
}

static bool action_taken(const settings_t *s, size_t k, binding_t b)
{
    if (b.kind == BIND_NONE)
        return false;
    if (bindings_contain(&s->jump_bindings, b))
        return true;
    for (size_t j = 0; j < k; j++)
        if (binding_equal(*(const binding_t *)((const char *)s
            + ACTION_KEYS[j]), b))
            return true;
    return false;
}

static const setting_def_t *def_at(size_t offset)
{
    for (size_t i = 0; i < NB_DEFS; i++)
        if (DEFS[i].offset == offset)
            return &DEFS[i];
    return NULL;
}

/*
** An action key can't also be a jump input, nor share a key with an earlier
** one: it falls back to its default, or is left unbound if that's taken too.
*/
static void check_action_keys(settings_t *s, sim_log_fn log)
{
    settings_t defaults;
    char msg[MSG_LEN];
    char name[BINDING_NAME_MAX];

    settings_defaults(&defaults);
    for (size_t k = 0; k < NB_ACTION_KEYS; k++) {
        binding_t *b = (binding_t *)((char *)s + ACTION_KEYS[k]);
        const binding_t *def = (const binding_t *)((const char *)&defaults
            + ACTION_KEYS[k]);

        if (!action_taken(s, k, *b))
            continue;
        binding_format(*b, name, sizeof(name));
        *b = action_taken(s, k, *def) ? (binding_t){BIND_NONE, 0} : *def;
        snprintf(msg, sizeof(msg), "%s: %s=%s is already used, %s",
            s->path, def_at(ACTION_KEYS[k])->key, name,
            b->kind == BIND_NONE ? "unbound" : "using its default");
        say(log, msg);
    }
}

static bool regular_file(FILE *f)
{
    struct stat st;

    return fstat(fileno(f), &st) == 0 && S_ISREG(st.st_mode);
}

static int read_lines(settings_t *s, FILE *f, sim_log_fn log)
{
    char line[LINE_MAX_LEN];
    char *t = NULL;
    int nb = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        nb += 1;
        t = trim(line);
        if (t[0] != '\0' && t[0] != '#')
            parse_line(s, t, nb, log);
    }
    return ferror(f) ? -1 : 0;
}

int settings_load(settings_t *s, const char *path, sim_log_fn log)
{
    char msg[MSG_LEN];
    FILE *f = NULL;

    settings_defaults(s);
    snprintf(s->path, sizeof(s->path), "%s", path);
    f = fopen(path, "r");
    if (f == NULL && errno == ENOENT)
        return 0;                            /* the first run: defaults */
    if (f == NULL || !regular_file(f) || read_lines(s, f, log) != 0) {
        if (f != NULL)
            fclose(f);
        settings_free(s);
        settings_defaults(s);
        snprintf(s->path, sizeof(s->path), "%s", path);
        s->unreadable = true;
        snprintf(msg, sizeof(msg), "%s: unreadable, using the defaults"
            " (it won't be overwritten)", path);
        return say(log, msg), -1;
    }
    fclose(f);
    check_action_keys(s, log);
    return 0;
}

static void write_settings(FILE *f, const void *data)
{
    const settings_t *s = data;
    char value[LINE_MAX_LEN];

    fprintf(f, "# my_gd settings (FEATURES 2.2). Comments aren't kept.\n");
    for (size_t i = 0; i < NB_DEFS; i++) {
        format_value(s, &DEFS[i], value, sizeof(value));
        fprintf(f, "%s=%s\n", DEFS[i].key, value);
    }
    for (size_t i = 0; i < s->nb_extra; i++)
        fprintf(f, "%s\n", s->extra[i]);
}

int settings_save(settings_t *s)
{
    if (s->unreadable)
        return -1;
    if (save_atomic(s->path, write_settings, s) != 0)
        return -1;
    s->dirty = false;
    return 0;
}

void settings_free(settings_t *s)
{
    for (size_t i = 0; i < s->nb_extra; i++)
        free(s->extra[i]);
    free(s->extra);
    s->extra = NULL;
    s->nb_extra = 0;
}
