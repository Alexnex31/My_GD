/*
** ALEXNEX PROJECT, 2026
** sim/level_parse.c
** File description:
** reads a level file's text into objects (7.2, 7.3)
*/

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/alloc.h"
#include "sim/hitbox.h"
#include "sim/level.h"
#include "sim/modes.h"

#define MAX_TOKENS 16

typedef struct parse_ctx {
    sim_log_fn log;
    const char *source;
    int lineno;
    bool group_warned;
} parse_ctx_t;

static void warn(parse_ctx_t *ctx, const char *what)
{
    char msg[256];

    if (ctx->log == NULL)
        return;
    snprintf(msg, sizeof(msg), "%s:%d: %s", ctx->source, ctx->lineno, what);
    ctx->log(msg);
}

/*
** Numbers are checked to the last character: strtod alone would read "2.5" as
** an int field, accept "nan" or "inf", and leave junk behind (7.3).
*/
static bool parse_double(const char *s, double *out)
{
    char *end;
    double v;

    errno = 0;
    v = strtod(s, &end);
    if (end == s || *end != '\0' || errno == ERANGE || !isfinite(v))
        return false;
    *out = v;
    return true;
}

static bool parse_int(const char *s, int *out)
{
    char *end;
    long v;

    errno = 0;
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE || v < INT_MIN || v > INT_MAX)
        return false;
    *out = (int)v;
    return true;
}

/* Splits on spaces and tabs, in place; a '#' ends the line (7.2). */
static int split_words(char *line, char **tok, int max)
{
    int n = 0;

    while (*line != '\0' && n < max) {
        while (*line == ' ' || *line == '\t')
            line += 1;
        if (*line == '#' || *line == '\0')
            break;
        tok[n] = line;
        n += 1;
        while (*line != '\0' && *line != ' ' && *line != '\t' && *line != '#')
            line += 1;
        if (*line == '#') {
            *line = '\0';
            break;
        }
        if (*line != '\0') {
            *line = '\0';
            line += 1;
        }
    }
    return n;
}

/* size, w and h are whole grid units of 50 px, at least 1 (7.2). */
static int parse_units(const char *val, double *px)
{
    int units = 0;

    if (!parse_int(val, &units) || units <= 0)
        return -1;
    *px = (double)units * UNIT;
    return 0;
}

/* One key=value field; an unknown key is ignored, the line stays (7.2). */
static int parse_field(object_t *o, char *tok, parse_ctx_t *ctx)
{
    char *val = strchr(tok, '=');

    if (val == NULL || val == tok)
        return -1;
    *val = '\0';
    val += 1;
    if (strcmp(tok, "rot") == 0)
        return parse_double(val, &o->rotation) ? 0 : -1;
    if (strcmp(tok, "w") == 0)
        return parse_units(val, &o->rect.w);
    if (strcmp(tok, "h") == 0)
        return parse_units(val, &o->rect.h);
    if (strcmp(tok, "group") == 0) {
        if (!ctx->group_warned)
            warn(ctx, "group= is reserved for triggers, ignored");
        ctx->group_warned = true;
        return 0;
    }
    warn(ctx, "unknown field, ignored");
    return 0;
}

static int type_from_name(const char *name, obj_type_t *out)
{
    static const char *const names[OBJ_TYPE_COUNT] = {
        [OBJ_BLOCK] = "block", [OBJ_SLOPE] = "slope",
        [OBJ_SPIKE] = "spike", [OBJ_PORTAL] = "portal",
    };

    for (int i = 0; i < OBJ_TYPE_COUNT; i++)
        if (names[i] != NULL && strcmp(names[i], name) == 0) {
            *out = (obj_type_t)i;
            return 0;
        }
    return -1;
}

/* The extra word (portals need a mode, 5.5), then the hitbox. */
static int object_init(object_t *o, const char *word, parse_ctx_t *ctx)
{
    int mode = 0;

    if (o->type == OBJ_PORTAL) {
        mode = word == NULL ? -1 : mode_from_name(word);
        if (mode < 0) {
            warn(ctx, "portal needs a known gamemode, line skipped");
            return -1;
        }
        o->portal_mode = (gamemode_t)mode;
    } else if (word != NULL) {
        warn(ctx, "unexpected word after the size, line skipped");
        return -1;
    }
    hitbox_for_object(o);
    return 0;
}

