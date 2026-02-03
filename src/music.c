/*
** ALEXNEX PROJECT, 2026
** music
** File description:
** functions to play music and sounds
*/

#include "mygd.h"

back_mus_t *play_background_music(char *filepath)
{
    sfSoundBuffer *music = sfSoundBuffer_createFromFile(filepath);
    sfSound *m = sfSound_create();
    back_mus_t *back = malloc(sizeof(back_mus_t));

    if (music == NULL || m == NULL)
        return NULL;
    sfSound_setBuffer(m, music);
    sfSound_setLoop(m, sfTrue);
    sfSound_setVolume(m, 20);
    sfSound_play(m);
    back->sbuf = music;
    back->sound = m;
    return back;
}

void free_music_back(back_mus_t *m)
{
    if (m->sbuf == NULL || m->sound == NULL) {
        return;
    }
    sfSound_stop(m->sound);
    sfSound_destroy(m->sound);
    sfSoundBuffer_destroy(m->sbuf);
}

sound_t play_sound(char *filepath)
{
    sound_t sound_buffer;
    sfSoundBuffer *music = sfSoundBuffer_createFromFile(filepath);
    sfSound *m = sfSound_create();

    if (music == NULL || m == NULL)
        return sound_buffer;
    sfSound_setBuffer(m, music);
    sfSound_setVolume(m, 20);
    sound_buffer.s = m;
    sound_buffer.sb = music;
    return sound_buffer;
}
