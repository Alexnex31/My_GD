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

/* The click lands on the button's square, its edges included. */
bool button_hit(const button_t *b, int mx, int my)
{
    return mx >= b->pos.x && mx <= b->pos.x + b->size
        && my >= b->pos.y && my <= b->pos.y + b->size;
}

void print_button(button_t *b, sfRenderWindow *w)
{
    sfRenderWindow_drawSprite(w, b->sprite, NULL);
}

button_t *create_button(float x, float y, int size, sfTexture *texture)
{
    button_t *b = sim_xcalloc(1, sizeof(button_t));

    b->pos = (sfVector2f){x, y};
    b->size = size;
    b->sprite = sfSprite_create();
    sfSprite_setPosition(b->sprite, b->pos);
    sfSprite_setTexture(b->sprite, texture, sfTrue);
    return b;
}
