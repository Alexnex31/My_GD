/*
** ALEXNEX PROJECT, 2026
** fx/trail.c
** File description:
** the wave's trail: the corners of its path, nothing drawn here (FEATURES 8.5)
*/

#include <string.h>

#include "fx/trail.h"

#define DIR_UNKNOWN 2         /* a trail that has not moved yet              */
#define LEVEL_EPSILON 1e-6    /* px: a slide along a ceiling's skin is level */

static void push(trail_t *t, vec2_t pt)
{
    if (t->n == TRAIL_CAP) {                 /* full: the oldest goes */
        memmove(t->pts, t->pts + 1, (TRAIL_CAP - 1) * sizeof(vec2_t));
        t->n -= 1;
    }
    t->pts[t->n] = pt;
    t->n += 1;
}

static void begin(trail_t *t, const player_t *p)
{
    t->n = 0;
    push(t, p->pos);
    t->dir = DIR_UNKNOWN;
    t->mini = p->mini;
    t->tick_mini = p->mini;
    t->was_wave = true;
}

/* An attempt that starts as a wave has its trail from the first pixel. */
void trail_reset(trail_t *t, const player_t *p)
{
    t->n = 0;
    t->was_wave = false;
    if (p->mode == MODE_WAVE)
        begin(t, p);
}

/* Which way the tick really went: a wave sliding on a surface goes level. */
static int tick_dir(const player_t *p)
{
    double up = p->prev_pos.y - p->pos.y;

    if (up > LEVEL_EPSILON)
        return 1;
    return up < -LEVEL_EPSILON ? -1 : 0;
}

/*
** A corner is where the tick that turned started: a new way, or a new slope
** after a size portal, which keeps the way. A portal acts at the end of its
** tick, so a trail begins where that tick ended, the tick after a size
** portal is the first of the new slope, and the tick that leaves wave mode
** ends the trail.
*/
void trail_after_tick(trail_t *t, const player_t *p)
{
    int dir;

    if (p->mode != MODE_WAVE) {
        t->n = 0;
        t->was_wave = false;
        return;
    }
    if (!t->was_wave)
        return begin(t, p);
    dir = tick_dir(p);
    if (t->dir != DIR_UNKNOWN && (dir != t->dir
        || (dir != 0 && t->mini != t->tick_mini)))
        push(t, p->prev_pos);
    t->dir = dir;
    t->tick_mini = t->mini;
    t->mini = p->mini;
}

/*
** Corners left of the view go, except the last of them: it is the start of
** the line that enters the screen.
*/
void trail_drop_left_of(trail_t *t, double x)
{
    size_t gone = 0;

    while (gone + 1 < t->n && t->pts[gone + 1].x < x)
        gone += 1;
    if (gone == 0)
        return;
    memmove(t->pts, t->pts + gone, (t->n - gone) * sizeof(vec2_t));
    t->n -= gone;
}
