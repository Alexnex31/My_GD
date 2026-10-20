/*
** ALEXNEX PROJECT, 2026
** music/credits.h
** File description:
** header file for my_gd project
*/

#ifndef MUSIC_CREDITS_H
    #define MUSIC_CREDITS_H

    #include "music/library.h"

    #define CREDITS_FILE "res/CREDITS.md"
    #define CREDITS_WRAP 60       /* characters on a line of the screen   */

/*
** The Credits screen's lines (FEATURES 4.11): every song of the library,
** "Title — Artist (License)", then res/CREDITS.md's lines, its headings'
** `#` taken off and long lines wrapped at a space. `md` is NULL when the
** file isn't there: a line says where it goes. NULL-terminated, `*count`
** of them; credits_free frees them.
*/
char **credits_rows(const library_t *lib, const char *md, int *count);
void credits_free(char **rows);

#endif
