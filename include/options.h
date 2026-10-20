/*
** ALEXNEX PROJECT, 2026
** options.h
** File description:
** header file for my_gd project
*/

#ifndef GD_OPTIONS_H
    #define GD_OPTIONS_H

    #include <SFML/Graphics.h>

    #include "ui/options_rules.h"
    #include "ui/ui.h"

typedef struct gd gd_t;

typedef enum options_section {
    SEC_AUDIO,
    SEC_GAMEPLAY,
    SEC_CONTROLS,
    SEC_EDITOR,                   /* the editor's shortcuts (11.6)          */
    SEC_DISPLAY,
    SEC_DATA,
    NB_SECTIONS
} options_section_t;

typedef enum options_dialog {
    DIALOG_NONE,
    DIALOG_REVERT,                /* "Keep these display settings?" (5.4)   */
    DIALOG_RESET,                 /* "Delete all attempts...?" (5.6)        */
    DIALOG_CALIBRATE              /* the click and the square (5.2)         */
} options_dialog_t;

    #define OPT_ROWS_MAX 10
    #define OPT_LABEL_X 560.0f
    #define OPT_ROW_Y 200.0f
    #define OPT_ROW_STEP 100.0f

/*
** The options scene (FEATURES 5). The widgets point at the ints below; each
** change goes to gd->settings and is applied at once. Callbacks only set
** the `pending_*` fields: the scene acts on them after its events (3.5).
*/
typedef struct option_menu {
    gd_t *gd;
    sfSprite *background;
    sfText *text;                 /* titles, hints, notices: one, reused    */
    options_section_t section;
    ui_screen_t tabs;             /* the sections and Back: the mouse's     */
    widget_t tab_widgets[NB_SECTIONS + 1];
    char tab_labels[NB_SECTIONS][24];
    ui_screen_t rows;             /* the section's rows: every event        */
    int rows_of;                  /* the section they were built for, -1    */
    widget_t row_widgets[OPT_ROWS_MAX];
    int music_volume;
    int sfx_volume;
    int menu_song;
    int audio_offset;
    int show_percent;
    int show_progress_bar;
    int show_attempts;
    int fullscreen;
    int window_size;
    int vsync;
    int fps;
    binding_t slots[NB_SLOTS];
    int editor_page;              /* which of the editor's actions are rows */
    ed_key_t page_keys[ED_KEY_PAGE_MAX];   /* row 1 + i is page_keys[i]     */
    const char *page_names[ED_KEY_PAGES];
    const char **song_titles;
    window_sizes_t sizes;
    options_dialog_t dialog;
    ui_screen_t dialog_ui;
    widget_t dialog_widgets[2];
    display_trial_t trial;        /* a new display mode, until kept (5.4)   */
    calib_t calib;                /* its beats, while it's open             */
    char notice[200];
    int64_t notice_until_ms;
    bool pending_leave;
    bool pending_display;
    bool pending_rows;            /* another page of the Editor section     */
    int pending_section;          /* -1 none                                */
    options_dialog_t pending_dialog;
    int pending_answer;           /* 0 none, 1 the first button, 2 the second */
} option_m_t;

option_m_t *create_option_menu(gd_t *gd);
void free_option_menu(option_m_t *om);
void print_option_menu(option_m_t *om, gd_t *gd);
void keyboard_events_option_menu(option_m_t **om, gd_t *gd);

/* The rows of each section, built when it's shown (options_rows.c). */
void options_build_rows(option_m_t *om);
void options_notice(option_m_t *om, const char *text);

/* Shared with the calibration's own slider (5.2). */
void options_audio_offset_changed(void *ctx, widget_t *w);

/* The dialogs and what they do (options_dialogs.c). */
void options_open_dialog(option_m_t *om, options_dialog_t which);
void options_answer(option_m_t *om, int answer);
void options_forget_display(option_m_t *om);   /* an unkept mode, on quit */
void options_apply_display(option_m_t *om);
void options_dialogs_update(option_m_t *om);
void options_draw_dialog(option_m_t *om);
void options_open_save_folder(option_m_t *om);

#endif
