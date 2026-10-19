/*
** ALEXNEX PROJECT, 2026
** sim/binding.c
** File description:
** key, mouse and gamepad names, as the settings file writes them (FEATURES 2.2)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "sim/binding.h"

#define NAME_STRING(name) #name,

static const char *const KEY_NAMES[KEY_COUNT] = {KEY_LIST(NAME_STRING)};
static const char *const MOUSE_NAMES[MOUSE_COUNT] = {MOUSE_LIST(NAME_STRING)};

/* "Joy" then 0 to JOY_BUTTONS - 1, digits only. */
static bool parse_joy(const char *digits, int *out)
{
    char *end = NULL;
    long n = 0;

    if (digits[0] < '0' || digits[0] > '9')
        return false;
    n = strtol(digits, &end, 10);
    if (*end != '\0' || n >= JOY_BUTTONS)
        return false;
    *out = (int)n;
    return true;
}

static const struct {
    const char *prefix;
    int mod;
} MODS[] = {{"Ctrl+", MOD_CTRL}, {"Shift+", MOD_SHIFT}, {"Alt+", MOD_ALT}};

/* "Ctrl+Shift+", in any order and case, each once: their bits, the rest. */
static int parse_mods(const char **name)
{
    int mods = 0;
    bool more = true;

    while (more) {
        more = false;
        for (size_t i = 0; i < sizeof(MODS) / sizeof(MODS[0]); i++) {
            size_t len = strlen(MODS[i].prefix);

            if (strncasecmp(*name, MODS[i].prefix, len) != 0
                || (mods & MODS[i].mod))
                continue;
            mods |= MODS[i].mod;
            *name += len;
            more = true;
        }
    }
    return mods;
}

/* A key with modifiers: only keys are combinations (the editor's). */
bool binding_parse(const char *name, binding_t *out)
{
    int mods = parse_mods(&name);

    for (int i = 0; i < KEY_COUNT; i++)
        if (strcasecmp(name, KEY_NAMES[i]) == 0)
            return *out = (binding_t){BIND_KEY, i, mods}, true;
    if (mods != 0)
        return false;
    if (strncasecmp(name, "Mouse", 5) == 0)
        for (int i = 0; i < MOUSE_COUNT; i++)
            if (strcasecmp(name + 5, MOUSE_NAMES[i]) == 0)
                return *out = (binding_t){BIND_MOUSE, i, 0}, true;
    if (strncasecmp(name, "Joy", 3) == 0 && parse_joy(name + 3, &out->code))
        return out->kind = BIND_JOY, true;
    return false;
}

void binding_format(binding_t b, char *buf, size_t size)
{
    if (b.kind == BIND_KEY && b.code >= 0 && b.code < KEY_COUNT)
        snprintf(buf, size, "%s%s%s%s", (b.mods & MOD_CTRL) ? "Ctrl+" : "",
            (b.mods & MOD_SHIFT) ? "Shift+" : "",
            (b.mods & MOD_ALT) ? "Alt+" : "", KEY_NAMES[b.code]);
    else if (b.kind == BIND_MOUSE && b.code >= 0 && b.code < MOUSE_COUNT)
        snprintf(buf, size, "Mouse%s", MOUSE_NAMES[b.code]);
    else if (b.kind == BIND_JOY)
        snprintf(buf, size, "Joy%d", b.code);
    else
        snprintf(buf, size, "%s", "");
}

bool binding_equal(binding_t a, binding_t b)
{
    return a.kind == b.kind && (a.kind == BIND_NONE
        || (a.code == b.code && a.mods == b.mods));
}
