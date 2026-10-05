/*
** ALEXNEX PROJECT, 2026
** options_dialogs
** File description:
** the options screen's dialogs: display revert, progress reset, save folder (FEATURES 5.4, 5.6)
*/

#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "mygd.h"

#define REVERT_MS 10000

static void first_button(void *ctx, widget_t *w)
{
    (void)w;
    ((option_m_t *)ctx)->pending_answer = 1;
}

static void second_button(void *ctx, widget_t *w)
{
    (void)w;
    ((option_m_t *)ctx)->pending_answer = 2;
}

static void dialog_back(void *ctx)
{
    ((option_m_t *)ctx)->pending_answer = 1;   /* the first is the safe one */
}

/*
** Two buttons, the first focused: Revert, so a player who can't see an
** unsupported mode gets the old one back by waiting or pressing Enter; and
** Cancel, so Enter never deletes anything (5.4, 5.6).
*/
void options_open_dialog(option_m_t *om, options_dialog_t which)
{
    om->dialog = which;
    om->dialog_widgets[0] = (widget_t){.kind = W_BUTTON, .enabled = true,
        .label = which == DIALOG_REVERT ? "Revert" : "Cancel",
        .bounds = {640.0f, 600.0f, 300.0f, 90.0f}, .on_activate = first_button};
    om->dialog_widgets[1] = (widget_t){.kind = W_BUTTON, .enabled = true,
        .label = which == DIALOG_REVERT ? "Keep" : "Delete",
        .bounds = {980.0f, 600.0f, 300.0f, 90.0f},
        .on_activate = second_button};
    ui_init(&om->dialog_ui, om->dialog_widgets, 2, om);
    om->dialog_ui.on_back = dialog_back;
    om->dialog_ui.panel = (ui_rect_t){260.0f, 370.0f, 1400.0f, 370.0f};
    om->rows.modal = &om->dialog_ui;
    om->revert_at_ms = ui_now_ms() + REVERT_MS;
}

static void close_dialog(option_m_t *om)
{
    om->dialog = DIALOG_NONE;
    om->rows.modal = NULL;
}

static void set_display(settings_t *s, bool fullscreen, int w, int h)
{
    s->fullscreen = fullscreen;
    s->window_width = w;
    s->window_height = h;
}

static void revert_display(option_m_t *om)
{
    set_display(&om->gd->settings, om->before_fullscreen, om->before_w,
        om->before_h);
    if (window_apply(om->gd) != 0)
        options_notice(om, "The previous display mode failed too");
    options_build_rows(om);
}

/*
** After the events (5.4): the new window, made before the old one goes. An
** unsupported mode keeps the old window and says so; a supported one asks
** to be kept.
*/
void options_apply_display(option_m_t *om)
{
    settings_t *s = &om->gd->settings;

    om->before_fullscreen = s->fullscreen;
    om->before_w = s->window_width;
    om->before_h = s->window_height;
    set_display(s, om->fullscreen, om->sizes.w[om->window_size],
        om->sizes.h[om->window_size]);
    if (window_apply(om->gd) != 0) {
        set_display(s, om->before_fullscreen, om->before_w, om->before_h);
        options_build_rows(om);
        return options_notice(om, "Display mode not supported");
    }
    options_build_rows(om);
    options_open_dialog(om, DIALOG_REVERT);
}

/* Everything gone, the old file kept as progress.txt.bak (5.6). */
static void reset_progress(option_m_t *om)
{
    if (progress_reset(&om->gd->progress) != 0)
        return options_notice(om, "Couldn't back the progress up: nothing"
            " was deleted");
    options_notice(om, "Progress reset (the old file is progress.txt.bak)");
}

void options_answer(option_m_t *om, int answer)
{
    options_dialog_t which = om->dialog;

    close_dialog(om);
    if (which == DIALOG_REVERT && answer == 1)
        revert_display(om);
    if (which == DIALOG_RESET && answer == 2)
        reset_progress(om);
}

/* Unanswered for 10 s: Revert (5.4). */
void options_dialogs_update(option_m_t *om)
{
    if (om->dialog == DIALOG_REVERT && ui_now_ms() >= om->revert_at_ms)
        options_answer(om, 1);
}

static void centered(option_m_t *om, const char *s, float y)
{
    sfFloatRect b;

    sfText_setUnicodeString(om->text, utf8_to_utf32(s));
    b = sfText_getLocalBounds(om->text);
    sfText_setOrigin(om->text, (sfVector2f){b.left + b.width / 2.0f, 0.0f});
    sfText_setPosition(om->text, (sfVector2f){VIEW_W / 2.0f, y});
    draw_text(om->gd, om->text);
    sfText_setOrigin(om->text, (sfVector2f){0.0f, 0.0f});
}

void options_draw_dialog(option_m_t *om)
{
    char line[120];
    int64_t left = om->revert_at_ms - ui_now_ms();

    if (om->dialog == DIALOG_REVERT) {
        snprintf(line, sizeof(line), "Keep these display settings? Reverting"
            " in %d", (int)((left + 999) / 1000));
        centered(om, line, 450.0f);
    }
    if (om->dialog == DIALOG_RESET) {
        centered(om, "Delete all attempts, best percentages and per-level"
            " song choices?", 420.0f);
        centered(om, "This cannot be undone (a copy is kept as"
            " progress.txt.bak).", 490.0f);
    }
}

/*
** xdg-open, without a shell and without waiting for it: the child starts it
** from a grandchild and exits at once, so nothing is left to reap.
*/
void options_open_save_folder(option_m_t *om)
{
    char path[PATH_MAX];
    char text[PATH_MAX + 16];
    pid_t child = 0;

    mkdir("save", 0755);
    if (realpath("save", path) == NULL)
        return options_notice(om, "There's no save folder");
    printf("%s\n", path);
    fflush(stdout);
    child = fork();
    if (child == 0) {
        if (fork() == 0) {
            execlp("xdg-open", "xdg-open", path, (char *)NULL);
            _exit(127);
        }
        _exit(0);
    }
    if (child > 0)
        waitpid(child, NULL, 0);
    snprintf(text, sizeof(text), "Opened %s", path);
    options_notice(om, text);
}
