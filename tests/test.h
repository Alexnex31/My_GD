#ifndef TESTS_TEST_H
    #define TESTS_TEST_H
    #include <stdio.h>

extern int failures;

    #define CHECK(cond) do { if (!(cond)) { \
        fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
        failures++; } } while (0)

void test_constants(void);
void test_hitbox(void);
void test_sweep(void);
void test_circle(void);
void test_parser(void);
void test_tick(void);

#endif