/*
** ALEXNEX PROJECT, 2026
** utilitary
** File description:
** useful functions
*/

#include "mygd.h"

sfVector2u create_vector(int x, int y)
{
    sfVector2u vector;

    vector.x = x;
    vector.y = y;
    return vector;
}

sfVector2f create_vector_f(float x, float y)
{
    sfVector2f vector;

    vector.x = x;
    vector.y = y;
    return vector;
}

char *int_to_str(int nb)
{
    char *str = malloc(20);
    int i = 0;
    int temp = nb;
    int len = 0;

    if (nb == 0) {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }
    while (temp > 0) {
        temp /= 10;
        len += 1;
    }
    i = len - 1;
    while (nb > 0) {
        str[i] = (nb % 10) + '0';
        nb /= 10;
        i -= 1;
    }
    str[len] = '\0';
    return str;
}

char *float_to_str(float nb)
{
    char *str = malloc(20);
    int integer_part = (int)nb;
    int decimal_part = (int)((nb - integer_part) * 100);
    char *int_str = int_to_str(integer_part);
    char *dec_str = int_to_str(decimal_part);
    int i = 0;
    int j = 0;

    while (int_str[i] != '\0') {
        str[i] = int_str[i];
        i += 1;
    }
    str[i] = '.';
    i += 1;
    if (decimal_part < 10) {
        str[i] = '0';
        i += 1;
    }
    while (dec_str[j] != '\0') {
        str[i] = dec_str[j];
        i += 1;
        j += 1;
    }
    str[i] = '%';
    str[i + 1] = '\0';
    free(int_str);
    free(dec_str);
    return str;
}
