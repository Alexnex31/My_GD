/*
** ALEXNEX PROJECT, 2026
** my_str_to_word_array
** File description:
** split a string into words in an array
*/
#include <stdlib.h>
#include "mygd.h"

int my_is_alphanumerical(char *str, int i)
{
    int result = 0;

    if (str[i] >= '0' && str[i] <= '9') {
        result = 1;
    }
    if (str[i] >= 65 && str[i] <= 90) {
        result = 1;
    }
    if (str[i] >= 97 && str[i] <= 122) {
        result = 1;
    }
    return result;
}

int next_alphanumerical(char *str, int b)
{
    int a_ = b;

    while (str[a_] != '\0' && my_is_alphanumerical(str, a_) == 0) {
        a_ = a_ + 1;
    }
    return a_;
}

char *my_create_str(char *str, int a, int b)
{
    char *string;
    int i = 0;
    int a_ = a;

    string = malloc((unsigned long)b - (unsigned long)a + 1);
    if (string == NULL)
        return NULL;
    while (a_ < b) {
        string[i] = str[a_];
        a_ = a_ + 1;
        i = i + 1;
    }
    string[i] = '\0';
    return string;
}

char **my_str_to_word_array(char *str)
{
    char **tab;
    int j = 0;
    int a = next_alphanumerical(str, 0);
    int len = strlen(str);

    tab = malloc(sizeof(char *) * len);
    for (int i = a; i < len && tab != NULL; i++) {
        if (my_is_alphanumerical(str, i) == 0 || str[i] == '\0') {
            tab[j] = my_create_str(str, a, i);
            j = j + 1;
            a = next_alphanumerical(str, i);
            i = a;
        }
    }
    if (a < len) {
        tab[j] = my_create_str(str, a, len);
        j = j + 1;
    }
    tab[j] = NULL;
    return tab;
}

void free_arr(char **ar)
{
    int i = 0;

    while (ar[i] != NULL) {
        free(ar[i]);
        i += 1;
    }
    free(ar);
}
