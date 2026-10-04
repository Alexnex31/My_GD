/*
** ALEXNEX PROJECT, 2026
** check
** File description:
** ./my_gd --check levels/<id>.gd: load a level and report what looks wrong (7.4)
*/

#include "mygd.h"
#include "sim/bot.h"
#include "sim/modes.h"

static int g_loader_messages;

/*
** The loader's messages. Only the lines it skipped make the exit code 1
** (lvl.skipped_lines); an ignored field or header value is a warning.
*/
static void loader_warning(const char *msg)
{
    dprintf(2, "  %s\n", msg);
    g_loader_messages += 1;
}

static int warn(const char *fmt, int line, double x)
{
    dprintf(2, "  line %d (x %.0f): %s\n", line, x, fmt);
    return 1;
}

/*
** Two identical objects in the same place: one of them is invisible and only
** costs collision time. They are sorted by x, so a duplicate is a neighbour.
*/
static int check_duplicates(const level_data_t *lvl)
{
    int found = 0;

    for (size_t i = 1; i < lvl->nb_objects; i++) {
        const object_t *a = &lvl->objects[i - 1];
        const object_t *b = &lvl->objects[i];

        if (a->type == b->type && a->rotation == b->rotation
            && a->rect.x == b->rect.x && a->rect.y == b->rect.y
            && a->rect.w == b->rect.w && a->rect.h == b->rect.h)
            found += warn("the same object twice in the same place", b->line,
                b->rect.x);
    }
    return found;
}

static int check_below_ground(const level_data_t *lvl)
{
    int found = 0;

    for (size_t i = 0; i < lvl->nb_objects; i++) {
        const object_t *o = &lvl->objects[i];

        if (o->hitbox.aabb.y >= GROUND_Y)
            found += warn("entirely below the ground: unreachable", o->line,
                o->rect.x);
    }
    return found;
}

/*
** A neutral face that looks up but is steeper than 50 degrees is a wall, not
** a floor (4.4): the player runs into it and dies instead of climbing it.
*/
static int check_steep_faces(const level_data_t *lvl)
{
    int found = 0;

    for (size_t i = 0; i < lvl->nb_objects; i++) {
        const object_t *o = &lvl->objects[i];

        if (OBJ_CATEGORY[o->type] != CAT_NEUTRAL)
            continue;
        for (int f = 0; f < o->hitbox.nverts; f++) {
            double up = -o->hitbox.face_n[f].y;

            if (up > 0.0 && up < FLOOR_MIN_DOT)
                found += warn("a face steeper than 50 degrees faces up: it is"
                    " a wall, not a slope", o->line, o->rect.x);
        }
    }
    return found;
}

/*
** More objects within one tick's reach than the broadphase can hold (4.1):
** past MAX_CANDIDATES the rest are simply not tested.
*/
static int check_crowding(const level_data_t *lvl)
{
    double window = PER_TICK(SPEED_XFAST) + 2.0 * PLAYER_HALF + lvl->reach;
    size_t j = 0;

    for (size_t i = 0; i < lvl->nb_objects; i++) {
        j = i;
        while (j < lvl->nb_objects
            && lvl->objects[j].hitbox.aabb.x < lvl->objects[i].hitbox.aabb.x
            + window)
            j += 1;
        if (j - i > MAX_CANDIDATES)
            return warn("more objects here than the broadphase tests at once:"
                " some are ignored", lvl->objects[i].line,
                lvl->objects[i].rect.x);
    }
    return 0;
}

/*
** The level ends 500 px after its last object, so nothing can sit past the
** end; what can be unreachable is what sits BEHIND the spawn (7.2's start_x).
*/
static int check_behind_spawn(const level_data_t *lvl)
{
    double spawn = lvl->hdr.start.pos.x - PLAYER_HALF;
    int found = 0;

    for (size_t i = 0; i < lvl->nb_objects; i++) {
        const rect_t *a = &lvl->objects[i].hitbox.aabb;

        if (a->x + a->w < spawn)
            found += warn("behind the spawn: never reached",
                lvl->objects[i].line, lvl->objects[i].rect.x);
    }
    return found;
}

static void library_warning(const char *msg)
{
    dprintf(2, "  %s\n", msg);
}

/*
** The song that will play (FEATURES 4.4), from the music/ next to the
** executable, and whether the level outlasts it (4.9): it would loop.
*/
static int check_song(const level_data_t *lvl)
{
    char dir[4096];
    library_t lib = {0};
    song_choice_t c;
    double over = 0.0;
    int found = 0;

    if (exe_dir(dir, sizeof(dir) - 8) != 0)
        return 0;
    strcat(dir, "/" MUSIC_DIR);
    library_scan(&lib, dir, music_probe, library_warning);
    c = music_choose(&lib, NULL, &lvl->hdr, SONG_OVERRIDE);
    if (lvl->hdr.music[0] != '\0' && c.source != SONG_LEVEL)
        found += (dprintf(2, "  music %s isn't in music/: the default song"
            " plays\n", lvl->hdr.music), 1);
    if (c.song == NULL) {
        dprintf(2, "  song: none, the level plays in silence\n");
        return library_free(&lib), found;
    }
    dprintf(2, "  song: %s (%s), from %.2f s; level %.1f s, song %.1f s\n",
        c.song->title, c.song->file, c.offset, level_duration(lvl),
        c.song->duration);
    over = c.offset + level_duration(lvl) - c.song->duration;
    if (over > 0.0) {
        dprintf(2, "  the level is %.1f s longer than its song: it loops\n",
            over);
        found += 1;
    }
    library_free(&lib);
    return found;
}

/* Whether the bot finds a way through: information, never a failure. */
static void report_bot(sim_t *s)
{
    bot_result_t r = bot_solve(s, BOT_MAX_ATTEMPTS);
    char line[128];

    bot_describe(&r, line, sizeof(line));
    dprintf(2, "  bot: %s\n", line);
    bot_result_free(&r);
}

/*
** Loads with the simulation only, no window. Exit 1 when the file cannot be
** read or has invalid lines; everything else is information (7.4).
*/
int level_check(const char *path)
{
    sim_t s;
    int issues = 0;
    int bad = 0;

    g_loader_messages = 0;
    dprintf(2, "%s:\n", path);
    if (sim_load(&s, path, loader_warning) != 0) {
        dprintf(2, "  cannot be read\n");
        return 1;
    }
    dprintf(2, "  \"%s\" by %s: %zu objects, %.0f px long, ends at %.1f%%\n",
        s.lvl.hdr.name, s.lvl.hdr.author[0] ? s.lvl.hdr.author : "nobody",
        s.lvl.nb_objects, s.lvl.end_shift, 100.0);
    issues += check_duplicates(&s.lvl);
    issues += check_below_ground(&s.lvl);
    issues += check_steep_faces(&s.lvl);
    issues += check_crowding(&s.lvl);
    issues += check_behind_spawn(&s.lvl);
    issues += check_song(&s.lvl);
    issues += g_loader_messages - s.lvl.skipped_lines;
    bad = s.lvl.skipped_lines;
    dprintf(2, "  %d invalid line(s), %d warning(s)\n", bad, issues);
    report_bot(&s);
    sim_free(&s);
    return bad > 0;
}
