/*
** ALEXNEX PROJECT, 2026
** input
** File description:
** the jump input, sampled once per simulation tick (3.6, FEATURES 1)
*/

#include "mygd.h"

/* The mouse is a jump only once the click that started the level is up. */
static bool jump_is_down(gd_t *gd)
{
    bool mouse = sfMouse_isButtonPressed(sfMouseLeft) == sfTrue;

    if (!mouse)
        gd->click_latched = false;
    return sfKeyboard_isKeyPressed(sfKeySpace) == sfTrue
        || sfKeyboard_isKeyPressed(sfKeyUp) == sfTrue
        || (mouse && !gd->click_latched);
}

/*
** A level starts from a click (Play, Retry), usually still down when its
** first tick runs: held, it would be a jump at tick 0 (3.4's carried hold).
*/
void input_level_started(gd_t *gd)
{
    gd->click_latched = true;
    gd->was_held = false;
}

/*
** Called once per tick from inside the loop, so `pressed` is true on exactly
** one tick per physical press. A press and release inside one frame is missed;
** FEATURES 1 adds event edges without changing this loop.
*/
input_t input_for_tick(gd_t *gd)
{
    bool held = jump_is_down(gd);
    input_t in = {held, held && !gd->was_held};

    gd->was_held = held;
    return in;
}
