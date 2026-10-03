/*
** ALEXNEX PROJECT, 2026
** sim/input_ticks.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_INPUT_TICKS_H
    #define SIM_INPUT_TICKS_H

    #include <stdatomic.h>
    #include <stdint.h>

    #include "sim/sim_types.h"

    #define INPUT_QUEUE_SIZE 256      /* a power of two */

/*
** The jump buttons at one instant (FEATURES 1): bit i is jump input i, as the
** game layer numbers them. Only changes are queued.
*/
typedef struct input_change {
    int64_t at_us;                /* when the poll saw it, monotonic clock     */
    unsigned int down;
} input_change_t;

/*
** One writer (the polling thread), one reader (the ticks), no lock. `polled_us`
** is the time up to which every change is in the queue: the ticks never run
** past it, so a change can't arrive after its tick has run.
*/
typedef struct input_queue {
    input_change_t items[INPUT_QUEUE_SIZE];
    atomic_uint head;             /* next slot to write: the writer's          */
    atomic_uint tail;             /* next slot to read: the reader's           */
    atomic_int_least64_t polled_us;
} input_queue_t;

typedef struct input_reader {
    unsigned int down;            /* the buttons after the last change read    */
    unsigned int latched;         /* down since the level started: not a jump
                                     until released (PLAN 3.6)                 */
} input_reader_t;

/* The writer's side: false when the queue is full (push the change again). */
bool input_queue_push(input_queue_t *q, input_change_t c);
void input_queue_publish(input_queue_t *q, int64_t polled_us);

/*
** The input of the tick that covers [start, end), in microseconds x TICK_RATE
** (3.6). Pressed only if a button went down inside it: a change before it
** updates the hold but presses nothing, and two presses in it are one.
*/
input_t input_read_tick(input_reader_t *r, input_queue_t *q,
    int64_t start, int64_t end);

#endif
