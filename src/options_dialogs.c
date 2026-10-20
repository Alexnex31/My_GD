/*
** ALEXNEX PROJECT, 2026
** options_dialogs
** File description:
** the options screen's dialogs: display revert, progress reset, save folder (FEATURES 5.4, 5.6)
*/

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "mygd.h"

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
/*
** The calibration (5.2): the menu song stops (its place kept), a click
** every half second against a flashing square. The slider, focused, moves
** the offset while it runs; Done or Escape ends it.
*/
static void open_calibration(option_m_t *om)
{
    om->dialog = DIALOG_CALIBRATE;
    om->dialog_widgets[0] = (widget_t){.kind = W_SLIDER, .enabled = true,
        .label = "Audio offset (ms)", .value = &om->audio_offset,
        .min = -300, .max = 300, .step = 5,
        .bounds = {900.0f, 640.0f, 380.0f, 70.0f},
        .on_change = options_audio_offset_changed};
    om->dialog_widgets[1] = (widget_t){.kind = W_BUTTON, .enabled = true,
        .label = "Done", .bounds = {810.0f, 760.0f, 300.0f, 90.0f},
        .on_activate = first_button};
    ui_init(&om->dialog_ui, om->dialog_widgets, 2, om);
    om->dialog_ui.on_back = dialog_back;
    om->dialog_ui.panel = (ui_rect_t){260.0f, 170.0f, 1400.0f, 720.0f};
    om->rows.modal = &om->dialog_ui;
    music_silence(&om->gd->music);
    calib_begin(&om->calib, ui_now_ms());
}

void options_open_dialog(option_m_t *om, options_dialog_t which)
{
    if (which == DIALOG_CALIBRATE)
        return open_calibration(om);
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
}

static void close_dialog(option_m_t *om)
{
    om->dialog = DIALOG_NONE;
    om->rows.modal = NULL;
}

/*
** The game closed while the mode was still on trial: what is saved is the
** mode before it, which is known to work. The window goes anyway.
*/
void options_forget_display(option_m_t *om)
{
    display_trial_end(&om->trial, &om->gd->settings, false);
}

static void revert_display(option_m_t *om)
{
    display_trial_end(&om->trial, &om->gd->settings, false);
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
    display_trial_begin(&om->trial, &om->gd->settings, om->fullscreen,
        om->sizes.w[om->window_size], om->sizes.h[om->window_size],
        ui_now_ms());
    if (window_apply(om->gd) != 0) {
        display_trial_end(&om->trial, &om->gd->settings, false);
        options_build_rows(om);
        return options_notice(om, "Display mode not supported");
    }
    options_build_rows(om);
    options_open_dialog(om, DIALOG_REVERT);
}

/* Everything gone, the old file kept as progress.txt.bak if it had any (5.6). */
static void reset_progress(option_m_t *om)
{
    int kept = progress_reset(&om->gd->progress);

    if (kept < 0)
        return options_notice(om, "Couldn't reset the progress: nothing"
            " was deleted");
    options_notice(om, kept > 0 ? "Progress reset (the old file is"
        " progress.txt.bak)" : "Progress reset (it was empty: nothing to keep)");
}

void options_answer(option_m_t *om, int answer)
{
    options_dialog_t which = om->dialog;

    close_dialog(om);
    if (which == DIALOG_REVERT && answer == 1)
        revert_display(om);
    if (which == DIALOG_REVERT && answer == 2)
        display_trial_end(&om->trial, &om->gd->settings, true);   /* 5.7 */
    if (which == DIALOG_RESET && answer == 2)
        reset_progress(om);
    if (which == DIALOG_CALIBRATE) {
        options_build_rows(om);              /* the row shows the new offset */
        music_menu(om->gd);                  /* where it was (4.10) */
    }
}

/* Unanswered for 10 s: Revert (5.4). The calibration's clicks, on time. */
void options_dialogs_update(option_m_t *om)
{
    if (om->dialog == DIALOG_CALIBRATE && calib_click_due(&om->calib,
        ui_now_ms(), om->gd->settings.audio_offset_ms))
        sound_click(om->gd);
    if (om->dialog == DIALOG_REVERT && display_trial_due(&om->trial,
        ui_now_ms()))
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

/* The square lit on each beat; the click is heard `offset` ms before it. */
static void draw_calibration(option_m_t *om)
{
    float lit = calib_flash(&om->calib, ui_now_ms());
    sfRectangleShape *sq = om->gd->ui_shape;

    centered(om, "Calibrate the audio offset", 210.0f);
    centered(om, "A click and a white square, twice a second. Move the", 290.0f);
    centered(om, "offset until you hear the click as the square lights:", 340.0f);
    centered(om, "more if you hear it late, less if it comes first.", 390.0f);
    sfRectangleShape_setPosition(sq, (sfVector2f){885.0f, 460.0f});
    sfRectangleShape_setSize(sq, (sfVector2f){150.0f, 150.0f});
    sfRectangleShape_setFillColor(sq, (sfColor){255, 255, 255,
        (sfUint8)(40.0f + 215.0f * lit)});
    sfRectangleShape_setOutlineThickness(sq, 4.0f);
    sfRectangleShape_setOutlineColor(sq, sfWhite);
    draw_rect(om->gd, sq);
}

void options_draw_dialog(option_m_t *om)
{
    char line[120];
    int64_t left = om->trial.until_ms - ui_now_ms();

    if (om->dialog == DIALOG_REVERT) {
        snprintf(line, sizeof(line), "Keep these display settings? Reverting"
            " in %d", (int)((left + 999) / 1000));
        centered(om, line, 450.0f);
    }
    if (om->dialog == DIALOG_CALIBRATE)
        draw_calibration(om);
    if (om->dialog == DIALOG_RESET) {
        centered(om, "Delete all attempts, best percentages and per-level"
            " song choices?", 420.0f);
        centered(om, "This cannot be undone (a copy is kept as"
            " progress.txt.bak).", 490.0f);
    }
}

/*
** xdg-open, without a shell, from a grandchild: the child exits at once, so
** nothing is left to reap. The pipe closes on its own when xdg-open starts
** (close-on-exec); a byte in it is an exec that failed.
*/
static bool start_opener(const char *path)
{
    int fds[2];
    int status = 1;
    char c = 0;
    pid_t child;

    if (pipe(fds) != 0)
        return false;
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    child = fork();
    if (child == 0) {
        close(fds[0]);
        if ((child = fork()) == 0) {
            execlp("xdg-open", "xdg-open", path, (char *)NULL);
            (void)!write(fds[1], "x", 1);
            _exit(127);
        }
        _exit(child < 0);
    }
    close(fds[1]);
    if (child > 0)
        waitpid(child, &status, 0);
    status = child > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0
        && read(fds[0], &c, 1) == 0;
    close(fds[0]);
    return status;
}

void options_open_save_folder(option_m_t *om)
{
    char path[PATH_MAX];
    char text[PATH_MAX + 32];

    mkdir("save", 0755);
    if (realpath("save", path) == NULL)
        return options_notice(om, "There's no save folder");
    printf("%s\n", path);
    fflush(stdout);
    snprintf(text, sizeof(text), start_opener(path) ? "Opened %s"
        : "Couldn't open it: %s", path);
    options_notice(om, text);
}
