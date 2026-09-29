/*
** ALEXNEX PROJECT, 2026
** input
** File description:
** the jump input, sampled once per simulation tick (3.6, FEATURES 1)
*/

#include "mygd.h"

static bool jump_is_down(void)
{
    return sfKeyboard_isKeyPressed(sfKeySpace) == sfTrue
        || sfKeyboard_isKeyPressed(sfKeyUp) == sfTrue
        || sfMouse_isButtonPressed(sfMouseLeft) == sfTrue;
}

/*
** Called once per tick from inside the loop, so `pressed` is true on exactly
** one tick per physical press. A press and release inside one frame is missed;
** FEATURES 1 adds event edges without changing this loop.
*/
input_t input_for_tick(gd_t *gd)
{
    bool held = jump_is_down();
    input_t in = {held, held && !gd->was_held};

    gd->was_held = held;
    return in;
}
