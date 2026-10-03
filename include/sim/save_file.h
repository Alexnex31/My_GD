/*
** ALEXNEX PROJECT, 2026
** sim/save_file.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_SAVE_FILE_H
    #define SIM_SAVE_FILE_H

    #include <stdio.h>

typedef void (*save_write_fn)(FILE *f, const void *data);

/*
** Writes `path` through `path`.tmp, fsync, then rename, making its folder if
** needed: a crash or a power cut leaves the old file whole (PLAN 6.4,
** FEATURES 2.4). 0, or -1 with the old file untouched.
*/
int save_atomic(const char *path, save_write_fn write, const void *data);

#endif
