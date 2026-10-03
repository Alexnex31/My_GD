/*
** ALEXNEX PROJECT, 2026
** window
** File description:
** functions to create and manage a window, and its two views (9.1, 9.7)
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

/*
** Keep the 16:9 image proportional whatever the window is, with black bars
** on the side that has room to spare (9.7). Both views get the same viewport.
*/
static void letterbox_view(sfView *view, float win, float want)
{
    sfFloatRect vp = {0.0f, 0.0f, 1.0f, 1.0f};

    if (win > want) {
        vp.width = want / win;
        vp.left = (1.0f - vp.width) / 2.0f;
    } else {
        vp.height = win / want;
        vp.top = (1.0f - vp.height) / 2.0f;
    }
    sfView_setViewport(view, vp);
}

void apply_letterbox(gd_t *gd, unsigned int w, unsigned int h)
{
    float win = (float)w / (float)(h == 0 ? 1 : h);
    float want = VIEW_W / VIEW_H;
    sfFloatRect vp;

    letterbox_view(gd->ui_view, win, want);
    letterbox_view(gd->level_view, win, want);
    vp = sfView_getViewport(gd->ui_view);
    gd->viewport_px_w = vp.width * (float)w;   /* pixel snapping (9.1) */
}

sfRenderWindow *create_window(unsigned int width, unsigned int height)
{
    sfRenderWindow *window = NULL;
    sfVideoMode desktop = sfVideoMode_getDesktopMode();
    sfVideoMode video_mode = {width, height, 32};

    if (video_mode.width > desktop.width)
        video_mode.width = desktop.width;
    if (video_mode.height > desktop.height)
        video_mode.height = desktop.height;
    window = sfRenderWindow_create(video_mode, "my_gd",
        sfResize | sfClose, NULL);
    sfRenderWindow_setFramerateLimit(window, 60);
    sfRenderWindow_setMouseCursorVisible(window, sfFalse);
    sfRenderWindow_setKeyRepeatEnabled(window, sfFalse);   /* FEATURES 1.3 */
    return window;
}
