/*
** ALEXNEX PROJECT, 2026
** editor/ed_settings.c
** File description:
** the level's settings: its song and how its attempts start (FEATURES 11.16)
*/

#include <stdio.h>

#include "editor/ed_settings.h"
#include "sim/modes.h"
#include "sim/speed.h"

static const char *const NAMES[ED_SET_COUNT] = {"Song", "Offset", "BPM",
    "Beat 1", "Gamemode", "Speed", "Size", "Gravity"};

const char *ed_setting_name(ed_setting_t s)
{
    return NAMES[s];
}

static bool song_row(ed_setting_t s)
{
    return s < ED_SET_FIRST_START;
}

void ed_setting_text(const ed_level_t *lv, const library_t *lib,
    ed_setting_t s, char *out, size_t size)
{
    const level_start_t *st = &lv->hdr.start;

    if (song_row(s))
        return ed_song_prop_text(lv, lib, (ed_song_prop_t)s, out, size);
    if (s == ED_SET_MODE)
        snprintf(out, size, "%s", MODES[st->mode].name);
    if (s == ED_SET_SPEED)
        snprintf(out, size, "x%s", speed_word(st->speed));
    if (s == ED_SET_SIZE)
        snprintf(out, size, "%s", st->mini ? "mini" : "normal");
    if (s == ED_SET_GRAVITY)
        snprintf(out, size, "%s", st->gravity_dir < 0 ? "flipped" : "normal");
}

/* As a start position's speed (11.8): nothing is slower than x0.5. */
static void step_speed(level_start_t *st, int dir)
{
    int at = 1;

    for (int i = 0; i < SPEED_COUNT; i++)
        if (SPEED_NAMES[i] == st->speed)
            at = i;
    at += dir;
    at = at < 0 ? 0 : (at >= SPEED_COUNT ? SPEED_COUNT - 1 : at);
    st->speed = SPEED_NAMES[at];
}

static void step_start(level_start_t *st, ed_setting_t s, int dir)
{
    if (s == ED_SET_MODE)
        st->mode = (gamemode_t)((st->mode + dir + MODE_COUNT) % MODE_COUNT);
    if (s == ED_SET_SPEED)
        step_speed(st, dir);
    if (s == ED_SET_SIZE)
        st->mini = !st->mini;
    if (s == ED_SET_GRAVITY)
        st->gravity_dir = -st->gravity_dir;
}

static bool same_start(const level_start_t *a, const level_start_t *b)
{
    return a->mode == b->mode && a->speed == b->speed && a->mini == b->mini
        && a->gravity_dir == b->gravity_dir;
}

bool ed_setting_step(ed_level_t *lv, ed_history_t *h, const library_t *lib,
    ed_setting_t s, int dir, bool big)
{
    level_header_t before = lv->hdr;

    if (song_row(s) && !ed_song_prop_step(lv, lib, (ed_song_prop_t)s, dir,
        big))
        return false;
    if (!song_row(s)) {
        step_start(&lv->hdr.start, s, dir);
        if (same_start(&before.start, &lv->hdr.start))
            return false;
    }
    ed_do_header(lv, h, &before);
    return true;
}
