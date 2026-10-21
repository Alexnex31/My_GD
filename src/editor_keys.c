/*
** ALEXNEX PROJECT, 2026
** editor_keys
** File description:
** the editor's shortcuts: what each action does, and the list of them (FEATURES 11.6)
*/

#include "editor.h"

static const sfColor SHEET = {18, 20, 32, 240};
static const sfColor QUIET = {180, 190, 210, 255};

/* With a selection it moves; with none, Up and Down go through the palette. */
static void move(editor_t *ed, double dx, double dy, bool palette)
{
    if (ed_selected(&ed->lv) > 0) {
        ed_nudge(&ed->lv, &ed->hist, dx, dy);
        return;
    }
    if (!palette || dy == 0.0)
        return;
    ed->entry = (ed->entry + (dy > 0.0 ? 1 : ed->nb_entries - 1))
        % ed->nb_entries;
    ed->tool = ED_TOOL_PLACE;
}

static void home(editor_t *ed)
{
    ed->zoom = 1.0f;                         /* where it opened, at 100% */
    ed->zoom_step = 0;
    ed->cam = (vec2_t){ED_CANVAS_W / 2.0 - 100.0, GROUND_Y - 300.0};
}

static void entry(editor_t *ed, int i)
{
    if (i >= ed->nb_entries)
        return;
    ed->entry = i;
    ed->tool = ED_TOOL_PLACE;
}

/* The edits: the history, the selection, the clipboard. */
static bool edit_action(editor_t *ed, gd_t *gd, ed_key_t k)
{
    if (k == ED_KEY_SAVE)
        editor_save(ed);
    if (k == ED_KEY_UNDO && !ed_undo(&ed->lv, &ed->hist))
        editor_notice(ed, "Nothing to undo");
    if (k == ED_KEY_REDO && !ed_redo(&ed->lv, &ed->hist))
        editor_notice(ed, "Nothing to redo");
    if (k == ED_KEY_SELECT_ALL) {
        ed_select_all(&ed->lv);
        ed->tool = ED_TOOL_SELECT;
    }
    if (k == ED_KEY_COPY && ed_copy(&ed->lv, &gd->clip))
        editor_notice(ed, "Copied");
    if (k == ED_KEY_PASTE && editor_mouse_on_canvas(gd) && ed_paste(&ed->lv,
        &ed->hist, &gd->clip, editor_mouse_world(ed, gd), ed->grid))
        ed->tool = ED_TOOL_SELECT;
    if (k == ED_KEY_DUPLICATE)
        ed_duplicate(&ed->lv, &ed->hist, ed->grid);
    if (k == ED_KEY_DELETE)
        ed_delete_selected(&ed->lv, &ed->hist);
    if (k == ED_KEY_TOOL)
        ed->tool = ed->tool == ED_TOOL_PLACE ? ED_TOOL_SELECT : ED_TOOL_PLACE;
    return k <= ED_KEY_TOOL;
}

/* A grid step, a pixel, a whole block; a quarter turn, 15 degrees (11.4). */
static bool move_action(editor_t *ed, ed_key_t k)
{
    static const double DX[] = {-1, 1, 0, 0};
    static const double DY[] = {0, 0, -1, 1};

    if (k >= ED_KEY_MOVE_LEFT && k <= ED_KEY_MOVE_DOWN)
        move(ed, DX[k - ED_KEY_MOVE_LEFT] * ed->grid,
            DY[k - ED_KEY_MOVE_LEFT] * ed->grid, true);
    if (k >= ED_KEY_NUDGE_LEFT && k <= ED_KEY_NUDGE_DOWN)
        move(ed, DX[k - ED_KEY_NUDGE_LEFT], DY[k - ED_KEY_NUDGE_LEFT], false);
    if (k >= ED_KEY_BLOCK_LEFT && k <= ED_KEY_BLOCK_DOWN)
        move(ed, DX[k - ED_KEY_BLOCK_LEFT] * ED_BIG_STEP,
            DY[k - ED_KEY_BLOCK_LEFT] * ED_BIG_STEP, false);
    if (k == ED_KEY_ROTATE || k == ED_KEY_ROTATE_BACK)
        ed_rotate(&ed->lv, &ed->hist, k == ED_KEY_ROTATE ? 90.0 : -90.0,
            ed->grid);
    if (k == ED_KEY_TURN || k == ED_KEY_TURN_BACK)
        ed_prop_step(&ed->lv, &ed->hist, ED_PROP_TURN,
            k == ED_KEY_TURN ? 1 : -1);
    return k <= ED_KEY_TURN_BACK;
}

