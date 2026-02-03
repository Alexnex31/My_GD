/*
** ALEXNEX PROJECT, 2026
** my_putstr
** File description:
** display one by one the characters of a string
*/
#include "mygd.h"

int my_putstr(char const *str)
{
    int i = 0;

    while (str[i] != '\0') {
        my_putchar(str[i]);
        i = i + 1;
    }
    return 0;
}
