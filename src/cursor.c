/*
** ALEXNEX PROJECT, 2026
** cursor
** File description:
** functions to create and manage a custom cursor
*/

#include "mygd.h"

void free_cursor(cursor_t *cursor)
{
    sfSprite_destroy(cursor->cursor_s);
    sfTexture_destroy(cursor->cursor_t);
    free(cursor);
}

/* Drawn in the current view, so the mouse's pixel is mapped into it (9.7). */
void print_cursor(cursor_t *cursor, sfRenderWindow *window)
{
    sfVector2i pixel = sfMouse_getPositionRenderWindow(window);
    sfVector2f v = sfRenderWindow_mapPixelToCoords(window, pixel,
        sfRenderWindow_getView(window));

    sfSprite_setPosition(cursor->cursor_s, v);
    sfRenderWindow_drawSprite(window, cursor->cursor_s, NULL);
}

cursor_t *create_cursor(void)
{
    cursor_t *cursor = sim_xcalloc(1, sizeof(cursor_t));
    sfVector2f v = {0.55f, 0.55f};

    cursor->cursor_s = sfSprite_create();
    cursor->cursor_t = sfTexture_createFromFile("res/cursor.png", NULL);
    sfSprite_setTexture(cursor->cursor_s, cursor->cursor_t, sfTrue);
    sfSprite_setScale(cursor->cursor_s, v);
    return cursor;
}