/* type x y size [word] [key=value ...]  (7.2) */
static int parse_object(char *line, object_t *o, parse_ctx_t *ctx)
{
    char *tok[MAX_TOKENS];
    int n = split_words(line, tok, MAX_TOKENS);
    const char *word = NULL;
    int first_field = 4;
    double x = 0.0;
    double y = 0.0;
    int size = 0;

    if (n < 1 || type_from_name(tok[0], &o->type) != 0)
        return 1;                             /* not an object: a header field */
    if (n < 4 || !parse_double(tok[1], &x) || !parse_double(tok[2], &y)
        || !parse_int(tok[3], &size) || size <= 0) {
        warn(ctx, "invalid object line, skipped");
        return -1;
    }
    if (n > 4 && strchr(tok[4], '=') == NULL) {
        word = tok[4];
        first_field = 5;
    }
    *o = (object_t){.type = o->type, .line = ctx->lineno, .size = size,
        .rect = {x, y, size * UNIT, size * UNIT}};
    for (int i = first_field; i < n; i++)
        if (parse_field(o, tok[i], ctx) != 0) {
            warn(ctx, "invalid field value, line skipped");
            return -1;
        }
    return object_init(o, word, ctx);
}

/* The rest of a header line, trimmed, into a fixed buffer (7.2). */
static void take_text(const char *line, char *out, size_t size)
{
    size_t len;

    while (*line == ' ' || *line == '\t')
        line += 1;
    len = strlen(line);
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t'))
        len -= 1;
    if (len >= size)
        len = size - 1;
    memcpy(out, line, len);
    out[len] = '\0';
}

static bool header_number(const char *key, const char *value,
    level_header_t *hdr, parse_ctx_t *ctx)
{
    char text[64];
    double v = 0.0;
    int n = 0;

    take_text(value, text, sizeof(text));
    if (strcmp(key, "version") == 0)
        return parse_int(text, &n) ? (hdr->version = n, true)
            : (warn(ctx, "version must be a whole number, ignored"), true);
    if (strcmp(key, "offset") != 0 && strcmp(key, "bpm") != 0
        && strcmp(key, "first_beat") != 0)
        return false;
    if (!parse_double(text, &v)) {
        warn(ctx, "header field needs a number, ignored");
        return true;
    }
    hdr->offset = strcmp(key, "offset") == 0 ? v : hdr->offset;
    hdr->bpm = strcmp(key, "bpm") == 0 ? v : hdr->bpm;
    hdr->first_beat = strcmp(key, "first_beat") == 0 ? v : hdr->first_beat;
    return true;
}

/* Every line that doesn't start with an object type is a header field. */
static void parse_header_line(char *line, level_header_t *hdr,
    parse_ctx_t *ctx)
{
    char *value = line;
    char key[32];
    size_t klen = 0;

    while (value[klen] != '\0' && value[klen] != ' ' && value[klen] != '\t')
        klen += 1;
    if (klen >= sizeof(key)) {
        warn(ctx, "unknown header field, ignored");
        return;
    }
    memcpy(key, value, klen);
    key[klen] = '\0';
    value += klen;
    if (strcmp(key, "name") == 0)
        return take_text(value, hdr->name, sizeof(hdr->name));
    if (strcmp(key, "author") == 0)
        return take_text(value, hdr->author, sizeof(hdr->author));
    if (strcmp(key, "song") == 0)
        return take_text(value, hdr->song, sizeof(hdr->song));
    if (header_number(key, value, hdr, ctx))
        return;
    warn(ctx, "unknown header field, ignored");
}

/* Copies one line out of the buffer, without its newline or a trailing \r. */
static size_t next_line(const char *buf, size_t len, size_t pos, char *out,
    size_t out_size)
{
    size_t end = pos;
    size_t n;

    while (end < len && buf[end] != '\n')
        end += 1;
    n = end - pos;
    while (n > 0 && buf[pos + n - 1] == '\r')
        n -= 1;
    if (n >= out_size)
        n = out_size - 1;
    memcpy(out, buf + pos, n);
    out[n] = '\0';
    return end < len ? end + 1 : len;
}

static bool is_blank(const char *line)
{
    while (*line == ' ' || *line == '\t')
        line += 1;
    return *line == '\0' || *line == '#';
}

static size_t count_lines(const char *buf, size_t len)
{
    size_t n = 1;

    for (size_t i = 0; i < len; i++)
        n += buf[i] == '\n';
    return n;
}

int level_parse_mem(const char *buf, size_t len, const char *source,
    object_t **objs, size_t *count, level_header_t *hdr, sim_log_fn log)
{
    parse_ctx_t ctx = {log, source, 0, false};
    char line[1024];
    char copy[1024];
    size_t pos = 0;
    size_t nb = 0;
    int ret = 0;

    *objs = sim_xcalloc(count_lines(buf, len), sizeof(object_t));
    *count = 0;
    while (pos < len) {
        pos = next_line(buf, len, pos, line, sizeof(line));
        ctx.lineno += 1;
        if (is_blank(line))
            continue;
        memcpy(copy, line, sizeof(copy));      /* parse_object splits in place */
        ret = parse_object(line, *objs + nb, &ctx);
        nb += ret == 0;
        if (ret > 0)
            parse_header_line(copy, hdr, &ctx);
    }
    *count = nb;
    return 0;
}
