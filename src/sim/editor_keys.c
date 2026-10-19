/*
** ALEXNEX PROJECT, 2026
** sim/editor_keys.c
** File description:
** the editor's shortcuts: what each is called, where it is listed, its default (FEATURES 11.6)
*/

#include "sim/editor_keys.h"

typedef struct ed_key_def {
    const char *name;
    const char *label;
    int page;
    binding_t def;
    bool repeats;
} ed_key_def_t;

#define ED_KEY_DEF(id, name, label, page, key, mods, rep) \
    {name, label, page, {BIND_KEY, key, mods}, rep},

static const ed_key_def_t DEFS[ED_KEY_COUNT] = {ED_KEY_LIST(ED_KEY_DEF)};

static const char *const PAGES[ED_KEY_PAGES] = {"Edit", "Move",
    "Blocks, turns", "Play", "View", "Palette"};

const char *ed_key_name(ed_key_t k)
{
    return DEFS[k].name;
}

const char *ed_key_label(ed_key_t k)
{
    return DEFS[k].label;
}

const char *ed_key_page_name(int page)
{
    return PAGES[page];
}

int ed_key_page(ed_key_t k)
{
    return DEFS[k].page;
}

binding_t ed_key_default(ed_key_t k)
{
    return DEFS[k].def;
}

bool ed_key_repeats(ed_key_t k)
{
    return DEFS[k].repeats;
}

int ed_key_page_list(int page, ed_key_t *out)
{
    int n = 0;

    for (int k = 0; k < ED_KEY_COUNT && n < ED_KEY_PAGE_MAX; k++)
        if (DEFS[k].page == page)
            out[n++] = (ed_key_t)k;
    return n;
}

static bool is_modifier(int code)
{
    return code == KEY_LControl || code == KEY_LShift || code == KEY_LAlt
        || code == KEY_LSystem || code == KEY_RControl || code == KEY_RShift
        || code == KEY_RAlt || code == KEY_RSystem;
}

bool ed_key_bindable(binding_t b)
{
    if (b.kind == BIND_NONE)
        return true;
    return b.kind == BIND_KEY && b.code >= 0 && b.code < KEY_COUNT
        && b.code != KEY_Escape && !is_modifier(b.code)
        && (b.mods & ~(MOD_CTRL | MOD_SHIFT | MOD_ALT)) == 0;
}

int ed_key_find(const binding_t keys[ED_KEY_COUNT], binding_t pressed)
{
    if (pressed.kind == BIND_NONE)
        return -1;
    for (int k = 0; k < ED_KEY_COUNT; k++)
        if (binding_equal(keys[k], pressed))
            return k;
    return -1;
}
