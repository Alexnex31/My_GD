/*
** ALEXNEX PROJECT, 2026
** music/sfx.c
** File description:
** the click, made from numbers, and the calibration's beats (FEATURES 5.2)
*/

#include <math.h>

#include "music/sfx.h"

/* 2 kHz, gone in a few milliseconds: a tick, not a beep. */
size_t sfx_click(int16_t *out, size_t max)
{
    size_t n = max < SFX_CLICK_SAMPLES ? max : SFX_CLICK_SAMPLES;

    for (size_t i = 0; i < n; i++) {
        double t = (double)i / SFX_RATE;
        double v = sin(2.0 * M_PI * 2000.0 * t) * exp(-t / 0.004);

        out[i] = (int16_t)lrint(v * 28000.0);
    }
    return n;
}

static int64_t beat_at(const calib_t *c, long k)
{
    return c->start_ms + CALIB_LEAD_MS + (int64_t)k * CALIB_PERIOD_MS;
}

void calib_begin(calib_t *c, int64_t now_ms)
{
    *c = (calib_t){now_ms, 0};
}

/*
** A frame late, or the offset moved so far that several clicks are past
** due: only the last is played, so a hitch is no burst of clicks.
*/
bool calib_click_due(calib_t *c, int64_t now_ms, int offset_ms)
{
    bool due = false;

    while (now_ms >= beat_at(c, c->next) - offset_ms) {
        c->next += 1;
        due = true;
    }
    return due;
}

float calib_flash(const calib_t *c, int64_t now_ms)
{
    int64_t since = now_ms - c->start_ms - CALIB_LEAD_MS;
    int64_t into = since % CALIB_PERIOD_MS;

    if (since < 0 || into >= CALIB_FLASH_MS)
        return 0.0f;
    return 1.0f - (float)into / (float)CALIB_FLASH_MS;
}
