/*
** ALEXNEX PROJECT, 2026
** my_put_nbr
** File description:
** display the number given as a parameter
** can display all possible values of an int
*/
#include "mygd.h"

int my_put_nbr(int nb)
{
    long n = nb;

    if (n < 0) {
        my_putchar('-');
        n = -n;
    }
    if (n >= 10)
        my_put_nbr((int)(n / 10));
    my_putchar('0' + n % 10);
    return 0;
}
