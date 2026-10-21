/*
** ALEXNEX PROJECT, 2026
** ui/options_rules.c
** File description:
** the options screen's rules: rebinding, window sizes (FEATURES 5.2, 5.5)
*/

#include <stdio.h>

#include "ui/options_rules.h"

static const binding_t NONE = {BIND_NONE, 0, 0};

void slots_from_settings(const settings_t *s, binding_t slots[NB_SLOTS])
{
    for (int i = 0; i < BINDINGS_MAX; i++)
        slots[SLOT_JUMP + i] = i < s->jump_bindings.count
            ? s->jump_bindings.items[i] : NONE;
    slots[SLOT_RESTART] = s->restart_key;
    slots[SLOT_CHECKPOINT] = s->checkpoint_key;
    slots[SLOT_REMOVE_CHECKPOINT] = s->remove_checkpoint_key;
    slots[SLOT_DEBUG_OVERLAY] = s->debug_overlay_key;
    slots[SLOT_PREVIOUS_START] = s->previous_start_key;
    slots[SLOT_NEXT_START] = s->next_start_key;
}

void slots_to_settings(settings_t *s, const binding_t slots[NB_SLOTS])
{
    s->jump_bindings.count = 0;
    for (int i = 0; i < BINDINGS_MAX; i++)
        if (slots[SLOT_JUMP + i].kind != BIND_NONE)
            s->jump_bindings.items[s->jump_bindings.count++]
                = slots[SLOT_JUMP + i];
    s->restart_key = slots[SLOT_RESTART];
    s->checkpoint_key = slots[SLOT_CHECKPOINT];
    s->remove_checkpoint_key = slots[SLOT_REMOVE_CHECKPOINT];
    s->debug_overlay_key = slots[SLOT_DEBUG_OVERLAY];
    s->previous_start_key = slots[SLOT_PREVIOUS_START];
    s->next_start_key = slots[SLOT_NEXT_START];
}

/*
** The jump inputs the game reads: keys and mouse buttons. Gamepads aren't
** polled yet (input.c), so a gamepad button alone would leave no jump.
*/
static int jump_inputs(const binding_t slots[NB_SLOTS])
{
    int n = 0;

    for (int i = 0; i < BINDINGS_MAX; i++)
        n += slots[SLOT_JUMP + i].kind == BIND_KEY
            || slots[SLOT_JUMP + i].kind == BIND_MOUSE;
    return n;
}

/*
** Swapping beats refusing: the player never gets stuck (5.5). The only
** refusal: a change that would leave nothing to jump with.
*/
rebind_result_t rebind(binding_t slots[NB_SLOTS], int slot, binding_t b,
    int *other)
{
    binding_t before[NB_SLOTS];
    rebind_result_t result = REBIND_SET;

    *other = -1;
    if (binding_equal(slots[slot], b))
        return REBIND_SAME;
    for (int i = 0; i < NB_SLOTS; i++)
        before[i] = slots[i];
    for (int i = 0; i < NB_SLOTS && b.kind != BIND_NONE; i++)
        if (i != slot && binding_equal(slots[i], b)) {
            slots[i] = slots[slot];
            *other = i;
            result = REBIND_SWAPPED;
        }
    slots[slot] = b;
    if (b.kind == BIND_NONE)
        result = REBIND_CLEARED;
    if (jump_inputs(slots) == 0) {
        for (int i = 0; i < NB_SLOTS; i++)
            slots[i] = before[i];
        *other = -1;
        return REBIND_REFUSED;
    }
    return result;
}

rebind_result_t editor_rebind(binding_t keys[ED_KEY_COUNT], int k, binding_t b,
    int *other)
{
    *other = -1;
    if (!ed_key_bindable(b))
        return REBIND_REFUSED;
    if (binding_equal(keys[k], b))
        return REBIND_SAME;
    *other = b.kind == BIND_NONE ? -1 : ed_key_find(keys, b);
    if (*other >= 0)
        keys[*other] = keys[k];
    keys[k] = b;
    if (b.kind == BIND_NONE)
        return REBIND_CLEARED;
    return *other >= 0 ? REBIND_SWAPPED : REBIND_SET;
}

static const char *const ACTION_NAMES[NB_SLOTS - SLOT_RESTART] = {
    "Restart", "Place checkpoint", "Remove checkpoint", "Debug overlay",
    "Previous start position", "Next start position"};

void slot_name(int slot, char *buf, size_t size)
{
    if (slot < SLOT_RESTART)
        snprintf(buf, size, "Jump %d", slot - SLOT_JUMP + 1);
    else
        snprintf(buf, size, "%s", ACTION_NAMES[slot - SLOT_RESTART]);
}

static void add_size(window_sizes_t *ws, int w, int h)
{
    ws->w[ws->count] = w;
    ws->h[ws->count] = h;
    snprintf(ws->label[ws->count], sizeof(ws->label[0]), "%dx%d", w, h);
    ws->labels[ws->count] = ws->label[ws->count];
    ws->count += 1;
}

static void set_display(settings_t *s, bool fullscreen, int w, int h)
{
    s->fullscreen = fullscreen;
    s->window_width = w;
    s->window_height = h;
}

void display_trial_begin(display_trial_t *t, settings_t *s, bool fullscreen,
    int w, int h, int64_t now_ms)
{
    *t = (display_trial_t){true, s->fullscreen, s->window_width,
        s->window_height, now_ms + DISPLAY_TRIAL_MS};
    set_display(s, fullscreen, w, h);
}

bool display_trial_due(const display_trial_t *t, int64_t now_ms)
{
    return t->open && now_ms >= t->until_ms;
}

void display_trial_end(display_trial_t *t, settings_t *s, bool keep)
{
    if (!t->open)
        return;
    t->open = false;
    if (keep)
        s->dirty = true;
    else
        set_display(s, t->fullscreen, t->w, t->h);
}

void window_sizes_list(window_sizes_t *ws, int desk_w, int desk_h,
    int cur_w, int cur_h)
{
    static const int SIZES[3][2] = {{1280, 720}, {1600, 900}, {1920, 1080}};
    bool listed = false;

    *ws = (window_sizes_t){0};
    for (int i = 0; i < 3; i++)
        listed |= SIZES[i][0] == cur_w && SIZES[i][1] == cur_h
            && SIZES[i][0] <= desk_w && SIZES[i][1] <= desk_h;
    if (!listed)
        add_size(ws, cur_w, cur_h);          /* set in the file, or too big */
    for (int i = 0; i < 3; i++)
        if (SIZES[i][0] <= desk_w && SIZES[i][1] <= desk_h)
            add_size(ws, SIZES[i][0], SIZES[i][1]);
    if (ws->count == 0)
        add_size(ws, SIZES[0][0], SIZES[0][1]);   /* a tiny desktop */
    for (int i = 0; i < ws->count; i++)
        if (ws->w[i] == cur_w && ws->h[i] == cur_h)
            ws->current = i;
}
