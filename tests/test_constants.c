/*
** ALEXNEX PROJECT, 2026
** tests/test_constants.c
** File description:
** the physics constants, against the numbers they come from (FEATURES 6.9)
*/

#include <math.h>
#include <string.h>

#include "sim/modes.h"
#include "sim/sim.h"
#include "test.h"

/*
** A ship held from rest climbs to its cap and then hovers within one tick of
** gravity of it: thrust only applies while vy is under the cap, so the speed
** alternates between the cap and one gravity step below. Returns the extreme
** reached over the last ticks, both parities included. It is the real tick:
** only the height is put back mid-corridor, so the ceiling never stops it.
*/
static void ship_held_speed(double *lowest, double *highest)
{
    const char *text = "start_gamemode ship\nblock 100000 700 2\n";
    sim_t s;

    sim_load_mem(&s, text, strlen(text), "1", NULL);
    *lowest = 1e9;
    *highest = -1e9;
    for (int i = 0; i < 400 && s.st.player.alive; i++) {
        s.st.player.pos.y = (s.st.bounds.top + s.st.bounds.bottom) / 2.0;
        sim_tick(&s, (input_t){true, i == 0});
        if (i < 200)
            continue;                        /* let it reach the cap first */
        *lowest = fmin(*lowest, s.st.player.vy);
        *highest = fmax(*highest, s.st.player.vy);
    }
    CHECK(s.st.player.alive);
    sim_free(&s);
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
    CHECK(modes_tallest_corridor() == 1000.0);           /* the ship's (5.2) */
    ship_held_speed(&lowest, &highest);
    CHECK(highest == PER_TICK(SHIP_MAX_VY));           /* never above the cap */
    CHECK(lowest >= PER_TICK(SHIP_MAX_VY) - MODES[MODE_SHIP].gravity);
}