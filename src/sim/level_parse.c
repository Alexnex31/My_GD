/*
** ALEXNEX PROJECT, 2026
** sim/level_parse.c
** File description:
** reads a level file's text into objects (7.2, 7.3)
*/

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim/alloc.h"
#include "sim/hitbox.h"
#include "sim/level.h"
#include "sim/modes.h"
#include "sim/speed.h"

#define MAX_TOKENS 64          /* words on one line: more is no line of ours */
#define LINE_MAX_LEN 1023      /* characters on one line, its newline aside   */

typedef struct parse_ctx {
    sim_log_fn log;
    const char *source;
    int lineno;
    bool group_warned;
    bool full_warned;          /* the object limit, said once                  */
    char *extra;              /* where the fields nothing edits are kept, or NULL */
    size_t extra_size;
} parse_ctx_t;

static void warn(parse_ctx_t *ctx, const char *fmt, ...)
{
    char what[192];
    char msg[256];
    va_list ap;

    if (ctx->log == NULL)
        return;
    va_start(ap, fmt);
    vsnprintf(what, sizeof(what), fmt, ap);
    va_end(ap);
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

/*
** Splits on spaces and tabs, in place; a '#' ends the line (7.2). More than
** max words returns max + 1: the caller skips the line rather than read a
** part of it.
*/
static int split_words(char *line, char **tok, int max)
{
    int n = 0;

    while (*line != '\0') {
        while (*line == ' ' || *line == '\t')
            line += 1;
        if (*line == '#' || *line == '\0')
            break;
        if (n == max)
            return max + 1;
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

/*
** w and h are grid units of 50 px, any decimal down to one pixel: a portal's
** own width is 1.2 blocks, and the editor's steps add whole ones to it (7.2).
*/
static int parse_units(const char *val, double *px)
{
    double units = 0.0;

    if (!parse_double(val, &units) || units * UNIT < 1.0)
        return -1;
    *px = units * UNIT;
    return 0;
}

/*
** A portal is not the square cell it is written in: it is 1.2 blocks wide and
** 2.8 tall, GD's own proportions (4.2). Each axis is derived only when the
** line doesn't give it, and a derived axis is centered on the cell the level
** wrote, so the corridor a mode portal opens (5.2) stays where the level put
** it. An explicit w= or h= anchors at the cell's corner, like every other
** object (7.2): the line said exactly what it wanted.
*/
static void portal_shape(object_t *o, bool given_w, bool given_h)
{
    double block = o->size * UNIT;            /* the cell as written: 1 block */
    vec2_t center = {o->rect.x + block / 2.0, o->rect.y + block / 2.0};

    if (o->type != OBJ_PORTAL && o->type != OBJ_GRAVITY
        && o->type != OBJ_SPEED && o->type != OBJ_MINI)
        return;
    if (!given_w) {                          /* derived: centered on the cell */
        o->rect.w = block * PORTAL_BOX_W;
        o->rect.x = center.x - o->rect.w / 2.0;
    }
    if (!given_h) {
        o->rect.h = block * PORTAL_BOX_H;
        o->rect.y = center.y - o->rect.h / 2.0;
    }
}

/* A field this version doesn't act on, kept as written for the next save. */
static void keep_extra(parse_ctx_t *ctx, const char *key, const char *val)
{
    size_t used;

    if (ctx->extra == NULL)
        return;
    used = strlen(ctx->extra);
    snprintf(ctx->extra + used, ctx->extra_size - used, " %s=%s", key, val);
}

/* A turn is any angle a person writes: past a million degrees it is noise. */
static bool parse_rotation(const char *val, double *deg)
{
    return parse_double(val, deg) && fabs(*deg) <= LEVEL_ROT_MAX;
}

typedef enum field_result {
    FIELD_OK, FIELD_UNKNOWN, FIELD_BAD_VALUE, FIELD_NOT_A_FIELD
} field_result_t;

/*
** One key=value field, split in place: tok is then the key. An unknown key
** is kept as written and ignored, the line stays (7.2); its warning waits
** until the whole line is known to load.
*/
static field_result_t parse_field(object_t *o, char *tok, parse_ctx_t *ctx)
{
    char *val = strchr(tok, '=');

    if (val == NULL || val == tok)
        return FIELD_NOT_A_FIELD;
    *val = '\0';
    val += 1;
    if (strcmp(tok, "rot") == 0)
        return parse_rotation(val, &o->rotation) ? FIELD_OK : FIELD_BAD_VALUE;
    if (strcmp(tok, "w") == 0)
        return parse_units(val, &o->rect.w) == 0 ? FIELD_OK : FIELD_BAD_VALUE;
    if (strcmp(tok, "h") == 0)
        return parse_units(val, &o->rect.h) == 0 ? FIELD_OK : FIELD_BAD_VALUE;
    keep_extra(ctx, tok, val);
    return strcmp(tok, "group") == 0 ? FIELD_OK : FIELD_UNKNOWN;
}

/* What a field that stops the line was, said with the field itself. */
static void bad_field(parse_ctx_t *ctx, field_result_t r, const char *tok)
{
    if (r == FIELD_NOT_A_FIELD)
        warn(ctx, "unexpected word '%.40s', line skipped", tok);
    else
        warn(ctx, "invalid value for %.20s= ('%.40s'), line skipped", tok,
            tok + strlen(tok) + 1);
}

/* The fields the line kept without using, once it is known to load. */
static void warn_ignored(parse_ctx_t *ctx, char **tok, const bool *unknown,
    int n)
{
    for (int i = 0; i < n; i++) {
        if (unknown[i])
            warn(ctx, "unknown field %.20s=, ignored", tok[i]);
        if (!unknown[i] && strcmp(tok[i], "group") == 0 && !ctx->group_warned) {
            warn(ctx, "group= is reserved for triggers, ignored");
            ctx->group_warned = true;
        }
    }
}

static const char *const TYPE_NAMES[OBJ_TYPE_COUNT] = {
    [OBJ_BLOCK] = "block", [OBJ_SLOPE] = "slope",
    [OBJ_SPIKE] = "spike", [OBJ_PORTAL] = "portal",
    [OBJ_GRAVITY] = "gravity", [OBJ_PAD] = "pad", [OBJ_ORB] = "orb",
    [OBJ_SAW] = "saw", [OBJ_SPEED] = "speed", [OBJ_MINI] = "mini",
};

const char *obj_type_name(obj_type_t type)
{
    return TYPE_NAMES[type];
}

static int type_from_name(const char *name, obj_type_t *out)
{
    const char *const *names = TYPE_NAMES;

    for (int i = 0; i < OBJ_TYPE_COUNT; i++)
        if (names[i] != NULL && strcmp(names[i], name) == 0) {
            *out = (obj_type_t)i;
            return 0;
        }
    return -1;
}

/* A gravity portal's word: the way it makes the player fall (FEATURES 9.4). */
static int gravity_word(object_t *o, const char *word, parse_ctx_t *ctx)
{
    if (word != NULL && strcmp(word, "up") == 0)
        o->portal_gravity = -1;
    else if (word != NULL && strcmp(word, "down") == 0)
        o->portal_gravity = 1;
    else {
        warn(ctx, "gravity needs up or down, line skipped");
        return -1;
    }
    hitbox_for_object(o);
    return 0;
}

/* A size portal's word: on makes the player mini, off full size (10.4). */
static int mini_word(object_t *o, const char *word, parse_ctx_t *ctx)
{
    if (word != NULL && strcmp(word, "on") == 0)
        o->portal_mini = true;
    else if (word != NULL && strcmp(word, "off") == 0)
        o->portal_mini = false;
    else {
        warn(ctx, "mini needs on or off, line skipped");
        return -1;
    }
    hitbox_for_object(o);
    return 0;
}

/* A pad's or an orb's word: its colour, one that object comes in (10.1, 10.2). */
static int launch_word(object_t *o, const char *word, parse_ctx_t *ctx)
{
    int kind = word == NULL ? -1 : launch_from_name(word);

    if (kind >= 0)
        o->launch = (launch_kind_t)kind;
    if (kind < 0 || launch_speed(o) == 0.0) {
        warn(ctx, o->type == OBJ_PAD
            ? "pad needs yellow, pink, red or blue, line skipped"
            : "orb needs yellow, pink, red, blue, green or black, line skipped");
        return -1;
    }
    hitbox_for_object(o);
    return 0;
}

/* The speeds of a speed portal (FEATURES 10.3): 0.5, 1, 2, 3 or 4. */
static bool known_speed(const char *text, double *out)
{
    double v = 0.0;

    if (!parse_double(text, &v) || !speed_is_known(v))
        return false;
    *out = v;
    return true;
}

/* A speed portal's word: one of GD's five speeds, by its name (10.3). */
static int speed_word_of(object_t *o, const char *word, parse_ctx_t *ctx)
{
    if (word == NULL || !known_speed(word, &o->speed)) {
        warn(ctx, "speed needs 0.5, 1, 2, 3 or 4, line skipped");
        return -1;
    }
    hitbox_for_object(o);
    return 0;
}

/* The extra word (portals need a mode, 5.5), then the hitbox. */
static int object_init(object_t *o, const char *word, parse_ctx_t *ctx)
{
    int mode = 0;

    if (o->type == OBJ_GRAVITY)
        return gravity_word(o, word, ctx);
    if (o->type == OBJ_PAD || o->type == OBJ_ORB)
        return launch_word(o, word, ctx);
    if (o->type == OBJ_SPEED)
        return speed_word_of(o, word, ctx);
    if (o->type == OBJ_MINI)
        return mini_word(o, word, ctx);
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

/*
** Finite isn't enough: the renderer cuts the level into chunks up to its
** rightmost object, so one object at x = 1e12 asks for a billion (7.2).
*/
static bool in_range(const rect_t *r)
{
    return fabs(r->x) <= LEVEL_COORD_MAX && fabs(r->y) <= LEVEL_COORD_MAX
        && r->w <= LEVEL_COORD_MAX && r->h <= LEVEL_COORD_MAX;
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
    bool given_h = false;
    bool given_w = false;
    bool unknown[MAX_TOKENS] = {false};
    field_result_t r;

    if (n < 1 || type_from_name(tok[0], &o->type) != 0)
        return 1;                             /* not an object: a header field */
    if (n > MAX_TOKENS) {
        warn(ctx, "more than %d words, line skipped", MAX_TOKENS);
        return -1;
    }
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
    for (int i = first_field; i < n; i++) {
        given_h = given_h || strncmp(tok[i], "h=", 2) == 0;
        given_w = given_w || strncmp(tok[i], "w=", 2) == 0;
        r = parse_field(o, tok[i], ctx);       /* it splits tok in place */
        if (r != FIELD_OK && r != FIELD_UNKNOWN) {
            bad_field(ctx, r, tok[i]);
            return -1;
        }
        unknown[i] = r == FIELD_UNKNOWN;
    }
    portal_shape(o, given_w, given_h);
    if (!in_range(&o->rect)) {
        warn(ctx, "object out of range (10000000 px), line skipped");
        return -1;
    }
    if (object_init(o, word, ctx) != 0)
        return -1;
    warn_ignored(ctx, tok + first_field, unknown + first_field,
        n - first_field);
    return 0;
}

/* Around a value: spaces, tabs, and a \r, which a line end would swallow. */
static bool is_trim(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/*
** The rest of a header line, trimmed, into a fixed buffer (7.2). An empty
** value keeps the field's default instead of blanking it. Text that doesn't
** fit is cut with a warning; a number or a word that doesn't fit is no
** number or word at all (cut_ok false), and the line is ignored.
*/
static bool take_text(const char *line, char *out, size_t size,
    bool cut_ok, parse_ctx_t *ctx)
{
    size_t len;

    while (is_trim(*line))
        line += 1;
    len = strlen(line);
    while (len > 0 && is_trim(line[len - 1]))
        len -= 1;
    if (len == 0) {
        warn(ctx, "header field has no value, ignored");
        return false;
    }
    if (len >= size && !cut_ok) {
        warn(ctx, "header field value too long, ignored");
        return false;
    }
    if (len >= size) {
        warn(ctx, "header field too long, truncated");
        len = size - 1;
    }
    memcpy(out, line, len);
    out[len] = '\0';
    return true;
}

/*
** One of two words into *on. Both are written, so a later "normal" undoes an
** earlier "flipped": the last line wins (7.2). Anything else changes nothing.
*/
static void set_flag(const char *text, const char *off, const char *on,
    bool *out, parse_ctx_t *ctx)
{
    if (strcmp(text, on) == 0 || strcmp(text, off) == 0)
        *out = strcmp(text, on) == 0;
    else
        warn(ctx, "header field takes one of its two words, ignored");
}


static bool start_speed(const char *text, level_start_t *st, parse_ctx_t *ctx)
{
    if (!known_speed(text, &st->speed))
        warn(ctx, "start_speed must be 0.5, 1, 2, 3 or 4, ignored");
    return true;
}

/*
** start x y mode [up|down] [speed] [mini]: another place an attempt can begin, for
** practice (FEATURES 11.8). It has no hitbox and is no object: the run never
** meets it. 0 kept, -1 skipped, 1 not a start line at all.
*/
static int parse_start(char *line, level_start_t *st, parse_ctx_t *ctx)
{
    char *tok[MAX_TOKENS];
    int n = split_words(line, tok, MAX_TOKENS);
    int mode = n >= 4 && n <= MAX_TOKENS ? mode_from_name(tok[3]) : -1;
    int next = 4;

    if (n < 1 || strcmp(tok[0], "start") != 0)
        return 1;
    *st = (level_start_t){.speed = 1.0, .gravity_dir = 1};
    if (n < 4 || n > MAX_TOKENS || mode < 0 || !parse_double(tok[1], &st->pos.x)
        || !parse_double(tok[2], &st->pos.y)) {
        warn(ctx, "start needs x y and a gamemode, line skipped");
        return -1;
    }
    if (fabs(st->pos.x) > LEVEL_COORD_MAX || fabs(st->pos.y) > LEVEL_COORD_MAX) {
        warn(ctx, "start out of range (10000000 px), line skipped");
        return -1;
    }
    st->mode = (gamemode_t)mode;
    if (next < n && (strcmp(tok[next], "up") == 0
        || strcmp(tok[next], "down") == 0)) {
        st->gravity_dir = strcmp(tok[next], "up") == 0 ? -1 : 1;
        next += 1;
    }
    if (next < n && known_speed(tok[next], &st->speed))
        next += 1;
    if (next < n && strcmp(tok[next], "mini") == 0) {
        st->mini = true;
        next += 1;
    }
    if (next < n) {
        warn(ctx, "start takes up or down, a speed, then mini, line skipped");
        return -1;
    }
    return 0;
}

/*
** How far each number may go (7.2): the start like any start position, the
** beat grid and the song's times within reason, so nothing downstream turns
** a huge one into a count of beats or ticks it can't hold.
*/
static bool header_in_range(const char *key, double v)
{
    if (strcmp(key, "start_x") == 0 || strcmp(key, "start_y") == 0)
        return fabs(v) <= LEVEL_COORD_MAX;
    if (strcmp(key, "bpm") == 0)
        return v >= 0.0 && v <= LEVEL_BPM_MAX;
    return fabs(v) <= LEVEL_SECONDS_MAX;
}

static bool header_number(const char *key, const char *value,
    level_header_t *hdr, parse_ctx_t *ctx)
{
    char text[64];
    double v = 0.0;

    if (strcmp(key, "start_speed") == 0)
        return take_text(value, text, sizeof(text), false, ctx)
            ? start_speed(text, &hdr->start, ctx) : true;
    if (strcmp(key, "music_offset") != 0 && strcmp(key, "bpm") != 0
        && strcmp(key, "first_beat") != 0 && strcmp(key, "start_x") != 0
        && strcmp(key, "start_y") != 0)
        return false;
    if (!take_text(value, text, sizeof(text), false, ctx))
        return true;
    if (!parse_double(text, &v)) {
        warn(ctx, "header field needs a number, ignored");
        return true;
    }
    if (!header_in_range(key, v)) {
        warn(ctx, "%s out of range, ignored", key);
        return true;
    }
    hdr->music_offset = strcmp(key, "music_offset") == 0 ? v : hdr->music_offset;
    hdr->bpm = strcmp(key, "bpm") == 0 ? v : hdr->bpm;
    hdr->first_beat = strcmp(key, "first_beat") == 0 ? v : hdr->first_beat;
    hdr->start.pos.x = strcmp(key, "start_x") == 0 ? v : hdr->start.pos.x;
    hdr->start.pos.y = strcmp(key, "start_y") == 0 ? v : hdr->start.pos.y;
    return true;
}

/* The three word fields: a gamemode name, normal|flipped, normal|mini. */
static bool header_word(const char *key, const char *value,
    level_header_t *hdr, parse_ctx_t *ctx)
{
    char text[64];
    int mode = 0;
    bool on = false;

    if (strcmp(key, "start_gamemode") != 0 && strcmp(key, "start_gravity") != 0
        && strcmp(key, "start_size") != 0)
        return false;
    if (!take_text(value, text, sizeof(text), false, ctx))
        return true;
    mode = mode_from_name(text);
    if (strcmp(key, "start_gamemode") == 0 && mode < 0)
        return (warn(ctx, "unknown gamemode, ignored"), true);
    if (strcmp(key, "start_gamemode") == 0)
        return (hdr->start.mode = (gamemode_t)mode, true);
    if (strcmp(key, "start_gravity") == 0) {
        on = hdr->start.gravity_dir < 0;
        set_flag(text, "normal", "flipped", &on, ctx);
        hdr->start.gravity_dir = on ? -1 : 1;
        return true;
    }
    set_flag(text, "normal", "mini", &hdr->start.mini, ctx);
    return true;
}

/* The text fields of 7.2, and how much room each one has. */
static char *header_text_field(const char *key, level_header_t *hdr,
    size_t *size)
{
    if (strcmp(key, "name") == 0)
        return (*size = sizeof(hdr->name), hdr->name);
    if (strcmp(key, "author") == 0)
        return (*size = sizeof(hdr->author), hdr->author);
    if (strcmp(key, "music") == 0)
        return (*size = sizeof(hdr->music), hdr->music);
    return NULL;
}

/*
** Every line that doesn't start with an object type is a header field, and
** blanks before its key don't change that. False: no field of ours.
*/
static bool parse_header_line(char *line, level_header_t *hdr,
    parse_ctx_t *ctx)
{
    char key[32];
    char *dst = NULL;
    size_t size = 0;
    size_t klen = 0;

    while (*line == ' ' || *line == '\t')
        line += 1;
    while (line[klen] != '\0' && line[klen] != ' ' && line[klen] != '\t')
        klen += 1;
    if (klen >= sizeof(key)) {
        warn(ctx, "unknown header field, ignored");
        return false;
    }
    memcpy(key, line, klen);
    key[klen] = '\0';
    dst = header_text_field(key, hdr, &size);
    if (dst != NULL)
        take_text(line + klen, dst, size, true, ctx);
    else if (!header_number(key, line + klen, hdr, ctx)
        && !header_word(key, line + klen, hdr, ctx)) {
        warn(ctx, "unknown header field '%.31s', ignored", key);
        return false;
    }
    return true;
}

/*
** One line of the buffer, without its newline or a trailing \r: where it
** starts and how long it is. A NUL ends its text, as it does for every C
** string after it. Copied out only when it fits; too long is reported, not
** cut, so a line is never read in part (7.3).
*/
static size_t next_line(const char *buf, size_t len, size_t pos, char *out,
    size_t *n_out)
{
    size_t end = pos;
    size_t n;

    while (end < len && buf[end] != '\n')
        end += 1;
    n = strnlen(buf + pos, end - pos);
    while (n > 0 && buf[pos + n - 1] == '\r')
        n -= 1;
    *n_out = n;
    if (n <= LINE_MAX_LEN) {
        memcpy(out, buf + pos, n);
        out[n] = '\0';
    }
    return end < len ? end + 1 : len;
}

/* A '#' ends the line, in the header as well as in the body (7.2). */
static void strip_comment(char *line)
{
    char *hash = strchr(line, '#');

    if (hash != NULL)
        *hash = '\0';
}

static bool is_blank(const char *line)
{
    while (*line == ' ' || *line == '\t')
        line += 1;
    return *line == '\0' || *line == '#';
}

/* Header only: an object line is recognized by its first word and skipped. */
static int skip_object(char *line)
{
    char *tok[1];
    obj_type_t type;

    if (split_words(line, tok, 1) < 1)
        return 1;
    if (strcmp(tok[0], "start") == 0)
        return 0;
    return type_from_name(tok[0], &type) != 0;
}

typedef struct parse_out {    /* what the caller wants filled, each or NULL */
    object_t **objs;
    char ***extras;
    level_start_t **starts;
    char ***kept;
    size_t *count;
    size_t *nb_starts;
    size_t *nb_kept;
    size_t cap_objs;          /* the arrays grow as lines come: a file of */
    size_t cap_starts;        /* comments allocates nothing (7.3)          */
    size_t cap_kept;
} parse_out_t;

/* Room for one more element at index n, doubling: the arrays only grow. */
static void grow(void **arr, size_t *cap, size_t n, size_t elem)
{
    char *bigger;

    if (n < *cap)
        return;
    *cap = *cap == 0 ? 64 : *cap * 2;
    bigger = sim_xcalloc(*cap, elem);
    if (n > 0)
        memcpy(bigger, *arr, n * elem);
    free(*arr);
    *arr = bigger;
}

/*
** A line the document keeps as it was, written back unchanged by the editor:
** a header field this build doesn't know, a line it skipped (7.2).
*/
static void keep_line(parse_out_t *out, const char *raw, size_t n)
{
    char *copy;

    if (out->kept == NULL)
        return;
    grow((void **)out->kept, &out->cap_kept, *out->nb_kept, sizeof(char *));
    copy = sim_xcalloc(n + 1, 1);
    memcpy(copy, raw, n);
    (*out->kept)[*out->nb_kept] = copy;
    *out->nb_kept += 1;
}

static int parse_start_line(char *copy, parse_out_t *out, parse_ctx_t *ctx)
{
    level_start_t st;
    int ret = parse_start(copy, &st, ctx);

    if (ret == 0 && out->starts != NULL) {
        grow((void **)out->starts, &out->cap_starts, *out->nb_starts,
            sizeof(level_start_t));
        (*out->starts)[*out->nb_starts] = st;
        *out->nb_starts += 1;
    }
    return ret;
}

/* An object line once the level holds all it may: skipped, said once, and
** not kept, or the kept lines would be the level. */
static int full(parse_ctx_t *ctx)
{
    if (!ctx->full_warned)
        warn(ctx, "more than %d objects, the rest skipped", LEVEL_OBJECTS_MAX);
    ctx->full_warned = true;
    return -2;
}

static int parse_object_line(char *copy, parse_out_t *out, parse_ctx_t *ctx)
{
    char extra[LINE_MAX_LEN + 2] = "";
    obj_type_t type;
    char *tok[1];
    int ret;

    if (out->objs == NULL)
        return skip_object(copy);
    if (*out->count >= LEVEL_OBJECTS_MAX) {
        if (split_words(copy, tok, 1) >= 1 && type_from_name(tok[0], &type) == 0)
            return full(ctx);
        return 1;
    }
    if (*out->count == out->cap_objs) {
        size_t cap = out->cap_objs;          /* the extras grow alongside */

        grow((void **)out->objs, &out->cap_objs, *out->count, sizeof(object_t));
        if (out->extras != NULL)
            grow((void **)out->extras, &cap, *out->count, sizeof(char *));
    }
    ctx->extra = out->extras != NULL ? extra : NULL;
    ctx->extra_size = sizeof(extra);
    ret = parse_object(copy, *out->objs + *out->count, ctx);
    if (ret == 0) {
        if (out->extras != NULL && extra[0] != '\0')
            (*out->extras)[*out->count] = sim_xstrdup(extra);
        *out->count += 1;
    }
    return ret;
}

/*
** One line that isn't blank: a start, an object, or else a header field.
** Below 0: a line skipped. A line of no use is kept for the next save.
*/
static int parse_line(char *line, const char *raw, size_t n,
    parse_out_t *out, level_header_t *hdr, parse_ctx_t *ctx)
{
    char copy[LINE_MAX_LEN + 1];
    int ret;

    memcpy(copy, line, sizeof(copy));         /* the parsers split in place */
    ret = parse_start_line(copy, out, ctx);
    if (ret > 0) {
        memcpy(copy, line, sizeof(copy));
        ret = parse_object_line(copy, out, ctx);
    }
    if (ret > 0 && !parse_header_line(line, hdr, ctx))
        keep_line(out, raw, n);
    if (ret == -1)
        keep_line(out, raw, n);
    return ret;
}

/* The UTF-8 mark some editors put first is no part of the first line. */
static size_t skip_bom(const char *buf, size_t len)
{
    if (len >= 3 && (unsigned char)buf[0] == 0xEF
        && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
        return 3;
    return 0;
}

static int parse_all(const char *buf, size_t len, const char *source,
    parse_out_t *out, level_header_t *hdr, sim_log_fn log)
{
    parse_ctx_t ctx = {.log = log, .source = source};
    char line[LINE_MAX_LEN + 1];
    size_t pos = skip_bom(buf, len);
    size_t start;
    size_t n = 0;
    int skipped = 0;

    while (pos < len) {
        start = pos;
        pos = next_line(buf, len, pos, line, &n);
        ctx.lineno += 1;
        if (n > LINE_MAX_LEN) {
            warn(&ctx, "line longer than %d characters, skipped", LINE_MAX_LEN);
            keep_line(out, buf + start, n);
            skipped += 1;
            continue;
        }
        strip_comment(line);
        if (is_blank(line))
            continue;
        skipped += parse_line(line, buf + start, n, out, hdr, &ctx) < 0;
    }
    return skipped;
}

int level_parse_mem(const char *buf, size_t len, const char *source,
    object_t **objs, size_t *count, level_header_t *hdr, sim_log_fn log)
{
    size_t nb = 0;
    size_t nb_starts = 0;
    parse_out_t out = {.objs = objs, .count = &nb, .nb_starts = &nb_starts};
    int skipped;

    if (objs != NULL)
        *objs = NULL;
    skipped = parse_all(buf, len, source, &out, hdr, log);
    if (objs != NULL && *objs == NULL)
        *objs = sim_xcalloc(1, sizeof(object_t));
    if (count != NULL)
        *count = nb;
    return skipped;
}

/* Every array is allocated, empty or not: a document is freed one way. */
int level_parse_doc(const char *buf, size_t len, const char *source,
    level_doc_t *doc, level_header_t *hdr, sim_log_fn log)
{
    parse_out_t out = {&doc->objs, &doc->extras, &doc->starts, &doc->kept,
        &doc->count, &doc->nb_starts, &doc->nb_kept, 0, 0, 0};
    int skipped;

    *doc = (level_doc_t){0};
    skipped = parse_all(buf, len, source, &out, hdr, log);
    if (doc->objs == NULL) {
        doc->objs = sim_xcalloc(1, sizeof(object_t));
        doc->extras = sim_xcalloc(1, sizeof(char *));
    }
    if (doc->starts == NULL)
        doc->starts = sim_xcalloc(1, sizeof(level_start_t));
    if (doc->kept == NULL)
        doc->kept = sim_xcalloc(1, sizeof(char *));
    return skipped;
}

void level_doc_free(level_doc_t *doc)
{
    for (size_t i = 0; doc->extras != NULL && i < doc->count; i++)
        free(doc->extras[i]);
    for (size_t i = 0; doc->kept != NULL && i < doc->nb_kept; i++)
        free(doc->kept[i]);
    free(doc->extras);
    free(doc->kept);
    free(doc->objs);
    free(doc->starts);
    *doc = (level_doc_t){0};
}

/* The loader's own reading of one line: what the editor places is exactly
** what the game will load. */
int level_object_from_line(const char *line, object_t *o)
{
    parse_ctx_t ctx = {0};
    char copy[LINE_MAX_LEN + 1];

    snprintf(copy, sizeof(copy), "%s", line);
    return parse_object(copy, o, &ctx) == 0 ? 0 : -1;
}
