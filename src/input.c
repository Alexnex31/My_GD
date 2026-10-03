/*
** ALEXNEX PROJECT, 2026
** input
** File description:
** the jump input, polled by its own thread and stamped (3.6, FEATURES 1)
*/

#include <X11/Xlib.h>
#include <sched.h>
#include <time.h>

#include "mygd.h"

#define POLL_US 1000                 /* a stamp is at most this late */

/* The jump inputs: bit i of a change is JUMP_INPUTS[i]. */
typedef struct jump_input {
    bool mouse;
    int code;                        /* sfKeyCode or sfMouseButton */
} jump_input_t;

static const jump_input_t JUMP_INPUTS[] = {
    {false, sfKeySpace},
    {false, sfKeyUp},
    {true, sfMouseLeft},
};

#define NB_JUMP_INPUTS (int)(sizeof(JUMP_INPUTS) / sizeof(JUMP_INPUTS[0]))

/*
** SFML polls through the X display the window uses, and never makes Xlib
** thread safe: the polling thread would race the window. Before any X call.
*/
void input_init_threads(void)
{
    XInitThreads();
}

int64_t input_clock_us(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static unsigned int buttons_down(void)
{
    unsigned int down = 0;

    for (int i = 0; i < NB_JUMP_INPUTS; i++) {
        const jump_input_t *j = &JUMP_INPUTS[i];

        if (j->mouse ? sfMouse_isButtonPressed(j->code)
            : sfKeyboard_isKeyPressed(j->code))
            down |= 1u << i;
    }
    return down;
}

static unsigned int mouse_inputs(void)
{
    unsigned int mask = 0;

    for (int i = 0; i < NB_JUMP_INPUTS; i++)
        if (JUMP_INPUTS[i].mouse)
            mask |= 1u << i;
    return mask;
}

static void sleep_until(struct timespec *t)
{
    t->tv_nsec += POLL_US * 1000;
    if (t->tv_nsec >= 1000000000) {
        t->tv_nsec -= 1000000000;
        t->tv_sec += 1;
    }
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, t, NULL);
}

/*
** The stamp is taken before the poll: a change the poll sees happened before
** it was published, so a tick that runs up to `polled_us` has all of its own.
*/
static void *poll_loop(void *arg)
{
    input_poll_t *ip = arg;
    struct timespec t;
    unsigned int queued = ~0u;               /* forces the first push */

    clock_gettime(CLOCK_MONOTONIC, &t);
    while (atomic_load(&ip->running)) {
        int64_t now = input_clock_us();
        unsigned int down = buttons_down();

        if (down != queued
            && input_queue_push(&ip->queue, (input_change_t){now, down}))
            queued = down;
        input_queue_publish(&ip->queue, now);
        sleep_until(&t);
    }
    return NULL;
}

/*
** A level starts from a click (Play, Retry), usually still down when its
** first tick runs: held, it would be a jump at tick 0 (3.4's carried hold).
** The mouse is ignored until it's released; the keyboard never is.
*/
void input_start(gd_t *gd)
{
    input_poll_t *ip = &gd->input;

    atomic_store(&ip->queue.head, 0);
    atomic_store(&ip->queue.tail, 0);
    atomic_store(&ip->queue.polled_us, 0);
    ip->reader = (input_reader_t){0, mouse_inputs()};
    atomic_store(&ip->running, true);
    if (pthread_create(&ip->thread, NULL, poll_loop, ip) != 0) {
        dprintf(2, "my_gd: cannot start the input thread\n");
        exit(84);
    }
    while (atomic_load(&ip->queue.polled_us) == 0)
        sched_yield();                       /* the first poll: under a ms */
}

void input_stop(gd_t *gd)
{
    atomic_store(&gd->input.running, false);
    pthread_join(gd->input.thread, NULL);
}

/* The time up to which ticks may run: the input is known until then. */
int64_t input_now_us(gd_t *gd)
{
    return atomic_load(&gd->input.queue.polled_us);
}

/* The tick covering [start, end), in microseconds x TICK_RATE (3.6). */
input_t input_for_tick(gd_t *gd, int64_t start, int64_t end)
{
    return input_read_tick(&gd->input.reader, &gd->input.queue, start, end);
}
