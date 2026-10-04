/*
** ALEXNEX PROJECT, 2026
** tests/test_music.c
** File description:
** the song library, which song plays and from where, the sync rule (FEATURES 4)
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "music/library.h"
#include "sim/constants.h"
#include "test.h"

/* A folder of its own under /tmp, with files the fake decoder judges. */
static char tmp_dir[] = "/tmp/my_gd_musicXXXXXX";
static char logged[4096];
static int nb_logged;

static void capture(const char *msg)
{
    size_t used = strlen(logged);

    snprintf(logged + used, sizeof(logged) - used, "%s\n", msg);
    nb_logged += 1;
}

/* "broken" in the name doesn't decode; every other song lasts 100 s. */
static bool fake_probe(const char *path, float *duration)
{
    if (strstr(path, "broken") != NULL)
        return false;
    *duration = 100.0f;
    return true;
}

static void touch(const char *name, const char *text)
{
    char path[256];
    FILE *f = NULL;

    snprintf(path, sizeof(path), "%s/%s", tmp_dir, name);
    f = fopen(path, "w");
    if (f == NULL)
        return;
    fputs(text, f);
    fclose(f);
}

static void scan(library_t *lib)
{
    logged[0] = '\0';
    nb_logged = 0;
    library_scan(lib, tmp_dir, fake_probe, capture);
}

static void test_scan(void)
{
    static const char *const files[] = {"zeta.ogg", "Alpha.mp3", "b.flac",
        "c.wav", "broken.ogg", ".hidden.ogg", "notes.txt", "has space.ogg"};
    library_t lib;

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
        touch(files[i], "");
    scan(&lib);
    CHECK(lib.count == 4);
    CHECK(strcmp(lib.songs[0].file, "Alpha.mp3") == 0);  /* by title, any case */
    CHECK(strcmp(lib.songs[1].title, "b") == 0);
    CHECK(strcmp(lib.songs[3].file, "zeta.ogg") == 0);
    CHECK(strcmp(lib.songs[0].artist, "Unknown") == 0);
    CHECK(strcmp(lib.songs[0].license, "?") == 0);
    CHECK(lib.songs[0].duration == 100.0f);
    CHECK(nb_logged == 2);                          /* broken, has space */
    CHECK(strstr(logged, "can't decode") != NULL);
    CHECK(library_find(&lib, "broken.ogg") == NULL);
    library_free(&lib);
}

/* songs.txt: trimmed fields, defaults for what's missing (4.3). */
static void test_meta(void)
{
    library_t lib;
    const song_t *s = NULL;

    touch(SONGS_META, "# file | title | artist | license | offset\n\n"
        "zeta.ogg |  The Last  | Some One | CC BY 4.0 | 1.25\n"
        "b.flac | Bee\n"
        "c.wav | | | CC0 | 250\n"
        "Alpha.mp3 | Alpha | A | CC0 | -1\n"
        "gone.ogg | Gone\n");
    scan(&lib);
    s = library_find(&lib, "zeta.ogg");
    CHECK(s != NULL && strcmp(s->title, "The Last") == 0);
    CHECK(s != NULL && strcmp(s->artist, "Some One") == 0);
    CHECK(s != NULL && strcmp(s->license, "CC BY 4.0") == 0);
    CHECK(s != NULL && fabsf(s->default_offset - 1.25f) < 1e-6f);
    s = library_find(&lib, "b.flac");
    CHECK(s != NULL && strcmp(s->title, "Bee") == 0
        && strcmp(s->artist, "Unknown") == 0);
    s = library_find(&lib, "c.wav");
    CHECK(s != NULL && strcmp(s->title, "c") == 0
        && strcmp(s->license, "CC0") == 0);
    CHECK(s != NULL && s->default_offset == 0.0f);     /* past its 100 s */
    CHECK(library_find(&lib, "Alpha.mp3")->default_offset == 0.0f);
    CHECK(nb_logged == 5);              /* 2 files, 2 offsets, gone.ogg */
    CHECK(strstr(logged, "songs.txt:7: gone.ogg") != NULL);
    CHECK(strstr(logged, "songs.txt:5: offset 250") != NULL);
    CHECK(strcmp(lib.songs[lib.count - 1].title, "The Last") == 0);
    library_free(&lib);
}

static void test_missing_folder(void)
{
    library_t lib;

    logged[0] = '\0';
    nb_logged = 0;
    library_scan(&lib, "/tmp/my_gd_no_such_folder", fake_probe, capture);
    CHECK(lib.count == 0 && nb_logged == 1);
    CHECK(music_menu_song(&lib, "x.ogg") == NULL);
    library_free(&lib);
}

