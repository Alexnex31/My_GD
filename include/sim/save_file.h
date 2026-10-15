/*
** ALEXNEX PROJECT, 2026
** sim/save_file.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_SAVE_FILE_H
    #define SIM_SAVE_FILE_H

    #include <stdio.h>

    #define SAVE_FILE_MAX (16 << 20)  /* bigger is no save of ours: unreadable */

typedef void (*save_write_fn)(FILE *f, const void *data);

typedef enum save_read {
    SAVE_MISSING,                 /* no file yet: a first run                 */
    SAVE_READ,
    SAVE_UNREADABLE               /* there, but not read whole: never
                                     overwrite it                             */
} save_read_t;

/*
** Writes `path` through a temporary file beside it, fsync, then rename, making
** its folder if needed: a crash or a power cut leaves the old file whole
** (PLAN 6.4, FEATURES 2.4). A symlinked save writes its target, and the file
** keeps its permissions. 0, or -1 with the old file untouched.
*/
int save_atomic(const char *path, save_write_fn write, const void *data);

/*
** The whole file in `*text`, NUL-terminated, for the caller to free (NULL
** unless SAVE_READ). A NUL byte in it ends a line, as a '\n' does.
*/
save_read_t save_read(const char *path, char **text);

/*
** The next line of a text from save_read, cut there and without its '\r', or
** NULL at the end. `*cursor` starts at the text and moves past the line.
*/
char *save_next_line(char **cursor);

#endif
