/*
** ALEXNEX PROJECT, 2026
** tests/test_edmusic.c
** File description:
** the song along the level, beat lines, the level's song settings (FEATURES 11.11)
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor/ed_music.h"
#include "sim/constants.h"
#include "test.h"

static song_t SONGS[2] = {
    {.file = "a.ogg", .title = "Alpha", .default_offset = 0.5f},
    {.file = "b.ogg", .title = "Beta"},
};
static const library_t LIB = {SONGS, 2};

static bool text_is(const ed_level_t *lv, ed_song_prop_t p, const char *want)
{
    char text[64];

    ed_song_prop_text(lv, &LIB, p, text, sizeof(text));
    return strcmp(text, want) == 0;
}

/* A time in the song is an x, and back; the offset is the game's own. */
static void test_time_and_x(void)
{
    ed_level_t lv;

    ed_level_new(&lv, "1");
    CHECK(ed_song_offset(&lv, &LIB) == 0.0);           /* no song */
    CHECK(ed_time_at_x(&lv, &LIB, PLAYER_SPAWN_X) == 0.0);
    CHECK(fabs(ed_time_at_x(&lv, &LIB, PLAYER_SPAWN_X + 3.0 * SCROLL_SPEED)
        - 3.0) < 1e-9);
    snprintf(lv.hdr.music, sizeof(lv.hdr.music), "a.ogg");
    CHECK(ed_song_offset(&lv, &LIB) == 0.5);           /* the song's own */
    lv.hdr.music_offset = 2.0;
    CHECK(ed_song_offset(&lv, &LIB) == 2.0);           /* the level's */
    CHECK(ed_time_at_x(&lv, &LIB, PLAYER_SPAWN_X) == 2.0);
    CHECK(fabs(ed_x_at_time(&lv, &LIB, 5.0) - (PLAYER_SPAWN_X
        + 3.0 * SCROLL_SPEED)) < 1e-9);
    lv.hdr.start.speed_mult = 2.0;                     /* twice as far a second */
    lv.hdr.start.pos.x = 1000.0;
    CHECK(fabs(ed_x_at_time(&lv, &LIB, 3.0) - (1000.0 + 2.0 * SCROLL_SPEED))
        < 1e-9);
    CHECK(fabs(ed_time_at_x(&lv, &LIB, ed_x_at_time(&lv, &LIB, 7.25)) - 7.25)
        < 1e-9);
    ed_level_free(&lv);
}

/* Beat k is at first_beat + k * 60 / bpm of the song (11.11). */
static void test_beats(void)
{
    double gap = SCROLL_SPEED * 60.0 / 120.0;
    ed_level_t lv;
    long first = 0;
    long last = 0;

    ed_level_new(&lv, "1");
    CHECK(!ed_beats_between(&lv, &LIB, 0.0, 5000.0, &first, &last));
    lv.hdr.bpm = 120.0;
    CHECK(fabs(ed_beat_x(&lv, &LIB, 0) - PLAYER_SPAWN_X) < 1e-9);
    CHECK(fabs(ed_beat_x(&lv, &LIB, 4) - (PLAYER_SPAWN_X + 4.0 * gap)) < 1e-9);
    CHECK(fabs(gap - 519.3) < 0.1);                    /* about 5 blocks */
    CHECK(ed_beats_between(&lv, &LIB, PLAYER_SPAWN_X, PLAYER_SPAWN_X
        + 3.0 * gap, &first, &last));
    CHECK(first == 0 && last == 3);                    /* both ends count */
    CHECK(ed_beats_between(&lv, &LIB, PLAYER_SPAWN_X + 1.0, PLAYER_SPAWN_X
        + 3.0 * gap - 1.0, &first, &last));
    CHECK(first == 1 && last == 2);
    CHECK(!ed_beats_between(&lv, &LIB, PLAYER_SPAWN_X + 10.0, PLAYER_SPAWN_X
        + 100.0, &first, &last));                      /* between two beats */
    lv.hdr.first_beat = 0.25;                          /* half a beat later */
    CHECK(fabs(ed_beat_x(&lv, &LIB, 0) - (PLAYER_SPAWN_X + gap / 2.0)) < 1e-9);
    lv.hdr.music_offset = 1.0;                         /* song ahead: beats earlier */
    snprintf(lv.hdr.music, sizeof(lv.hdr.music), "b.ogg");
    CHECK(fabs(ed_beat_x(&lv, &LIB, 2) - (PLAYER_SPAWN_X + gap / 2.0)) < 1e-9);
    CHECK(ed_beats_between(&lv, &LIB, PLAYER_SPAWN_X, PLAYER_SPAWN_X + gap,
        &first, &last) && first == 2 && last == 2);
    ed_level_free(&lv);
}

