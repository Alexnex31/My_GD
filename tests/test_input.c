/*
** ALEXNEX PROJECT, 2026
** tests/test_input.c
** File description:
** each press belongs to the tick it happened in (FEATURES 1.6)
*/

#include <stdlib.h>

#include "test.h"
#include "sim/constants.h"
#include "sim/input_ticks.h"

#define SPACE 1u
#define UP 2u
#define MOUSE 4u

/* A microsecond `off` into tick k (a tick is 4166.67 us). */
static int64_t in_tick(int k, int off)
{
    return ((int64_t)k * 1000000 + TICK_RATE - 1) / TICK_RATE + off;
}

static input_queue_t *new_queue(void)
{
    return calloc(1, sizeof(input_queue_t));
}

static void push(input_queue_t *q, int64_t at, unsigned int down)
{
    CHECK(input_queue_push(q, (input_change_t){at, down}));
}

static input_t tick(input_reader_t *r, input_queue_t *q, int k)
{
    return input_read_tick(r, q, (int64_t)k * 1000000,
        (int64_t)(k + 1) * 1000000);
}

static void test_press_counts_in_its_tick_only(void)
{
    input_queue_t *q = new_queue();
    input_reader_t r = {0};
    input_t in[4];

    push(q, in_tick(1, 2000), SPACE);
    for (int k = 0; k < 4; k++)
        in[k] = tick(&r, q, k);
    CHECK(!in[0].pressed && !in[0].held);
    CHECK(in[1].pressed && in[1].held);
    CHECK(!in[2].pressed && in[2].held);
    CHECK(!in[3].pressed && in[3].held);
    free(q);
}

static void test_two_clicks_in_one_tick_are_one(void)
{
    input_queue_t *q = new_queue();
    input_reader_t r = {0};

    push(q, in_tick(0, 100), MOUSE);
    push(q, in_tick(0, 1100), 0);
    push(q, in_tick(0, 2100), MOUSE);
    push(q, in_tick(0, 3100), 0);
    CHECK(tick(&r, q, 0).pressed);
    CHECK(!tick(&r, q, 1).pressed);                  /* nothing carried over */
    CHECK(!tick(&r, q, 2).pressed);
    free(q);
}

static void test_tap_inside_one_tick(void)
{
    input_queue_t *q = new_queue();
    input_reader_t r = {0};
    input_t in;

    push(q, in_tick(3, 1000), SPACE);
    push(q, in_tick(3, 3000), 0);
    for (int k = 0; k < 3; k++)
        CHECK(!tick(&r, q, k).pressed);
    in = tick(&r, q, 3);
    CHECK(in.pressed && !in.held);
    in = tick(&r, q, 4);
    CHECK(!in.pressed && !in.held);
    free(q);
}

/* Two clicks in two ticks in a row are two presses. */
static void test_clicks_in_next_ticks(void)
{
    input_queue_t *q = new_queue();
    input_reader_t r = {0};

    push(q, in_tick(0, 3000), SPACE);
    push(q, in_tick(0, 4000), 0);
    push(q, in_tick(1, 500), SPACE);
    CHECK(tick(&r, q, 0).pressed);
    CHECK(tick(&r, q, 1).pressed);
    CHECK(!tick(&r, q, 2).pressed);
    free(q);
}

/* Polled, a key held for 2 s is one change, whatever the OS repeats. */
static void test_held_is_one_press(void)
{
    input_queue_t *q = new_queue();
    input_reader_t r = {0};
    int presses = 0;
    int held = 0;

    push(q, in_tick(0, 10), SPACE);
    for (int k = 0; k < 2 * TICK_RATE; k++) {
        input_t in = tick(&r, q, k);

        presses += in.pressed;
        held += in.held;
    }
    CHECK(presses == 1);
    CHECK(held == 2 * TICK_RATE);
    free(q);
}