void editor_key_action(editor_t *ed, gd_t *gd, ed_key_t k)
{
    if (edit_action(ed, gd, k) || move_action(ed, k))
        return;
    if (k == ED_KEY_PLAYTEST || k == ED_KEY_PLAYTEST_START)
        return editor_playtest(ed, gd, k == ED_KEY_PLAYTEST_START);
    if (k == ED_KEY_SONG)
        editor_song_toggle(ed, gd);
    if (k == ED_KEY_VERIFY)
        editor_verify_start(ed);
    if (k == ED_KEY_NEXT_ISSUE)
        editor_next_issue(ed);
    if (k == ED_KEY_LEVEL_PANEL)
        editor_settings_open(ed);
    if (k == ED_KEY_GRID)
        ed->grid = ed->grid == 50.0 ? 25.0 : 50.0;
    if (k == ED_KEY_LAYER_DOWN || k == ED_KEY_LAYER_UP)   /* All, 0, 1 (11.15) */
        ed_set_layer(&ed->lv, ed->lv.layer + (k == ED_KEY_LAYER_UP ? 1 : -1));
    if (k == ED_KEY_HOME)
        home(ed);
    if (k == ED_KEY_KEYS)
        ed->show_keys = !ed->show_keys;
    if (k >= ED_KEY_ENTRY_1 && k <= ED_KEY_ENTRY_9)
        entry(ed, k - ED_KEY_ENTRY_1);
}

/* The name of what a key does now, "-" when it has none. */
const char *editor_key_name(gd_t *gd, ed_key_t k, char *buf, size_t size)
{
    binding_format(gd->settings.editor_keys[k], buf, size);
    if (buf[0] == '\0')
        snprintf(buf, size, "%s", "-");
    return buf;
}

/*
** Every action and its key, as the options set them (11.6): three columns
** over the canvas, until the same key again, or Escape.
*/
void editor_keys_draw(editor_t *ed, gd_t *gd)
{
    const float col_w = (ED_CANVAS_W - 60.0f) / 3.0f;
    const int per_col = (ED_KEY_COUNT + 2) / 3;
    char name[BINDING_NAME_MAX];

    if (!ed->show_keys)
        return;
    editor_fill(gd, ed, (sfFloatRect){ED_PALETTE_W + 10.0f, ED_TOOLBAR_H
        + 10.0f, ED_CANVAS_W - 20.0f, ED_CANVAS_H - ED_STATUS_H - 20.0f},
        SHEET, (sfColor){90, 255, 140, 255});
    editor_label(ed, gd, "Keys: the options' Editor section changes them. "
        "Mouse: left places or selects, right deletes, middle or "
        "Space+left pans, wheel scrolls (Shift: up and down, Ctrl: zoom)",
        ED_PALETTE_W + 30.0f, ED_TOOLBAR_H + 20.0f, 18, QUIET);
    for (int k = 0; k < ED_KEY_COUNT; k++) {
        float x = ED_PALETTE_W + 30.0f + col_w * (float)(k / per_col);
        float y = ED_TOOLBAR_H + 60.0f + 25.0f * (float)(k % per_col);

        editor_label_fit(ed, gd, ed_key_label((ed_key_t)k), (sfVector2f){x, y},
            col_w * 0.55f, 18, QUIET);
        editor_label_fit(ed, gd, editor_key_name(gd, (ed_key_t)k, name,
            sizeof(name)), (sfVector2f){x + col_w * 0.58f, y},
            col_w * 0.4f, 18, sfWhite);
    }
}