static void test_song_settings(void)
{
    ed_level_t lv;
    char *text;

    ed_level_new(&lv, "1");
    CHECK(text_is(&lv, ED_SONG_FILE, "none") && !lv.dirty);
    CHECK(!ed_song_prop_step(&lv, &LIB, ED_SONG_BPM, -1, false) && !lv.dirty);
    CHECK(ed_song_prop_step(&lv, &LIB, ED_SONG_FILE, 1, false) && lv.dirty);
    CHECK(text_is(&lv, ED_SONG_FILE, "Alpha"));
    CHECK(ed_song_prop_step(&lv, &LIB, ED_SONG_FILE, 1, false));
    CHECK(strcmp(lv.hdr.music, "b.ogg") == 0);
    CHECK(ed_song_prop_step(&lv, &LIB, ED_SONG_FILE, 1, false));
    CHECK(text_is(&lv, ED_SONG_FILE, "none") && lv.hdr.music[0] == '\0');
    CHECK(ed_song_prop_step(&lv, &LIB, ED_SONG_FILE, -1, false));
    CHECK(text_is(&lv, ED_SONG_FILE, "Beta"));
    for (int i = 0; i < 40; i++)
        ed_song_prop_step(&lv, &LIB, ED_SONG_OFFSET, 1, false);
    CHECK(lv.hdr.music_offset == 2.0 && text_is(&lv, ED_SONG_OFFSET, "2.00 s"));
    CHECK(ed_song_prop_step(&lv, &LIB, ED_SONG_OFFSET, -1, true));
    CHECK(ed_song_prop_step(&lv, &LIB, ED_SONG_OFFSET, -1, true));
    CHECK(ed_song_prop_step(&lv, &LIB, ED_SONG_OFFSET, -1, true) == false);
    CHECK(lv.hdr.music_offset == 0.0);
    for (int i = 0; i < 12; i++)
        ed_song_prop_step(&lv, &LIB, ED_SONG_BPM, 1, true);
    ed_song_prop_step(&lv, &LIB, ED_SONG_BPM, 1, false);
    for (int i = 0; i < 35; i++)
        ed_song_prop_step(&lv, &LIB, ED_SONG_FIRST_BEAT, 1, false);
    CHECK(lv.hdr.bpm == 121.0 && lv.hdr.first_beat == 0.35);
    CHECK(text_is(&lv, ED_SONG_BPM, "121"));
    CHECK(text_is(&lv, ED_SONG_FIRST_BEAT, "0.35 s"));
    text = ed_level_text(&lv);
    CHECK(strcmp(text, "name LEVEL 1\nmusic b.ogg\nbpm 121\n"
        "first_beat 0.35\n") == 0);
    free(text);
    snprintf(lv.hdr.music, sizeof(lv.hdr.music), "gone.ogg");
    CHECK(text_is(&lv, ED_SONG_FILE, "missing"));
    CHECK(ed_song_prop_step(&lv, &LIB, ED_SONG_FILE, 1, false));
    CHECK(text_is(&lv, ED_SONG_FILE, "Alpha"));
    ed_level_free(&lv);
}

void test_edmusic(void)
{
    test_time_and_x();
    test_beats();
    test_song_settings();
}
