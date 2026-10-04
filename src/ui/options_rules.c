/*
** ALEXNEX PROJECT, 2026
** ui/options_rules.c
** File description:
** the options screen's rules: rebinding, window sizes (FEATURES 5.2, 5.5)
*/

#include <stdio.h>

#include "ui/options_rules.h"

static const binding_t NONE = {BIND_NONE, 0};

void slots_from_settings(const settings_t *s, binding_t slots[NB_SLOTS])
{
    for (int i = 0; i < BINDINGS_MAX; i++)
        slots[SLOT_JUMP + i] = i < s->jump_bindings.count
            ? s->jump_bindings.items[i] : NONE;
    slots[SLOT_RESTART] = s->restart_key;
    slots[SLOT_CHECKPOINT] = s->checkpoint_key;
    slots[SLOT_REMOVE_CHECKPOINT] = s->remove_checkpoint_key;
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
}

static int jump_inputs(const binding_t slots[NB_SLOTS])
{
    int n = 0;

    for (int i = 0; i < BINDINGS_MAX; i++)
        n += slots[SLOT_JUMP + i].kind != BIND_NONE;
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

void slot_name(int slot, char *buf, size_t size)
{
    if (slot < SLOT_RESTART)
        snprintf(buf, size, "Jump %d", slot - SLOT_JUMP + 1);
    else if (slot == SLOT_RESTART)
        snprintf(buf, size, "%s", "Restart");
    else if (slot == SLOT_CHECKPOINT)
        snprintf(buf, size, "%s", "Place checkpoint");
    else
        snprintf(buf, size, "%s", "Remove checkpoint");
}

static void add_size(window_sizes_t *ws, int w, int h)
{
    ws->w[ws->count] = w;
    ws->h[ws->count] = h;
    snprintf(ws->label[ws->count], sizeof(ws->label[0]), "%dx%d", w, h);
    ws->labels[ws->count] = ws->label[ws->count];
    ws->count += 1;
}

void window_sizes_list(window_sizes_t *ws, int desk_w, int desk_h,
    int cur_w, int cur_h)
{
    static const int SIZES[3][2] = {{1280, 720}, {1600, 900}, {1920, 1080}};
    bool listed = false;

    *ws = (window_sizes_t){0};
    for (int i = 0; i < 3; i++)
        listed |= SIZES[i][0] == cur_w && SIZES[i][1] == cur_h;
    if (!listed)
        add_size(ws, cur_w, cur_h);          /* a size set in the file */
    for (int i = 0; i < 3; i++)
        if (SIZES[i][0] <= desk_w && SIZES[i][1] <= desk_h)
            add_size(ws, SIZES[i][0], SIZES[i][1]);
    if (ws->count == 0)
        add_size(ws, SIZES[0][0], SIZES[0][1]);   /* a tiny desktop */
    for (int i = 0; i < ws->count; i++)
        if (ws->w[i] == cur_w && ws->h[i] == cur_h)
            ws->current = i;
}
