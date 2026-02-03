/*
** ALEXNEX PROJECT, 2026
** my_putchar
** File description:
** write a char
*/
#include <unistd.h>

void my_putchar(char c)
{
    write(1, &c, 1);
}
