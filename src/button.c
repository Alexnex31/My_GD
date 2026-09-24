/*
** ALEXNEX PROJECT, 2026
** button
** File description:
** functions to create and manage a button
*/

#include "mygd.h"

void free_button(button_t *b)
{
    sfSprite_destroy(b->sprite);
    free(b);
}

void print_button(button_t *b, sfRenderWindow *w)
{
    sfRenderWindow_drawSprite(w, b->sprite, NULL);
}

button_t *create_button(float x, float y, int size, sfTexture *texture)
{
    button_t *b = xcalloc(1, sizeof(button_t));

    b->pos = create_vector_f(x, y);
    b->pressed = 'n';
    b->size = size;
    b->sprite = sfSprite_create();
    sfSprite_setPosition(b->sprite, b->pos);
    sfSprite_setTexture(b->sprite, texture, sfTrue);
    return b;
}
