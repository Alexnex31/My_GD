/*
** ALEXNEX PROJECT, 2026
** sim/speed.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_SPEED_H
    #define SIM_SPEED_H

    #include "sim/sim_types.h"

    #define SPEED_COUNT 5

/*
** GD's five speeds under the names its players use, which are the values a
** level file writes: 0.5, 1, 2, 3 and 4 (FEATURES 10.3). They are names, not
** ratios: "2" is 1.243 times the normal speed.
*/
extern const double SPEED_NAMES[SPEED_COUNT];

bool speed_is_known(double name);
double speed_px_per_s(double name);          /* the normal one if unknown */
double speed_step(double name);              /* the same, px per tick */
const char *speed_word(double name);         /* as a file writes it */

/*
** How long the level takes to reach an x, when the speed changes along it
** (10.3, 11.11). A run is assumed to take every speed portal, the moment its
** side reaches the portal's hitbox: a portal placed where the player may not
** pass makes this an approximation, which is all the song's place in the
** editor and a start position's tick need.
*/
/* Adds the object if it is a speed portal, and does nothing otherwise. */
void speed_map_add(speed_map_t *m, const object_t *o);
void speed_map_sort(speed_map_t *m);
void speed_map_free(speed_map_t *m);

/* Ticks from the start to x, and the x reached after that many ticks. */
double speed_map_ticks(const speed_map_t *m, const level_start_t *start,
    double x);
double speed_map_x(const speed_map_t *m, const level_start_t *start,
    double ticks);

#endif
