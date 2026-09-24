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
} mode_ops_t;

extern const mode_ops_t MODES[MODE_COUNT];

int mode_from_name(const char *name);      /* -1 if unknown (5.5) */

#endif