/*
** The ticks don't care how frames group them: reading with every change
** queued up front gives what reading with them queued just in time gives.
*/
static void test_frames_dont_matter(void)
{
    static const struct { int k; int off; unsigned int down; } changes[] = {
        {0, 4000, SPACE}, {1, 100, 0}, {1, 200, SPACE}, {5, 0, SPACE | MOUSE},
        {5, 4100, MOUSE}, {9, 2000, 0}, {9, 2500, UP}, {9, 3000, 0},
    };
    int n = sizeof(changes) / sizeof(changes[0]);
    input_queue_t *early = new_queue();
    input_queue_t *late = new_queue();
    input_reader_t a = {0};
    input_reader_t b = {0};
    int next = 0;

    for (int i = 0; i < n; i++)
        push(early, in_tick(changes[i].k, changes[i].off), changes[i].down);
    for (int k = 0; k < 12; k++) {
        input_t x = tick(&a, early, k);
        input_t y;

        while (next < n && changes[next].k == k) {
            push(late, in_tick(k, changes[next].off), changes[next].down);
            next += 1;
        }
        y = tick(&b, late, k);
        CHECK(x.held == y.held && x.pressed == y.pressed);
    }
    free(early);
    free(late);
}

/* A change exactly on a tick's end is the next tick's. */
static void test_boundary(void)
{
    input_queue_t *q = new_queue();
    input_reader_t r = {0};

    push(q, 25000, SPACE);                    /* 25 ms: the end of tick 5 */
    CHECK(!tick(&r, q, 5).pressed);
    CHECK(tick(&r, q, 6).pressed);
    free(q);
}

/* Before a tick's window (a pause, a hitch): the hold counts, not the press. */
static void test_press_before_the_window(void)
{
    input_queue_t *q = new_queue();
    input_reader_t r = {0};
    input_t in;

    push(q, in_tick(2, 100), SPACE);
    in = tick(&r, q, 40);
    CHECK(!in.pressed && in.held);
    free(q);
}

/* The click that started the level: not held, not pressed, until it's up. */
static void test_latched_click(void)
{
    input_queue_t *q = new_queue();
    input_reader_t r = {0, MOUSE};
    input_t in;

    push(q, in_tick(0, 10), MOUSE);
    in = tick(&r, q, 0);
    CHECK(!in.pressed && !in.held);
    push(q, in_tick(1, 10), MOUSE | SPACE);
    in = tick(&r, q, 1);
    CHECK(in.pressed && in.held);             /* the keyboard is never ignored */
    push(q, in_tick(2, 10), SPACE);
    push(q, in_tick(3, 10), 0);
    tick(&r, q, 2);
    CHECK(!tick(&r, q, 3).held);
    push(q, in_tick(4, 10), MOUSE);
    in = tick(&r, q, 4);
    CHECK(in.pressed && in.held);
    free(q);
}

static void test_other_button_while_held(void)
{
    input_queue_t *q = new_queue();
    input_reader_t r = {0};

    push(q, in_tick(0, 10), SPACE);
    push(q, in_tick(3, 10), SPACE | MOUSE);
    push(q, in_tick(4, 10), SPACE);              /* a release isn't a press */
    CHECK(tick(&r, q, 0).pressed);
    CHECK(!tick(&r, q, 1).pressed);
    CHECK(!tick(&r, q, 2).pressed);
    CHECK(tick(&r, q, 3).pressed);
    CHECK(!tick(&r, q, 4).pressed);
    free(q);
}

static void test_full_queue(void)
{
    input_queue_t *q = new_queue();
    input_reader_t r = {0};

    for (int i = 0; i < INPUT_QUEUE_SIZE; i++)
        push(q, in_tick(0, i), (unsigned int)i & SPACE);
    CHECK(!input_queue_push(q, (input_change_t){in_tick(0, 999), MOUSE}));
    tick(&r, q, 0);
    CHECK(input_queue_push(q, (input_change_t){in_tick(1, 0), MOUSE}));
    free(q);
}

void test_input(void)
{
    test_press_counts_in_its_tick_only();
    test_two_clicks_in_one_tick_are_one();
    test_tap_inside_one_tick();
    test_clicks_in_next_ticks();
    test_held_is_one_press();
    test_frames_dont_matter();
    test_boundary();
    test_press_before_the_window();
    test_latched_click();
    test_other_button_while_held();
    test_full_queue();
}
