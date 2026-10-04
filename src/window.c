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

/* The window the settings ask for (FEATURES 2.2), never past the desktop. */
sfRenderWindow *create_window(const settings_t *set)
{
    sfRenderWindow *window = NULL;
    sfVideoMode desktop = sfVideoMode_getDesktopMode();
    sfVideoMode video_mode = {(unsigned int)set->window_width,
        (unsigned int)set->window_height, 32};

    if (set->fullscreen)
        video_mode = desktop;
    if (video_mode.width > desktop.width)
        video_mode.width = desktop.width;
    if (video_mode.height > desktop.height)
        video_mode.height = desktop.height;
    window = sfRenderWindow_create(video_mode, "my_gd",
        set->fullscreen ? sfFullscreen : sfResize | sfClose, NULL);
    if (window == NULL)
        return NULL;                         /* the caller keeps the old one */
    window_apply_sync(window, set);
    sfRenderWindow_setMouseCursorVisible(window, sfFalse);
    sfRenderWindow_setKeyRepeatEnabled(window, sfFalse);   /* FEATURES 1.3 */
    return window;
}

/* VSync or a frame limit, never both: they fight (FEATURES 5.3). */
void window_apply_sync(sfRenderWindow *w, const settings_t *set)
{
    sfRenderWindow_setVerticalSyncEnabled(w, set->vsync);
    sfRenderWindow_setFramerateLimit(w, set->vsync ? 0
        : (unsigned int)set->fps_limit);
}

/*
** A new window for new display settings (FEATURES 5.4), made before the old
** one goes: an unsupported mode leaves the old window as it was, and -1.
** Every window setting comes with create_window; textures and fonts survive.
*/
int window_apply(gd_t *gd)
{
    sfRenderWindow *w = create_window(&gd->settings);
    sfVector2u size;

    if (w == NULL)
        return -1;
    sfRenderWindow_destroy(gd->w);
    gd->w = w;
    size = sfRenderWindow_getSize(w);
    apply_letterbox(gd, size.x, size.y);
    return 0;
}
