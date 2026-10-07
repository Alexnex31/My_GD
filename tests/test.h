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
void test_slopes(void);
void test_portal(void);
void test_start(void);
void test_gravity(void);
void test_ufo(void);
void test_wave(void);
void test_trail(void);
void test_ball(void);
void test_progress(void);
void test_input(void);
void test_settings(void);
void test_ui(void);
void test_music(void);
void test_options(void);
void test_bot(void);

#endif