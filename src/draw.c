/*
** ALEXNEX PROJECT, 2026
** draw
** File description:
** every draw goes through here, so the overlay can count them (9.0)
*/

#include "mygd.h"

void draw_sprite(gd_t *gd, const sfSprite *sprite, const sfRenderStates *rs)
{
    gd->draw_calls += 1;
    sfRenderWindow_drawSprite(gd->w, sprite, rs);
}

void draw_text(gd_t *gd, const sfText *text)
{
    gd->draw_calls += 1;
    sfRenderWindow_drawText(gd->w, text, NULL);
}

void draw_rect(gd_t *gd, const sfRectangleShape *shape)
{
    gd->draw_calls += 1;
    sfRenderWindow_drawRectangleShape(gd->w, shape, NULL);
}

void draw_vertex_buffer(gd_t *gd, const sfVertexBuffer *buf,
    const sfRenderStates *rs)
{
    gd->draw_calls += 1;
    sfRenderWindow_drawVertexBuffer(gd->w, buf, rs);
}

void draw_vertex_array(gd_t *gd, const sfVertexArray *array,
    const sfRenderStates *rs)
{
    gd->draw_calls += 1;
    sfRenderWindow_drawVertexArray(gd->w, array, rs);
}
