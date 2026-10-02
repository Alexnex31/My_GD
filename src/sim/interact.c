/*
** ALEXNEX PROJECT, 2026
** sim/interact.c
** File description:
** objects that act once and then lose their hitbox: portals (4.6, 5.1, 5.2)
*/

#include <math.h>
#include <stdlib.h>

#include "sim/internal.h"
#include "sim/modes.h"
#include "sim/sweep.h"

static bool already_listed(const sim_t *s, size_t i)
{
    for (size_t k = 0; k < s->nb_touch; k++)
        if (s->touch[k].index == i)
            return true;
    return false;
}

/*
** The live interactive objects the rigid square meets on this leg, with where
** on the tick's path it met them (4.6). Swept, so a fast player skips none.
*/
void leg_touches(sim_t *s, vec2_t d, double t_end)
{
    const player_t *p = &s->st.player;
    double half = MODES[p->mode].half;
    double t;

    for (size_t k = 0; k < s->nb_cand; k++) {
        size_t i = s->cand[k];
        const object_t *o = &s->lvl.objects[i];

        if (OBJ_CATEGORY[o->type] != CAT_INTERACTIVE || is_spent(&s->st, i)
            || already_listed(s, i) || s->nb_touch >= MAX_TOUCHES)
            continue;
        t = sweep_box_touch(p->pos, half, d, &o->hitbox);
        if (t > t_end)
            continue;
        s->touch[s->nb_touch].index = i;
        s->touch[s->nb_touch].at = s->legs + t;
        s->nb_touch += 1;
    }
}

/* Path position first, then the object's index: never qsort's own order. */
static int touch_cmp(const void *a, const void *b)
{
    const touch_t *ta = a;
    const touch_t *tb = b;

    if (ta->at != tb->at)
        return (ta->at > tb->at) - (ta->at < tb->at);
    return (ta->index > tb->index) - (ta->index < tb->index);
}

/*
** The corridor a mode opens: as tall as the mode says, centered on the given
** y, snapped to the grid, and never below the ground (5.2). A portal centers
** it on itself; the start of an attempt centers it on the player (7.2).
** A portal taller than the corridor can be touched outside it: the corridor
** then moves along the grid just enough to hold the player, which a portal
** never moves.
*/
void corridor_from_center(sim_t *s, double center)
{
    const player_t *p = &s->st.player;
    double height = MODES[p->mode].corridor_height;
    double half = MODES[p->mode].half;
    double top = center - height / 2.0;
    ship_bounds_t *b = &s->st.bounds;

    top = floor(top / UNIT + 0.5) * UNIT;    /* the nearest grid line, ties down */
    if (p->pos.y - half < top)
        top = floor((p->pos.y - half) / UNIT) * UNIT;
    if (p->pos.y + half > top + height)
        top = ceil((p->pos.y + half) / UNIT) * UNIT - height;
    if (top + height > GROUND_Y)
        top = GROUND_Y - height;
    b->active = true;
    b->top = top;
    b->bottom = top + height;   /* the camera eases onto it, camera_follow (3.5) */
}

/*
** A mode portal changes the gamemode and nothing else: same position, same
** speeds, same gravity, same hold (5.2). A same-mode portal still switches
** to its own corridor.
*/
static void enter_portal(sim_t *s, const object_t *o)
{
    s->st.player.mode = o->portal_mode;
    if (MODES[o->portal_mode].corridor_height > 0.0)
        corridor_from_center(s, o->rect.y + o->rect.h / 2.0);
    else
        s->st.bounds.active = false;         /* its boundaries stop existing now */
}

static bool interactive_wants_activation(const sim_t *s, const object_t *o)
{
    (void)s;
    return o->type == OBJ_PORTAL;            /* pads, orbs and the rest: FEATURES */
}

static void interactive_act(sim_t *s, const object_t *o)
{
    if (o->type == OBJ_PORTAL)
        enter_portal(s, o);
}

/* It acts once, then keeps its sprite and loses its hitbox (5.1). */
static void touch_interactive(sim_t *s, size_t i)
{
    const object_t *o = &s->lvl.objects[i];

    if (!interactive_wants_activation(s, o))
        return;
    interactive_act(s, o);
    set_spent(&s->st, i);
}

void apply_interactive(sim_t *s)
{
    qsort(s->touch, s->nb_touch, sizeof(touch_t), touch_cmp);
    for (size_t k = 0; k < s->nb_touch; k++) {
        if (!s->st.player.alive)
            return;
        if (!is_spent(&s->st, s->touch[k].index))
            touch_interactive(s, s->touch[k].index);
    }
}
