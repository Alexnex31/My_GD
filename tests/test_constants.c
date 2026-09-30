/*
** ALEXNEX PROJECT, 2026
** tests/main.c
** File description:
** functions to create and manage a window
*/

#include <math.h>
#include "test.h"
#include "sim/modes.h"

/*
** A ship held from rest climbs to its cap and then hovers within one tick of
** gravity of it: thrust only applies while vy is under the cap, so the speed
** alternates between the cap and one gravity step below. Returns the extreme
** reached over the last ticks, both parities included.
*/
static void ship_held_speed(double *lowest, double *highest)
{
    double g = MODES[MODE_SHIP].gravity;
    double cap = MODES[MODE_SHIP].max_fall;
    double vy = 0.0;

    *lowest = 1e9;
    *highest = -1e9;
    for (int i = 0; i < 400; i++) {
        if (vy < PER_TICK(SHIP_MAX_VY))
            vy = fmin(vy + PER_TICK2(SHIP_THRUST), PER_TICK(SHIP_MAX_VY) + g);
        if (vy > -cap)
            vy = fmax(vy - g, -cap);
        if (i < 200)
            continue;                        /* let it reach the cap first */
        *lowest = fmin(*lowest, vy);
        *highest = fmax(*highest, vy);
    }
}

void test_constants(void)
{
    double lowest = 0.0;
    double highest = 0.0;


    CHECK(fabs(FLOOR_MIN_DOT - cos(50.0 * M_PI / 180.0)) < 1e-5);
    CHECK(fabs(FLOOR_MAX_TAN - tan(50.0 * M_PI / 180.0)) < 1e-5);
    CHECK(fabs(CAM_LERP - (1.0 - exp(-1.0 / (TICK_RATE * CAM_TAU)))) < 1e-9);
    CHECK(PER_TICK(SCROLL_SPEED) == 4.3275);
    CHECK(PER_TICK(SPEED_VFAST) == 6.5);
    CHECK(PER_TICK(SPEED_XFAST) == 8.0);
    ship_held_speed(&lowest, &highest);
    CHECK(highest == PER_TICK(SHIP_MAX_VY));           /* never above the cap */
    CHECK(lowest >= PER_TICK(SHIP_MAX_VY) - MODES[MODE_SHIP].gravity);
}