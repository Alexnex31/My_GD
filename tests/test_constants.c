/*
** ALEXNEX PROJECT, 2026
** tests/main.c
** File description:
** functions to create and manage a window
*/

#include <math.h>
#include "test.h"
#include "sim/modes.h"

static double ship_settle_speed(void)
{
    double g = MODES[MODE_SHIP].gravity;
    double cap = MODES[MODE_SHIP].max_fall;
    double vy = 0.0;

    for (int i = 0; i < 200; i++) {
        if (vy < PER_TICK(SHIP_MAX_VY))
            vy = fmin(vy + PER_TICK2(SHIP_THRUST), PER_TICK(SHIP_MAX_VY) + g);
        if (vy > -cap)
            vy = fmax(vy - g, -cap);
    }
    return vy;
}

void test_constants(void)
{
    CHECK(fabs(FLOOR_MIN_DOT - cos(50.0 * M_PI / 180.0)) < 1e-5);
    CHECK(fabs(FLOOR_MAX_TAN - tan(50.0 * M_PI / 180.0)) < 1e-5);
    CHECK(fabs(CAM_LERP - (1.0 - exp(-1.0 / (TICK_RATE * CAM_TAU)))) < 1e-9);
    CHECK(PER_TICK(SCROLL_SPEED) == 4.3275);
    CHECK(PER_TICK(SPEED_VFAST) == 6.5);
    CHECK(PER_TICK(SPEED_XFAST) == 8.0);
    CHECK(ship_settle_speed() == PER_TICK(SHIP_MAX_VY));
}