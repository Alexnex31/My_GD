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

/*
** The search's budget, in ticks simulated (replays included), for every
** caller: --check, make test and the editor's Verify give the same answer.
*/
    #define BOT_MAX_TICKS 20000000L

/*
** What the bot says is information, never a proof (8.3). It chooses only at
** some moments (when the cube can jump, every 12 ticks for the flying modes,
** the robot's rise at a few lengths, every tick near an orb), and two states
** in the same cell (1 px, 1/8 px per tick) are one: "no path" can be wrong.
*/
typedef enum bot_verdict {
    BOT_FOUND,                /* a run reached the end                       */
    BOT_NO_PATH,              /* none of the bot's own runs does             */
    BOT_GAVE_UP,              /* the budget ran out first: undecided         */
    BOT_SEARCHING             /* still at it (bot_progress only)             */
} bot_verdict_t;

typedef struct bot_result {
    bot_verdict_t verdict;
    long attempts;            /* runs that ended, the last one included      */
    long work;                /* ticks simulated, replays included           */
    float furthest;           /* the best percentage a run ended at        */
    bool *inputs;             /* the button, tick by tick: of the run found,
                                 or else of the one that went furthest       */
    long ticks;               /* the run's length; replayed from sim_reset    */
} bot_result_t;

/* Search for a run that completes the level (8.3). Leaves the sim reset. */
bot_result_t bot_solve(sim_t *s, long max_ticks);

void bot_result_free(bot_result_t *r);

/*
** Tick `t` of a result's run, its press edge included: the one rule every
** replay of a bot's run goes by.
*/
input_t bot_input_at(const bot_result_t *r, long t);

/*
** The result's run from sim_reset: true when it completes on its last tick,
** neither dead nor complete before it. Leaves the sim where the run ended.
*/
bool bot_replays(sim_t *s, const bot_result_t *r);

/*
** The same search a tick budget at a time, for a window that has to keep
** drawing (FEATURES 11.10): bot_step runs at most `ticks` more, and is true
** once it is over: found, no path, or `max_ticks` spent. bot_end frees it
** and resets the sim.
*/
typedef struct bot_search bot_search_t;

bot_search_t *bot_begin(sim_t *s);
bool bot_step(bot_search_t *b, long ticks, long max_ticks);
const bot_result_t *bot_progress(const bot_search_t *b);
bot_result_t bot_end(bot_search_t *b);

/* The one line --check and make test print for a result: information only. */
void bot_describe(const bot_result_t *r, char *buf, size_t size);

#endif
