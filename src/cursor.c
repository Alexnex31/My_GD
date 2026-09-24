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

void print_cursor(cursor_t *cursor, sfRenderWindow *window)
{
    sfVector2f v;
    float x = (float)(sfMouse_getPositionRenderWindow(window).x);
    float y = (float)(sfMouse_getPositionRenderWindow(window).y);

    v = create_vector_f(x, y);
    sfSprite_setPosition(cursor->cursor_s, v);
    sfRenderWindow_drawSprite(window, cursor->cursor_s, NULL);
}

cursor_t *create_cursor(void)
{
    cursor_t *cursor = xcalloc(1, sizeof(cursor_t));
    sfVector2f v = create_vector_f(0.55, 0.55);

    cursor->cursor_s = sfSprite_create();
    cursor->cursor_t = sfTexture_createFromFile("res/cursor.png", NULL);
    sfSprite_setTexture(cursor->cursor_s, cursor->cursor_t, sfTrue);
    sfSprite_setScale(cursor->cursor_s, v);
    return cursor;
}
