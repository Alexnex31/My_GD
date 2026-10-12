/*
** ALEXNEX PROJECT, 2026
** sim/bot.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_BOT_H
    #define SIM_BOT_H

    #include <stddef.h>

    #include "sim/sim_types.h"

    #define BOT_MAX_ATTEMPTS 200000L  /* a cap: "no path" can be wrong (8.3) */

typedef enum bot_verdict {
    BOT_FOUND,                /* a run reached the end                       */
    BOT_NO_PATH,              /* every decision was tried both ways          */
    BOT_GAVE_UP               /* the attempt cap ran out first               */
} bot_verdict_t;

typedef struct bot_result {
    bot_verdict_t verdict;
    long attempts;            /* runs that ended, the last one included      */
    float furthest;           /* the best percentage any run reached         */
    bool *inputs;             /* the button, tick by tick: of the run found,
                                 or else of the one that went furthest       */
    long ticks;               /* the run's length; replayed from sim_reset    */
} bot_result_t;

/* Search for a run that completes the level (8.3). Leaves the sim reset. */
bot_result_t bot_solve(sim_t *s, long max_attempts);

void bot_result_free(bot_result_t *r);

/*
** The same search a few runs at a time, for a window that has to keep
** drawing (FEATURES 11.10). bot_step is true once it is over: found, no
** path, or `max_attempts` runs made. bot_end frees it and resets the sim.
*/
typedef struct bot_search bot_search_t;

bot_search_t *bot_begin(sim_t *s);
bool bot_step(bot_search_t *b, long attempts, long max_attempts);
const bot_result_t *bot_progress(const bot_search_t *b);
bot_result_t bot_end(bot_search_t *b);

/* The one line --check and make test print for a result: information only. */
void bot_describe(const bot_result_t *r, char *buf, size_t size);

#endif
