/*
** ALEXNEX PROJECT, 2026
** practice
** File description:
** practice mode: checkpoints are run-state snapshots, a death goes back to the last (PLAN 13.1)
*/

#include "mygd.h"

#define DIAMOND 36.0f                 /* the marker's side, world pixels */

static const sfColor CHECKPOINT = {60, 220, 90, 230};

/* Every checkpoint goes: leaving practice, changing start, leaving the level. */
void practice_clear(level_t *lv)
{
    for (size_t i = 0; i < lv->nb_checkpoints; i++)
        sim_snapshot_free(&lv->checkpoints[i]);
    lv->nb_checkpoints = 0;
}

/* The two shapes it draws with, made once with the level (3.6). */
void practice_create(level_t *lv)
{
    lv->diamond = sfRectangleShape_create();
    sfRectangleShape_setSize(lv->diamond, (sfVector2f){DIAMOND, DIAMOND});
    sfRectangleShape_setOrigin(lv->diamond,
        (sfVector2f){DIAMOND / 2.0f, DIAMOND / 2.0f});
    sfRectangleShape_setRotation(lv->diamond, 45.0f);
    sfRectangleShape_setFillColor(lv->diamond, CHECKPOINT);
    sfRectangleShape_setOutlineThickness(lv->diamond, 3.0f);
    sfRectangleShape_setOutlineColor(lv->diamond, sfWhite);
    lv->bar_tick = sfRectangleShape_create();
    sfRectangleShape_setFillColor(lv->bar_tick, CHECKPOINT);
}

void practice_free(level_t *lv)
{
    practice_clear(lv);
    free(lv->checkpoints);
    lv->checkpoints = NULL;
    lv->cap_checkpoints = 0;
    sfRectangleShape_destroy(lv->diamond);
    sfRectangleShape_destroy(lv->bar_tick);
}

/*
** The run as it is at the last tick: player, camera, corridor, the objects
** already used, everything (3.3). Only a living player in practice places one.
*/
void practice_place(level_t *lv)
{
    sim_snapshot_t *bigger;

    if (!lv->practice || lv->state != LEVEL_PLAYING
        || !lv->sim.st.player.alive)
        return;
    if (lv->nb_checkpoints == lv->cap_checkpoints) {
        lv->cap_checkpoints = lv->cap_checkpoints == 0 ? 16
            : lv->cap_checkpoints * 2;
        bigger = sim_xcalloc(lv->cap_checkpoints, sizeof(sim_snapshot_t));
        if (lv->nb_checkpoints > 0)
            memcpy(bigger, lv->checkpoints,
                lv->nb_checkpoints * sizeof(sim_snapshot_t));
        free(lv->checkpoints);
        lv->checkpoints = bigger;
    }
    sim_snapshot_init(&lv->checkpoints[lv->nb_checkpoints], &lv->sim);
    sim_snapshot_save(&lv->checkpoints[lv->nb_checkpoints], &lv->sim);
    lv->nb_checkpoints += 1;
}

void practice_remove(level_t *lv)
{
    if (!lv->practice || lv->nb_checkpoints == 0)
        return;
    lv->nb_checkpoints -= 1;
    sim_snapshot_free(&lv->checkpoints[lv->nb_checkpoints]);
}

/*
** A respawn's run: the last checkpoint, exactly as it was taken. The objects
** it had used stay used, and aren't flashed again. False: none, the caller
** starts from the start.
*/
bool practice_restore(level_t *lv)
{
    if (!lv->practice || lv->nb_checkpoints == 0)
        return false;
    sim_snapshot_restore(&lv->sim, &lv->checkpoints[lv->nb_checkpoints - 1]);
    memcpy(lv->seen_spent, lv->sim.st.spent,
        lv->sim.st.spent_words * sizeof(uint64_t));
    return true;
}

/*
** On, the run goes on as it is. Off, the run under way was a practice one:
** its percentage is practice's, and the level starts again from its start,
** so no normal best ever comes from a checkpoint.
*/
void practice_set(level_t *lv, gd_t *gd, bool on)
{
    if (on == lv->practice)
        return;
    if (on) {
        lv->practice = true;
        return;
    }
    if (lv->state == LEVEL_PLAYING)
        level_record_best(lv);
    practice_clear(lv);
    lv->practice = false;
    level_respawn(lv, gd);
}

/* Green diamonds where the player was when each one was placed. */
void practice_draw_world(gd_t *gd, level_t *lv)
{
    if (!lv->practice || lv->nb_checkpoints == 0)
        return;
    for (size_t i = 0; i < lv->nb_checkpoints; i++) {
        vec2_t at = lv->checkpoints[i].st.player.pos;

        sfRectangleShape_setPosition(lv->diamond,
            (sfVector2f){(float)at.x, (float)at.y});
        draw_rect(gd, lv->diamond);
    }
}

/* A tick on the progress bar at each checkpoint's percentage. */
void practice_draw_bar(gd_t *gd, level_t *lv, sfVector2f bar, sfVector2f size)
{
    if (!lv->practice || lv->nb_checkpoints == 0)
        return;
    sfRectangleShape_setSize(lv->bar_tick, (sfVector2f){4.0f, size.y + 10.0f});
    for (size_t i = 0; i < lv->nb_checkpoints; i++) {
        double pct = lv->checkpoints[i].st.distance / lv->sim.lvl.end_shift;

        sfRectangleShape_setPosition(lv->bar_tick, (sfVector2f){bar.x
            + size.x * (float)fmin(pct, 1.0) - 2.0f, bar.y - 5.0f});
        draw_rect(gd, lv->bar_tick);
    }
}
