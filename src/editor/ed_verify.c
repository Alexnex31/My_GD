/*
** ALEXNEX PROJECT, 2026
** editor/ed_verify.c
** File description:
** the bot on the level being edited, and the path it took (FEATURES 11.10)
*/

#include <stdio.h>
#include <stdlib.h>

#include "editor/ed_verify.h"
#include "sim/alloc.h"
#include "sim/sim.h"

void ed_verify_free(ed_verify_t *v)
{
    bot_result_t r = {0};

    if (v->search != NULL)
        r = bot_end(v->search);
    bot_result_free(&r);
    if (v->sim != NULL)
        sim_free(v->sim);
    free(v->sim);
    free(v->path);
    *v = (ed_verify_t){0};
}

void ed_verify_begin(ed_verify_t *v, const ed_level_t *lv)
{
    level_doc_t doc;

    ed_verify_free(v);
    v->sim = sim_xcalloc(1, sizeof(sim_t));
    ed_level_doc(lv, &doc);
    sim_init(v->sim, &doc, &lv->hdr, lv->id);
    ed_doc_free(&doc);
    v->search = bot_begin(v->sim);
}

bool ed_verify_running(const ed_verify_t *v)
{
    return v->search != NULL;
}

/*
** The run played again from the start, a point every few ticks and the last
** one: it is the engine's own run, so the line is where a player would be.
*/
static void trace(ed_verify_t *v, const bot_result_t *r)
{
    sim_t *s = v->sim;
    bool prev = false;

    v->path = sim_xcalloc((size_t)r->ticks / ED_VERIFY_EVERY + 3,
        sizeof(vec2_t));
    sim_reset(s);
    v->path[v->path_len++] = s->st.player.pos;
    for (long t = 0; t < r->ticks && s->st.player.alive; t++) {
        sim_tick(s, (input_t){r->inputs[t], r->inputs[t] && !prev});
        prev = r->inputs[t];
        if ((t + 1) % ED_VERIFY_EVERY == 0 || t + 1 == r->ticks)
            v->path[v->path_len++] = s->st.player.pos;
    }
    v->end = s->st.player.pos;
}

bool ed_verify_step(ed_verify_t *v, long runs, long max_runs)
{
    bot_result_t r;

    if (v->search == NULL)
        return v->done;
    if (!bot_step(v->search, runs, max_runs))
        return false;
    r = bot_end(v->search);
    v->search = NULL;
    v->verdict = r.verdict;
    v->attempts = r.attempts;
    v->furthest = r.furthest;
    trace(v, &r);
    bot_result_free(&r);
    sim_free(v->sim);
    free(v->sim);
    v->sim = NULL;
    v->done = true;
    return true;
}

void ed_verify_text(const ed_verify_t *v, char *out, size_t size)
{
    const bot_result_t *r = v->search != NULL ? bot_progress(v->search) : NULL;

    if (r != NULL)
        snprintf(out, size, "Verifying: %ld runs, furthest %.0f%%   "
            "(Escape stops)", r->attempts, r->furthest < 0.0f ? 0.0f
            : r->furthest);
    else if (!v->done)
        snprintf(out, size, "%s", "");
    else if (v->verdict == BOT_FOUND)
        snprintf(out, size, "Bot found a path (%ld runs)", v->attempts);
    else if (v->verdict == BOT_NO_PATH)
        snprintf(out, size, "Bot found no path: furthest %.0f%% (it can "
            "miss one)", v->furthest);
    else
        snprintf(out, size, "Undecided: search limit reached, furthest "
            "%.0f%%", v->furthest);
}
