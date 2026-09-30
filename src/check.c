/*
** ALEXNEX PROJECT, 2026
** check
** File description:
** ./my_gd --check levels/<id>.gd: load a level and report what looks wrong (7.4)
*/

#include "mygd.h"
#include "sim/modes.h"

static int g_bad_lines;

/* The loader's own warnings: an invalid line is what makes the exit code 1. */
static void loader_warning(const char *msg)
{
    dprintf(2, "  %s\n", msg);
    g_bad_lines += 1;
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

/*
** Loads with the simulation only, no window. Exit 1 when the file cannot be
** read or has invalid lines; everything else is information (7.4).
*/
int level_check(const char *path)
{
    sim_t s;
    int issues = 0;

    g_bad_lines = 0;
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
    dprintf(2, "  %d invalid line(s), %d warning(s)\n", g_bad_lines, issues);
    dprintf(2, "  bot: not implemented yet (Phase 8)\n");
    sim_free(&s);
    return g_bad_lines > 0;
}
