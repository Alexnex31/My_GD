/*
** ALEXNEX PROJECT, 2026
** sim/binding.c
** File description:
** key, mouse and gamepad names, as the settings file writes them (FEATURES 2.2)
*/

#include <stdio.h>
#include <stdlib.h>
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

bool binding_parse(const char *name, binding_t *out)
{
    for (int i = 0; i < KEY_COUNT; i++)
        if (strcasecmp(name, KEY_NAMES[i]) == 0)
            return *out = (binding_t){BIND_KEY, i}, true;
    if (strncasecmp(name, "Mouse", 5) == 0)
        for (int i = 0; i < MOUSE_COUNT; i++)
            if (strcasecmp(name + 5, MOUSE_NAMES[i]) == 0)
                return *out = (binding_t){BIND_MOUSE, i}, true;
    if (strncasecmp(name, "Joy", 3) == 0 && parse_joy(name + 3, &out->code))
        return out->kind = BIND_JOY, true;
    return false;
}

void binding_format(binding_t b, char *buf, size_t size)
{
    if (b.kind == BIND_KEY && b.code >= 0 && b.code < KEY_COUNT)
        snprintf(buf, size, "%s", KEY_NAMES[b.code]);
    else if (b.kind == BIND_MOUSE && b.code >= 0 && b.code < MOUSE_COUNT)
        snprintf(buf, size, "Mouse%s", MOUSE_NAMES[b.code]);
    else if (b.kind == BIND_JOY)
        snprintf(buf, size, "Joy%d", b.code);
    else
        snprintf(buf, size, "%s", "");
}

bool binding_equal(binding_t a, binding_t b)
{
    return a.kind == b.kind && (a.kind == BIND_NONE || a.code == b.code);
}
