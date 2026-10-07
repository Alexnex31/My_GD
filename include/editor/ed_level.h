/*
** ALEXNEX PROJECT, 2026
** editor/ed_level.h
** File description:
** header file for my_gd project
*/

#ifndef EDITOR_ED_LEVEL_H
    #define EDITOR_ED_LEVEL_H

    #include <stdbool.h>
    #include <stddef.h>
    #include "sim/level.h"
    #include "sim/sim_types.h"

/*
** The level the editor edits (FEATURES 11.1): an unsorted list of objects,
** each with an id that never changes and is never given twice in a session,
** so a selection or an undo step still means the same object after others
** were deleted. Pure: no window, tested alone.
*/
typedef struct ed_object {
    int id;
    object_t obj;             /* the sim's own struct, hitbox included        */
    char *extra;              /* fields nothing here edits, saved unchanged   */
    bool selected;
} ed_object_t;

typedef struct ed_level {
    char id[LEVEL_ID_MAX + 1];    /* the file is levels/<id>.gd (7.2)         */
    level_header_t hdr;
    ed_object_t *objects;
    size_t count;
    size_t cap;
    level_start_t *starts;    /* the other start positions, as loaded (11.8)  */
    size_t nb_starts;
    int next_id;
    bool dirty;               /* changed since it was opened or saved         */
} ed_level_t;

/* One thing the palette can place: a type and the word its line needs. */
typedef struct ed_entry {
    obj_type_t type;
    const char *word;         /* "ship", "up", "yellow"..., or NULL           */
    char label[32];           /* what the palette shows: "portal ship"        */
} ed_entry_t;

    #define ED_MAX_ENTRIES 64

void ed_level_new(ed_level_t *lv, const char *id);
void ed_level_from_text(ed_level_t *lv, const char *buf, size_t len,
    const char *id);
int ed_level_open(ed_level_t *lv, const char *path);     /* -1: unreadable */
char *ed_level_text(const ed_level_t *lv);               /* what a save writes */
int ed_level_save(ed_level_t *lv, const char *path);     /* clears dirty */
void ed_level_free(ed_level_t *lv);

/* The same level as a document: for the writer, a playtest, the bot. */
void ed_level_doc(const ed_level_t *lv, level_doc_t *doc);
void ed_doc_free(level_doc_t *doc);

/* Where the level ends and its kill ceiling, as the game will compute them. */
void ed_level_measure(const ed_level_t *lv, double *end_x, double *kill_y);

int ed_add(ed_level_t *lv, const object_t *obj, const char *extra);  /* its id */
bool ed_remove(ed_level_t *lv, int id);
ed_object_t *ed_find(ed_level_t *lv, int id);
/* Puts an object back where it was, with the id it had: an undo (11.7). */
void ed_insert(ed_level_t *lv, const ed_object_t *o, size_t at);

/* The cell a point is in: floor, so it is the cell under the mouse (11.4). */
double ed_snap(double v, double grid);
/* The object drawn on top at a point, or -1 (11.5). */
int ed_hit(const ed_level_t *lv, vec2_t at);

/* Every entry, from the object, mode and colour tables (11.14, E4). */
int ed_entries(ed_entry_t *out, int max);
/*
** Places an entry in the cell under `at`. false when that cell already holds
** the very same object, so a dragged click doesn't stack copies (11.13).
*/
bool ed_place(ed_level_t *lv, const ed_entry_t *e, vec2_t at, double grid);
/* The object that would be placed, without placing it; false as above. */
bool ed_place_object(const ed_level_t *lv, const ed_entry_t *e, vec2_t at,
    double grid, object_t *out);

/* The id a new level takes: one past the highest of those that exist. */
void ed_next_id(const char *const *ids, size_t count, char *out, size_t size);

#endif
