/*
** ALEXNEX PROJECT, 2026
** editor/ed_verify.h
** File description:
** header file for my_gd project
*/

#ifndef EDITOR_ED_VERIFY_H
    #define EDITOR_ED_VERIFY_H

    #include "editor/ed_level.h"
    #include "sim/bot.h"

    #define ED_VERIFY_EVERY 4         /* ticks between two points of the path */

/*
** The bot on the level being edited (FEATURES 11.10), a few runs at a time
** so the window keeps drawing. It works on a copy: the level can't change
** under it. What it says is information only: its search is coarser than a
** player and can miss a path. Pure, tested alone.
*/
typedef struct ed_verify {
    sim_t *sim;               /* the copy, while the search runs             */
    bot_search_t *search;
    bool done;                /* what follows is filled                      */
    bot_verdict_t verdict;
    long attempts;
    float furthest;           /* percent                                     */
    vec2_t *path;             /* the run found, or else the furthest one     */
    size_t path_len;
    vec2_t end;               /* where that run stopped                      */
} ed_verify_t;

/* Stops and forgets whatever `v` held: zero it before its first use. */
void ed_verify_begin(ed_verify_t *v, const ed_level_t *lv);
/*
** Up to `ticks` more of the search (BOT_MAX_TICKS in all, as --check); true
** once it is over, and the result is there.
*/
bool ed_verify_step(ed_verify_t *v, long ticks, long max_ticks);
bool ed_verify_running(const ed_verify_t *v);
void ed_verify_free(ed_verify_t *v);

/* One line for the toolbar, while it runs and after. */
void ed_verify_text(const ed_verify_t *v, char *out, size_t size);

#endif
