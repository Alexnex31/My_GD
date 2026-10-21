/*
** ALEXNEX PROJECT, 2026
** sim/interact.c
** File description:
** objects that act once and then lose their hitbox: mode and gravity portals (4.6, 5.1, 5.2)
*/

#include <math.h>
#include <stdlib.h>

#include "sim/internal.h"
#include "sim/modes.h"
#include "sim/speed.h"
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
    double half = player_mode(p)->half;
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
    double height = player_mode(p)->corridor_height;
    double half = player_mode(p)->half;
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
** A mode bigger than the one it replaces can find itself partly under the
** ground it was sliding on: a wave there turning into a cube is 35 px inside
** it. A surface is never crossed (4.3), so the player is put back on it, by
** exactly that much and no further. Objects are left to the contact rules.
** For a mode portal only the ground can be in the way: a new corridor is
** placed around the player, and the ground is the one thing that can cut it
** short (5.2). A player growing back to full size (10.4) can also meet the
** corridor it is in, either side.
*/
static void stay_off_the_surfaces(sim_t *s)
{
    player_t *p = &s->st.player;
    double half = player_mode(p)->half;
    surface_t surf[3];
    int n = current_surfaces(s, surf);

    for (int k = 0; k < n; k++) {
        double rest = surf[k].y - surf[k].side * half;   /* exactly on it */

        if ((p->pos.y - rest) * surf[k].side > 0.0)
            p->pos.y = rest;
    }
}

/*
** A mode portal changes the gamemode and nothing else: same position, same
** speeds, same gravity, same hold (5.2). A same-mode portal still switches
** to its own corridor. The one exception to "same position" is a player
** that grew into the ground.
*/
static void enter_portal(sim_t *s, const object_t *o)
{
    s->st.player.mode = o->portal_mode;
    s->st.player.boost_ticks = 0;            /* a robot's rise doesn't outlive it */
    if (MODES[o->portal_mode].corridor_height > 0.0)
        corridor_from_center(s, o->rect.y + o->rect.h / 2.0);
    else
        s->st.bounds.active = false;         /* its boundaries stop existing now */
    stay_off_the_surfaces(s);
}

/*
** A size portal halves the player or gives it its size back, around the same
** center, and nothing else changes: speeds, gravity, mode (10.4). Growing can
** push it off a surface it would cross; an object it now overlaps is left to
** the contact rules, which kill an inner box inside a block. Shrinking lifts
** its feet off whatever held it: it is in the air until it lands again.
** A robot's rise keeps going, as long in all as the new size allows: the
** ticks it already rose count.
*/
static void enter_mini(sim_t *s, const object_t *o)
{
    player_t *p = &s->st.player;
    int risen = player_mode(p)->rise_ticks - p->boost_ticks;

    if (p->mini == o->portal_mini)
        return;                              /* already that size */
    p->mini = o->portal_mini;
    if (p->boost_ticks > 0)
        p->boost_ticks = risen < player_mode(p)->rise_ticks
            ? player_mode(p)->rise_ticks - risen : 0;
    if (p->mini) {
        p->grounded = false;
        p->surface_rise = 0.0;
    }
    stay_off_the_surfaces(s);
}

/*
** A gravity portal makes the player fall its way, and that is all: the motion
** is kept, the mode is kept and no corridor appears (FEATURES 9.4). One the
** player already agrees with is used up all the same.
*/
static void enter_gravity(sim_t *s, const object_t *o)
{
    if (s->st.player.gravity_dir != o->portal_gravity)
        player_flip_gravity(&s->st.player);
}

/*
** A speed portal sets how fast the player goes right, until the next one
** (FEATURES 10.3). Nothing else does: the heights of jumps don't change,
** only the ground they cover.
*/
static void enter_speed(sim_t *s, const object_t *o)
{
    s->st.player.vx = speed_step(o->speed);
    s->st.speed_mult = speed_px_per_s(o->speed) / SPEED_NORMAL;
}

/*
** A pad or an orb sets the rise speed to its own, whatever the player was
** doing: the same launch every time, and no mode's cap cuts it (3.4). Blue
** and green flip the gravity first, and their speed is in the new gravity:
** toward the new floor for blue, a jump away from it for green.
** An orb uses the hold that activated it (FEATURES 10.1, 10.2). A wave's
** blue orb only flips it: its climb is its input's, every tick.
*/
static void launch(sim_t *s, const object_t *o)
{
    player_t *p = &s->st.player;

    if (o->type == OBJ_ORB && player_mode(p)->orbs_flip_only) {
        player_flip_gravity(p);
        p->hold = HOLD_USED;
        return;
    }
    if (LAUNCHES[o->launch].flips)
        player_flip_gravity(p);
    p->vy = PER_TICK(launch_speed_for(o, p->mini) * V_UNIT);
    p->boost_ticks = 0;
    p->grounded = false;
    p->can_jump = false;
    if (o->type == OBJ_ORB)
        p->hold = HOLD_USED;
}

/*
** An orb needs a hold nothing has used yet; the others only a touch (5.1).
** A wave uses no orb but a blue one (FEATURES 10.2): it flies through the
** others, which stay live.
*/
static bool interactive_wants_activation(const sim_t *s, const object_t *o)
{
    const player_t *p = &s->st.player;

    if (o->type != OBJ_ORB)
        return true;
    if (player_mode(p)->orbs_flip_only && o->launch != LAUNCH_BLUE)
        return false;
    return p->hold == HOLD_FRESH;
}

static void interactive_act(sim_t *s, const object_t *o)
{
    if (o->type == OBJ_PORTAL)
        enter_portal(s, o);
    if (o->type == OBJ_GRAVITY)
        enter_gravity(s, o);
    if (o->type == OBJ_SPEED)
        enter_speed(s, o);
    if (o->type == OBJ_MINI)
        enter_mini(s, o);
    if (o->type == OBJ_PAD || o->type == OBJ_ORB)
        launch(s, o);
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

/*
** Tick step 0: a hold that is fresh while the player is already inside a
** live orb (it flew in without pressing, and the press comes now). The orb
** acts before the mode's own input and takes the click: nothing else may
** jump, hop or flip on it this tick, so the orb wins over a surface jump
** (FEATURES 10.2). The button stays down: what flies by holding it, a ship
** or a wave, keeps doing so. One orb per hold: the first in the level's
** order.
*/
input_t activate_orbs(sim_t *s, input_t in)
{
    const player_t *p = &s->st.player;
    double half = player_mode(p)->half;

    if (p->hold != HOLD_FRESH)
        return in;
    for (size_t i = s->st.first_active; i < s->lvl.nb_objects; i++) {
        const object_t *o = &s->lvl.objects[i];

        if (o->hitbox.aabb.x > p->pos.x + half)
            break;                           /* sorted: nothing further touches */
        if (o->type != OBJ_ORB || is_spent(&s->st, i)
            || !interactive_wants_activation(s, o)
            || !overlap_box_poly(p->pos, half, &o->hitbox))
            continue;
        touch_interactive(s, i);
        return (input_t){.held = in.held || in.pressed, .pressed = false};
    }
    return in;
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
