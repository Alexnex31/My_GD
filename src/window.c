/*
** ALEXNEX PROJECT, 2026
** window
** File description:
** functions to create and manage a window
*/

#include "mygd.h"

void close_window(sfRenderWindow *window)
{
    sfRenderWindow_close(window);
}

void destroy_all(sfRenderWindow *window)
{
    sfRenderWindow_close(window);
    sfRenderWindow_destroy(window);
}

sfRenderWindow *create_window(unsigned int width,
    unsigned int height)
{
    sfRenderWindow *window;
    sfVideoMode video_mode;

    video_mode.width = width;
    video_mode.height = height;
    video_mode.bitsPerPixel = 32;
    window = sfRenderWindow_create(video_mode,
        "Window",
        sfDefaultStyle,
        NULL);
    sfRenderWindow_setFramerateLimit(window, 60);
    sfRenderWindow_setMouseCursorVisible(window, sfFalse);
    sfRenderWindow_setTitle(window, "my_gd");
    return (window);
}
