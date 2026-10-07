/*
** ALEXNEX PROJECT, 2026
** sim/modes.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_MODES_H
    #define SIM_MODES_H

    #include "sim/sim_types.h"

typedef struct mode_ops {
    const char *name;         /* name in level files ("cube", "ship")               */
    double half;               /* rigid square half size, circle radius (4.3)        */
    double inner_half;         /* inner box half size: a neutral touch kills         */
    double gravity;            /* px/tick^2                                          */
    double max_fall;           /* px/tick, fall speed cap                            */
    double head_restitution;   /* ceiling hit: < 0 dies, else share of vy bounced back */
    double corridor_height;   /* the corridor this mode's portal opens, px; 0: none (5.2) */
    bool neutral_kills;       /* any contact with a neutral object kills (the wave);
                                 the ground and corridor surfaces don't            */
    bool keep_vy_on_surface;  /* sliding on a surface doesn't reset vy (the wave)   */
    int bot_decision_ticks;   /* the bot's choices: 0 when it can jump, else every N ticks (8.3) */
    void (*apply_input)(player_t *p, input_t in);   /* impulses, input-driven speed */
    void (*apply_forces)(player_t *p);              /* gravity and caps; never moves */
    void (*update_rotation)(player_t *p);           /* the icon, cosmetic (9.4)     */
} mode_ops_t;

extern const mode_ops_t MODES[MODE_COUNT];

/*
** What a colour does, as a pad and as an orb, in GD's velocity units
** (FEATURES 6.9): the rise speed it sets, negative toward the floor. 0 means
** that object doesn't exist in that colour.
*/
typedef struct launch_ops {
    const char *name;         /* name in level files ("yellow", ...)          */
    double pad_v;
    double orb_v;
    bool flips;               /* gravity flips first, then the speed is set    */
} launch_ops_t;

extern const launch_ops_t LAUNCHES[LAUNCH_KIND_COUNT];

int launch_from_name(const char *name);    /* -1 if unknown */
double launch_speed(const object_t *o);     /* velocity units; 0: not a launcher */

int mode_from_name(const char *name);      /* -1 if unknown (5.5) */
double mode_neutral_kill_half(const mode_ops_t *m);   /* FEATURES 6.4 */
double modes_tallest_corridor(void);       /* over MODES[]: the kill line (4.7) */

#endif
