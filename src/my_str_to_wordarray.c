/*
** ALEXNEX PROJECT, 2026
** my_str_to_word_array
** File description:
** split a string into words in an array
*/
#include <stdlib.h>
#include "mygd.h"

static int my_strlen(char *str)
{
    int i = 0;

    while (str[i] != '\0')
        i += 1;
    return i;
}

static int my_is_alphanumerical(char *str, int i)
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

static int next_alphanumerical(char *str, int b)
{
    int a_ = b;

    while (str[a_] != '\0' && my_is_alphanumerical(str, a_) == 0) {
        a_ = a_ + 1;
    }
    return a_;
}

static char *my_create_str(char *str, int a, int b)
{
    char *string = NULL;
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
    char **tab = NULL;
    int j = 0;
    int a = next_alphanumerical(str, 0);

    tab = malloc(sizeof(char *) * (unsigned long)my_strlen(str));
    for (int i = a; i < my_strlen(str) && tab != NULL; i++) {
        if (my_is_alphanumerical(str, i) == 0 || str[i] == '\0') {
            tab[j] = my_create_str(str, a, i);
            j = j + 1;
            a = next_alphanumerical(str, i);
            i = a;
        }
    }
    if (a < my_strlen(str)) {
        tab[j] = my_create_str(str, a, my_strlen(str));
        j = j + 1;
    }
    tab[j] = NULL;
    return tab;
}

static int my_is_alphanumerical_delim(char *str, int i, char *delim)
{
    int j = 0;

    if (delim == NULL)
        return my_is_alphanumerical(str, i);
    while (delim[j] != '\0') {
        if (str[i] == delim[j])
            return 0;
        j += 1;
    }
    return 1;
}

static int next_alphanumerical_delim(char *str, int b, char *delim)
{
    int a_ = b;

    while (str[a_] != '\0' && my_is_alphanumerical_delim(str, a_, delim) == 0) {
        a_ = a_ + 1;
    }
    return a_;
}

char **my_str_word_array_delim(char *str, char *delim)
{
    char **tab = NULL;
    int j = 0;
    int a = next_alphanumerical_delim(str, 0, delim);

    tab = malloc(sizeof(char *) * (unsigned long)my_strlen(str));
    for (int i = a; i < my_strlen(str) && tab != NULL; i++) {
        if (my_is_alphanumerical_delim(str, i, delim) == 0 || str[i] == '\0') {
            tab[j] = my_create_str(str, a, i);
            j = j + 1;
            a = next_alphanumerical_delim(str, i, delim);
            i = a;
        }
    }
    if (a < my_strlen(str)) {
        tab[j] = my_create_str(str, a, my_strlen(str));
        j = j + 1;
    }
    tab[j] = NULL;
    return tab;
}

void free_arr(char **ar)
{
    int i = 0;

    if (ar == NULL)
        return;
    while (ar[i] != NULL) {
        free(ar[i]);
        i += 1;
    }
    free(ar);
}
