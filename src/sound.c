/*
** ALEXNEX PROJECT, 2026
** sound
** File description:
** the sound effects: the click, at the effects volume (FEATURES 5.2)
*/

#include "mygd.h"

/* Made once from music/sfx.c's samples: no file to ship (PLAN 12.2). */
void sound_create(gd_t *gd)
{
    static int16_t samples[SFX_CLICK_SAMPLES];
    size_t n = sfx_click(samples, SFX_CLICK_SAMPLES);

    gd->click_buffer = sfSoundBuffer_createFromSamples(samples, n, 1,
        SFX_RATE);
    gd->click = sfSound_create();
    if (gd->click_buffer != NULL)
        sfSound_setBuffer(gd->click, gd->click_buffer);
    sound_set_volume(gd, gd->settings.sfx_volume);
}

void sound_free(gd_t *gd)
{
    sfSound_destroy(gd->click);
    if (gd->click_buffer != NULL)
        sfSoundBuffer_destroy(gd->click_buffer);
}

/* The music's curve: the slider's middle sounds like half (4.7). */
void sound_set_volume(gd_t *gd, int volume)
{
    sfSound_setVolume(gd->click, music_volume_curve(volume));
}

void sound_click(gd_t *gd)
{
    if (gd->click_buffer != NULL)
        sfSound_play(gd->click);
}
