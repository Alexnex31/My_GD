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
** file in those warnings. Returns the object lines it skipped: the invalid
** lines --check fails on (7.4). A field or header value it ignores is a
** warning, not one of them. With objs NULL it reads the header alone: object
** lines are recognized by their type and nothing else (7.5).
*/
int level_parse_mem(const char *buf, size_t len, const char *source,
    object_t **objs, size_t *count, level_header_t *hdr, sim_log_fn log);

/*
** A level as a document, for what edits one (FEATURES 11.1): its objects in
** file order, and what each line carried that nothing here edits (group=,
** keys from a newer version) to be written back unchanged. The header is
** kept beside it.
*/
typedef struct level_doc {
    object_t *objs;
    char **extras;            /* per object: " key=value ..." or NULL */
    size_t count;
} level_doc_t;

/* Returns the lines it skipped, like level_parse_mem. hdr holds its defaults. */
int level_parse_doc(const char *buf, size_t len, const char *source,
    level_doc_t *doc, level_header_t *hdr, sim_log_fn log);
void level_doc_free(level_doc_t *doc);
void level_header_defaults(level_header_t *hdr, const char *id);

/*
** The file's text: the header fields that aren't at their default, then the
** objects by x then y, each with only the fields it needs. Parsing it gives the same document back. malloc'd, NUL terminated.
*/
char *level_write_mem(const level_doc_t *doc, const level_header_t *hdr);
/* To <path>.tmp, then renamed over path: a crash never leaves half a level. */
int level_write(const char *path, const level_doc_t *doc,
    const level_header_t *hdr);

const char *obj_type_name(obj_type_t type);
/* One object from its line, as the loader reads it; -1 if it isn't one. */
int level_object_from_line(const char *line, object_t *o);

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
