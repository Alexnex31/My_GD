/*
** ALEXNEX PROJECT, 2026
** sim/input_ticks.c
** File description:
** each press belongs to the 240 Hz tick it happened in (FEATURES 1)
*/

#include "sim/constants.h"
#include "sim/input_ticks.h"

bool input_queue_push(input_queue_t *q, input_change_t c)
{
    unsigned int head = atomic_load_explicit(&q->head, memory_order_relaxed);
    unsigned int tail = atomic_load_explicit(&q->tail, memory_order_acquire);

    if (head - tail >= INPUT_QUEUE_SIZE)
        return false;
    q->items[head % INPUT_QUEUE_SIZE] = c;
    atomic_store_explicit(&q->head, head + 1, memory_order_release);
    return true;
}

/* After the pushes: every change up to `polled_us` is now visible. */
void input_queue_publish(input_queue_t *q, int64_t polled_us)
{
    atomic_store_explicit(&q->polled_us, polled_us, memory_order_release);
}

/* The oldest change not read yet, if it happened before `end`. */
static bool next_change(input_queue_t *q, int64_t end, input_change_t *out)
{
    unsigned int tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    unsigned int head = atomic_load_explicit(&q->head, memory_order_acquire);

    if (tail == head)
        return false;
    *out = q->items[tail % INPUT_QUEUE_SIZE];
    if (out->at_us * TICK_RATE >= end)
        return false;                        /* a later tick's */
    atomic_store_explicit(&q->tail, tail + 1, memory_order_release);
    return true;
}

input_t input_read_tick(input_reader_t *r, input_queue_t *q,
    int64_t start, int64_t end)
{
    input_change_t c;
    bool pressed = false;

    while (next_change(q, end, &c)) {
        if (c.at_us * TICK_RATE >= start && (c.down & ~r->down & ~r->latched))
            pressed = true;
        r->down = c.down;
        r->latched &= c.down;                /* released once: a jump from now on */
    }
    return (input_t){(r->down & ~r->latched) != 0, pressed};
}
