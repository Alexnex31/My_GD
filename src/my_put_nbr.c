/*
** ALEXNEX PROJECT, 2026
** my_put_nbr
** File description:
** display the number given as a parameter
** can display all possible values of an int
*/
#include "mygd.h"

int set_power(int i)
{
    int power = 1;

    while (i > 1) {
        power = power * 10;
        i = i - 1;
    }
    return (power);
}

int len_int(int nb)
{
    int length = 1;

    while (nb != 0) {
        nb = nb / 10;
        length = length + 1;
    }
    return (length);
}

int reverse_write(int nb)
{
    int d = 0;
    int length = len_int(nb);
    int nb_c = nb;

    while (length > 1) {
        d = nb / set_power(length - 1);
        if (d < 0) {
            d = -d;
        }
        nb = nb % set_power(length - 1);
        length = length - 1;
        my_putchar(48 + d);
    }
    return (0);
}

int my_put_nbr(int nb)
{
    if (nb == 0) {
        my_putchar('0');
        return (0);
    }
    if (nb < 0) {
        my_putchar('-');
        nb = -nb;
    }
    reverse_write(nb);
    return (0);
}
