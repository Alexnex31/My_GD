/*
** ALEXNEX PROJECT, 2026
** button
** File description:
** the menus' image buttons: a sprite, and the toolkit's widget over it (FEATURES 3)
*/

#include "mygd.h"

void free_button(button_t *b)
{
    sfSprite_destroy(b->sprite);
    free(b);
}

/*
** The button is the square at (x, y), `size` wide; its image fills it, its
** shape kept, centered. An image made at that size is drawn as it is; the
** placeholders (the cursor's picture) are stretched to it, so what is seen
** is what is clicked.
*/
button_t *create_button(float x, float y, int size, sfTexture *texture)
{
    button_t *b = sim_xcalloc(1, sizeof(button_t));
    sfVector2u ts = sfTexture_getSize(texture);
    float side = (float)size;
    float k = side / (float)(ts.x > ts.y ? ts.x : ts.y);

    b->sprite = sfSprite_create();
    sfSprite_setTexture(b->sprite, texture, sfTrue);
    sfSprite_setScale(b->sprite, (sfVector2f){k, k});
    sfSprite_setPosition(b->sprite, (sfVector2f){x + (side - k
        * (float)ts.x) / 2.0f, y + (side - k * (float)ts.y) / 2.0f});
    b->area = (ui_rect_t){x, y, side, side};
    return b;
}

widget_t button_widget(const button_t *b, const char *label, widget_fn fn)
{
    return (widget_t){.kind = W_BUTTON, .label = label, .enabled = true,
        .bounds = b->area, .on_activate = fn};
}
