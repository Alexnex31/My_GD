/*
** ALEXNEX PROJECT, 2026
** tests/main.c
** File description:
** runs every test of the simulation and counts the failed checks (8)
*/

#include "test.h"

int failures = 0;

int main(void)
{
    test_constants();
    test_hitbox();
    test_sweep();
    test_circle();
    test_parser();
    test_tick();
    test_slopes();
    test_portal();
    test_start();
    test_gravity();
    test_ufo();
    test_wave();
    test_trail();
    test_ball();
    test_mirror();
    test_pads();
    test_orbs();
    test_progress();
    test_input();
    test_settings();
    test_ui();
    test_music();
    test_options();
    test_bot();
    if (failures == 0)
        printf("all tests passed\n");
    else
        printf("%d check(s) failed\n", failures);
    return failures != 0;
}
