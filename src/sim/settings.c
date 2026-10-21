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

#include "sim/alloc.h"
#include "sim/save_file.h"
#include "sim/settings.h"

#define VALUE_LEN 1024                /* a value as this build writes one */
#define MSG_LEN 1400

typedef enum setting_type {
    ST_INT,
    ST_BOOL,
    ST_FILE,                      /* a plain file name, no folder: whether it
                                     exists is the game's to check          */
    ST_BINDING,
    ST_BINDINGS,
    ST_COMBO                      /* an editor shortcut: a key and modifiers */
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
#define ED_KEY_SETTING(id, name, label, page, key, mods, rep) \
    {"editor_" name, ST_COMBO, FIELD(editor_keys[ED_KEY_##id]), 0, 0, NULL},

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
    {"debug_overlay_key", ST_BINDING, FIELD(debug_overlay_key), 0, 0, NULL},
    {"previous_start_key", ST_BINDING, FIELD(previous_start_key), 0, 0,
        NULL},
    {"next_start_key", ST_BINDING, FIELD(next_start_key), 0, 0, NULL},
    {"audio_offset_ms", ST_INT, FIELD(audio_offset_ms), -300, 300, NULL},
    ED_KEY_LIST(ED_KEY_SETTING)
    {"icon_cube", ST_FILE, FIELD(icons[MODE_CUBE]), 0, 0, NULL},
    {"icon_ship", ST_FILE, FIELD(icons[MODE_SHIP]), 0, 0, NULL},
    {"icon_ball", ST_FILE, FIELD(icons[MODE_BALL]), 0, 0, NULL},
    {"icon_ufo", ST_FILE, FIELD(icons[MODE_UFO]), 0, 0, NULL},
    {"icon_wave", ST_FILE, FIELD(icons[MODE_WAVE]), 0, 0, NULL},
    {"icon_robot", ST_FILE, FIELD(icons[MODE_ROBOT]), 0, 0, NULL},
    {"icon_spider", ST_FILE, FIELD(icons[MODE_SPIDER]), 0, 0, NULL},
    {"icon_swing", ST_FILE, FIELD(icons[MODE_SWING]), 0, 0, NULL},
};

#define NB_DEFS (sizeof(DEFS) / sizeof(DEFS[0]))

/* The keys that act in a level, checked against the jump in this order. */
static const size_t ACTION_KEYS[] = {
    FIELD(restart_key), FIELD(checkpoint_key), FIELD(remove_checkpoint_key),
    FIELD(debug_overlay_key), FIELD(previous_start_key), FIELD(next_start_key),
};

#define NB_ACTION_KEYS (sizeof(ACTION_KEYS) / sizeof(ACTION_KEYS[0]))

static const binding_t ESCAPE = {BIND_KEY, KEY_Escape, 0};

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
        .jump_bindings = {{{BIND_KEY, KEY_Space, 0}, {BIND_KEY, KEY_Up, 0},
            {BIND_MOUSE, MOUSE_Left, 0}, {BIND_JOY, 0, 0}}, 4},
        .restart_key = {BIND_KEY, KEY_R, 0},
        .checkpoint_key = {BIND_KEY, KEY_Z, 0},
        .remove_checkpoint_key = {BIND_KEY, KEY_X, 0},
        .debug_overlay_key = {BIND_KEY, KEY_F3, 0},
        .previous_start_key = {BIND_KEY, KEY_A, 0},
        .next_start_key = {BIND_KEY, KEY_E, 0},
        .icons = {[MODE_CUBE] = "player_icon.png",
            [MODE_SHIP] = "ship_icon.png", [MODE_UFO] = "ufo_icon.png",
            [MODE_WAVE] = "wave_icon.png", [MODE_BALL] = "ball_icon.png",
            [MODE_ROBOT] = "robot_icon.png", [MODE_SWING] = "swing_icon.png",
            [MODE_SPIDER] = "spider_icon.png"},
    };
    for (int k = 0; k < ED_KEY_COUNT; k++)
        s->editor_keys[k] = ed_key_default((ed_key_t)k);
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

/* A name in a folder: no folder of its own, no hidden file, nothing to escape. */
static bool valid_file_name(const char *v)
{
    size_t n = strlen(v);

    if (n == 0 || n >= SETTING_FILE_MAX || v[0] == '.')
        return false;
    for (size_t i = 0; i < n; i++)
        if (v[i] == '/' || v[i] == '\\' || !isprint((unsigned char)v[i]))
            return false;
    return true;
}

/*
** "" is unbound. Escape is the pause, it can't be bound to anything. The
** game's inputs are single: no modifiers.
*/
static bool parse_binding(const char *v, binding_t *out)
{
    binding_t b = {BIND_NONE, 0, 0};

    if (v[0] != '\0' && (!binding_parse(v, &b) || binding_equal(b, ESCAPE)
        || b.mods != 0))
        return false;                        /* `out` untouched: the default stays */
    *out = b;
    return true;
}

/* An editor shortcut: "" or a key with modifiers (ed_key_bindable). */
static bool parse_combo(const char *v, binding_t *out)
{
    binding_t b = {BIND_NONE, 0, 0};

    if (v[0] != '\0' && (!binding_parse(v, &b) || !ed_key_bindable(b)))
        return false;
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
    char *buf = sim_xstrdup(v);
    char *tok = buf;
    char *comma = NULL;
    bindings_t b = {0};

    for (;;) {
        binding_t one;

        comma = strchr(tok, ',');
        if (comma != NULL)
            *comma = '\0';
        tok = trim(tok);
        if (b.count == BINDINGS_MAX || !parse_binding(tok, &one)
            || one.kind == BIND_NONE || bindings_contain(&b, one))
            return free(buf), false;
        b.items[b.count++] = one;
        if (comma == NULL)
            break;
        tok = comma + 1;
    }
    free(buf);
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
        snprintf(field(s, d), SETTING_FILE_MAX, "%s", v);
        return true;
    case ST_BINDING:
        return parse_binding(v, field(s, d));
    case ST_BINDINGS:
        return parse_bindings(v, field(s, d));
    case ST_COMBO:
        return parse_combo(v, field(s, d));
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
    case ST_COMBO:
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

/* Lower case, digits and '_': a newer build's "volume2" is kept (2.3). */
static bool valid_key(const char *k)
{
    if (k[0] == '\0')
        return false;
    for (; *k != '\0'; k++)
        if (!(*k >= 'a' && *k <= 'z') && !(*k >= '0' && *k <= '9')
            && *k != '_')
            return false;
    return true;
}

static void parse_line(settings_t *s, char *line, int nb, sim_log_fn log)
{
    char msg[MSG_LEN];
    char was[VALUE_LEN];
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
        *b = action_taken(s, k, *def) ? (binding_t){BIND_NONE, 0, 0} : *def;
        snprintf(msg, sizeof(msg), "%s: %s=%s is already used, %s",
            s->path, def_at(ACTION_KEYS[k])->key, name,
            b->kind == BIND_NONE ? "unbound" : "using its default");
        say(log, msg);
    }
}

/*
** Two editor actions can't share a combination: the later one falls back to
** its default, or is left unbound if that's taken too. The game's keys are
** another place: the editor's can be the same.
*/
static bool editor_taken(const settings_t *s, int k, binding_t b)
{
    for (int j = 0; j < ED_KEY_COUNT && b.kind != BIND_NONE; j++)
        if (j != k && binding_equal(s->editor_keys[j], b))
            return true;
    return false;
}

static void check_editor_keys(settings_t *s, sim_log_fn log)
{
    char msg[MSG_LEN];
    char name[BINDING_NAME_MAX];

    for (int k = 1; k < ED_KEY_COUNT; k++) {
        binding_t *b = &s->editor_keys[k];
        binding_t def = ed_key_default((ed_key_t)k);
        bool earlier = false;

        for (int j = 0; j < k && b->kind != BIND_NONE; j++)
            earlier = earlier || binding_equal(s->editor_keys[j], *b);
        if (!earlier)
            continue;
        binding_format(*b, name, sizeof(name));
        *b = editor_taken(s, k, def) ? (binding_t){BIND_NONE, 0, 0} : def;
        snprintf(msg, sizeof(msg), "%s: editor_%s=%s is already used, %s",
            s->path, ed_key_name((ed_key_t)k), name,
            b->kind == BIND_NONE ? "unbound" : "using its default");
        say(log, msg);
    }
}

/* Whole lines, however long: none is ever cut in two. */
static void read_lines(settings_t *s, char *text, sim_log_fn log)
{
    char *cursor = text;
    char *line = NULL;
    char *t = NULL;
    int nb = 0;

    while ((line = save_next_line(&cursor)) != NULL) {
        nb += 1;
        t = trim(line);
        if (t[0] != '\0' && t[0] != '#')
            parse_line(s, t, nb, log);
    }
}

int settings_load(settings_t *s, const char *path, sim_log_fn log)
{
    char msg[MSG_LEN];
    char *text = NULL;
    save_read_t got;

    settings_defaults(s);
    snprintf(s->path, sizeof(s->path), "%s", path);
    got = save_read(path, &text);
    if (got == SAVE_MISSING)
        return 0;                            /* the first run: defaults */
    if (got == SAVE_UNREADABLE) {
        s->unreadable = true;
        snprintf(msg, sizeof(msg), "%s: unreadable, using the defaults"
            " (it won't be overwritten)", path);
        return say(log, msg), -1;
    }
    read_lines(s, text, log);
    free(text);
    check_action_keys(s, log);
    check_editor_keys(s, log);
    return 0;
}

static void write_settings(FILE *f, const void *data)
{
    const settings_t *s = data;
    char value[VALUE_LEN];

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
