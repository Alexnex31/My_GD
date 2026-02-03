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
