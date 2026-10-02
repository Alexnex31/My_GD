/*
** ALEXNEX PROJECT, 2026
** sim/level.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_LEVEL_H
    #define SIM_LEVEL_H

    #include "sim/sim_types.h"

/*
** Reads a level file's text (7.2). Bad lines are warnings, never failures:
** they are skipped and the rest of the level loads. "source" only names the
** file in those warnings. Returns 0.
*/
int level_parse_mem(const char *buf, size_t len, const char *source,
    object_t **objs, size_t *count, level_header_t *hdr, sim_log_fn log);

    #define LEVEL_ID_MAX 18      /* digits in an id: fits any GD-sized number */
    #define LEVEL_COORD_MAX 1e7  /* px: x, y, w and h, hours of level (7.2)   */

/* "levels/10280.gd" -> "10280"; false when the name isn't <digits>.gd (7.2) */
bool level_id_from_path(const char *path, char *id, size_t size);

/*
** The header and the file's hash, without building a single hitbox (7.5): the
** level list needs a name and a version marker for every file it shows.
** Returns 0, or -1 when the file cannot be read.
*/
int level_read_header(const char *path, level_header_t *hdr,
    uint64_t *file_hash);

/* FNV-1a over raw bytes: the level file's version (6.4) and the progress store. */
uint64_t fnv1a(const void *data, size_t len);

/* Hitboxes, sort by (aabb.x, line), then reach, end_shift and kill_y. */
void level_finalize(level_data_t *lvl);

#endif
