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
    bool *inputs;             /* BOT_FOUND: the button, tick by tick         */
    long ticks;               /* the run's length; replayed from sim_reset    */
} bot_result_t;

/* Search for a run that completes the level (8.3). Leaves the sim reset. */
bot_result_t bot_solve(sim_t *s, long max_attempts);

void bot_result_free(bot_result_t *r);

/* The one line --check and make test print for a result: information only. */
void bot_describe(const bot_result_t *r, char *buf, size_t size);

#endif
