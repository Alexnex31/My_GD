/*
** ALEXNEX PROJECT, 2026
** tests/main.c
** File description:
** functions to create and manage a window
*/

#include "test.h"

int failures = 0;

int main(void)
{
    test_constants();
    if (failures == 0)
        printf("all tests passed\n");
    else
        printf("%d check(s) failed\n", failures);
    return failures != 0;
}
