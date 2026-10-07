/*
** ALEXNEX PROJECT, 2026
** sim/alloc.c
** File description:
** header file for my_gd project
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sim/alloc.h"

void *sim_xcalloc(size_t n, size_t size)
{
    void *p = calloc(n, size);

    if (p == NULL) {
        write(2, "my_gd: out of memory\n", 21);
        exit(84);
    }
    return p;
}

char *sim_xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *copy = sim_xcalloc(n, 1);

    memcpy(copy, s, n);
    return copy;
}
