/*
** ALEXNEX PROJECT, 2026
** music/sfx.h
** File description:
** header file for my_gd project
*/

#ifndef MUSIC_SFX_H
    #define MUSIC_SFX_H

    #include <stdbool.h>
    #include <stddef.h>
    #include <stdint.h>

    #define SFX_RATE 44100
    #define SFX_CLICK_MS 30
    #define SFX_CLICK_SAMPLES (SFX_RATE * SFX_CLICK_MS / 1000)

/*
** The click (FEATURES 5.2): made here, not loaded, so no file of anyone's
** is shipped for it (PLAN 12.2). A short high tone that dies at once, with
** a sharp start: where it begins is what the ear places in time.
*/
size_t sfx_click(int16_t *out, size_t max);

    #define CALIB_PERIOD_MS 500   /* 120 beats a minute                    */
    #define CALIB_LEAD_MS 600     /* the first beat, after the dialog opens */
    #define CALIB_FLASH_MS 150    /* the square, lit at a beat, fades       */

/*
** The audio offset's calibration (FEATURES 5.2, 4.8): a square flashes on
** each beat, and each beat's click is played `offset` ms before it, as a
** level's song is: when the offset is the output's latency, both are
** perceived together. Moving the offset while it runs moves the next clicks.
*/
typedef struct calib {
    int64_t start_ms;
    long next;                    /* the beat whose click is the next one   */
} calib_t;

void calib_begin(calib_t *c, int64_t now_ms);
/* The click to play now, if any: once per beat, never a burst of them. */
bool calib_click_due(calib_t *c, int64_t now_ms, int offset_ms);
/* How lit the square is, 1 on a beat to 0 CALIB_FLASH_MS later. */
float calib_flash(const calib_t *c, int64_t now_ms);

#endif
