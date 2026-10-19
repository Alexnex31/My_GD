/*
** ALEXNEX PROJECT, 2026
** practice
** File description:
** practice mode: checkpoints are run-state snapshots, a death goes back to the last (PLAN 13.1)
*/

#include "mygd.h"

#define DIAMOND 36.0f                 /* the marker's side, world pixels */
#define EDGE 4.2f                     /* its white edge, 3 px across a side */

static const sfColor CHECKPOINT = {60, 220, 90, 230};

/* Every checkpoint goes: leaving practice, changing start, leaving the level. */
void practice_clear(level_t *lv)
{
    for (size_t i = 0; i < lv->nb_checkpoints; i++)
        sim_snapshot_free(&lv->checkpoints[i]);
    lv->nb_checkpoints = 0;
}

/* The vertices it draws with, made once with the level (3.6). */
void practice_create(level_t *lv)
{
    lv->marks = sfVertexArray_create();
    sfVertexArray_setPrimitiveType(lv->marks, sfTriangles);
}

void practice_free(level_t *lv)
{
    practice_clear(lv);
    free(lv->checkpoints);
    lv->checkpoints = NULL;
    lv->cap_checkpoints = 0;
    sfVertexArray_destroy(lv->marks);
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
    flash_forget(lv->seen_spent, &lv->sim.st);
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

/* Two triangles: corners c[0..3] around, in one colour. */
static void add_quad(sfVertexArray *va, const sfVector2f *c, sfColor col)
{
    static const int ORDER[6] = {0, 1, 2, 0, 2, 3};

    for (int k = 0; k < 6; k++)
        sfVertexArray_append(va, (sfVertex){c[ORDER[k]], col, {0, 0}});
}

/* A square turned 45 degrees, `r` from its middle to each corner. */
static void add_diamond(sfVertexArray *va, sfVector2f at, float r,
    sfColor col)
{
    sfVector2f c[4] = {{at.x, at.y - r}, {at.x + r, at.y},
        {at.x, at.y + r}, {at.x - r, at.y}};

    add_quad(va, c, col);
}

/*
** Green diamonds with a white edge where the player was when each one was
** placed: those in view, all in one draw call.
*/
void practice_draw_world(gd_t *gd, level_t *lv, float cam_x)
{
    float r = DIAMOND / sqrtf(2.0f);

    if (!lv->practice || lv->nb_checkpoints == 0)
        return;
    sfVertexArray_clear(lv->marks);
    for (size_t i = 0; i < lv->nb_checkpoints; i++) {
        vec2_t at = lv->checkpoints[i].st.player.pos;
        sfVector2f p = {(float)at.x, (float)at.y};

        if (p.x + r + EDGE < cam_x || p.x - r - EDGE > cam_x + VIEW_W)
            continue;
        add_diamond(lv->marks, p, r + EDGE, sfWhite);
        add_diamond(lv->marks, p, r, CHECKPOINT);
    }
    if (sfVertexArray_getVertexCount(lv->marks) > 0)
        draw_vertex_array(gd, lv->marks, NULL);
}

/* A tick on the progress bar at each checkpoint's percentage, one call. */
void practice_draw_bar(gd_t *gd, level_t *lv, sfVector2f bar, sfVector2f size)
{
    if (!lv->practice || lv->nb_checkpoints == 0)
        return;
    sfVertexArray_clear(lv->marks);
    for (size_t i = 0; i < lv->nb_checkpoints; i++) {
        double pct = lv->checkpoints[i].st.distance / lv->sim.lvl.end_shift;
        float x = bar.x + size.x * (float)fmin(pct, 1.0) - 2.0f;
        sfVector2f c[4] = {{x, bar.y - 5.0f}, {x + 4.0f, bar.y - 5.0f},
            {x + 4.0f, bar.y + size.y + 5.0f}, {x, bar.y + size.y + 5.0f}};

        add_quad(lv->marks, c, CHECKPOINT);
    }
    draw_vertex_array(gd, lv->marks, NULL);
}
