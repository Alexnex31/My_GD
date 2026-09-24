/*
** ALEXNEX PROJECT, 2026
** sim/alloc.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_ALLOC_H
    #define SIM_ALLOC_H

    #include <stddef.h>

void *sim_xcalloc(size_t n, size_t size);   /* calloc, or write an error and exit(84) */

#endif