static void test_file_names(void)
{
    CHECK(song_file_name_ok("a.ogg") && song_file_name_ok("A_b-2.MP3"));
    CHECK(!song_file_name_ok("a b.ogg") && !song_file_name_ok("a#b.ogg"));
    CHECK(!song_file_name_ok("a|b.ogg") && !song_file_name_ok("a=b.ogg"));
    CHECK(!song_file_name_ok("../a.ogg") && !song_file_name_ok("d/a.ogg"));
    CHECK(!song_file_name_ok(".a.ogg") && !song_file_name_ok("a.txt"));
    CHECK(!song_file_name_ok("") && !song_file_name_ok("a"));
}

/* Override, the level's song, the default; offsets go with them (4.4). */
static void test_choice(void)
{
    song_t songs[] = {
        {"back_mus.ogg", "Back", "", "", 2.0f, 100.0f},
        {"level.ogg", "Level", "", "", 3.0f, 100.0f},
        {"mine.ogg", "Mine", "", "", 4.0f, 100.0f},
    };
    library_t lib = {songs, 3};
    level_header_t hdr = {.music = "level.ogg", .music_offset = 1.5};
    song_choice_t c;

    c = music_choose(&lib, "mine.ogg", &hdr, SONG_OVERRIDE);
    CHECK(c.source == SONG_OVERRIDE && c.offset == 4.0);  /* its own offset */
    c = music_choose(&lib, "", &hdr, SONG_OVERRIDE);
    CHECK(c.source == SONG_LEVEL && c.offset == 1.5);
    c = music_choose(&lib, "deleted.ogg", &hdr, SONG_OVERRIDE);
    CHECK(c.source == SONG_LEVEL);
    c = music_choose(&lib, "mine.ogg", &hdr, SONG_LEVEL);     /* it failed */
    CHECK(c.source == SONG_LEVEL);
    hdr.music_offset = 0.0;
    CHECK(music_choose(&lib, NULL, &hdr, SONG_OVERRIDE).offset == 3.0);
    snprintf(hdr.music, sizeof(hdr.music), "%s", "deleted.ogg");
    c = music_choose(&lib, NULL, &hdr, SONG_OVERRIDE);
    CHECK(c.source == SONG_DEFAULT && c.offset == 2.0);
    CHECK(strcmp(c.song->file, "back_mus.ogg") == 0);
    c = music_choose(&lib, NULL, &hdr, SONG_NONE);
    CHECK(c.source == SONG_NONE && c.song == NULL);
    lib.count = 0;
    CHECK(music_choose(&lib, "mine.ogg", &hdr, SONG_OVERRIDE).song == NULL);
    lib.count = 3;
    CHECK(music_menu_song(&lib, "mine.ogg") == &songs[2]);
    CHECK(music_menu_song(&lib, "menu_loop.ogg") == &songs[0]);
}

static void test_sync_and_volume(void)
{
    level_data_t lvl = {0};

    CHECK(music_expected(1.25, 0, 0.0) == 1.25);
    CHECK(fabs(music_expected(1.25, TICK_RATE * 3, 0.1) - 4.35) < 1e-9);
    CHECK(!music_drifted(10.0, 10.04) && !music_drifted(10.0, 9.96));
    CHECK(music_drifted(10.0, 10.06) && music_drifted(10.0, 9.9));
    CHECK(music_volume_curve(100) == 100.0f);
    CHECK(music_volume_curve(50) == 25.0f);       /* sounds like half (4.7) */
    CHECK(music_volume_curve(0) == 0.0f && music_volume_curve(150) == 100.0f);
    lvl.end_shift = SCROLL_SPEED * 12.0;
    lvl.hdr.start.speed_mult = 1.0;
    CHECK(fabs(level_duration(&lvl) - 12.0) < 1e-9);
    lvl.hdr.start.speed_mult = 2.0;
    CHECK(fabs(level_duration(&lvl) - 6.0) < 1e-9);
}

static void clean(void)
{
    static const char *const files[] = {"zeta.ogg", "Alpha.mp3", "b.flac",
        "c.wav", "broken.ogg", ".hidden.ogg", "notes.txt", "has space.ogg",
        SONGS_META};
    char path[256];

    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        snprintf(path, sizeof(path), "%s/%s", tmp_dir, files[i]);
        remove(path);
    }
    rmdir(tmp_dir);
}

void test_music(void)
{
    if (mkdtemp(tmp_dir) == NULL) {
        CHECK(!"mkdtemp");
        return;
    }
    test_scan();
    test_meta();
    test_missing_folder();
    test_file_names();
    test_choice();
    test_sync_and_volume();
    clean();
}
