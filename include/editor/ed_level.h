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
    bool is_start;            /* a start position (11.8), not a level object: */
    level_start_t start;      /* obj is then only the cell it is shown in     */
} ed_object_t;

typedef struct ed_level {
    char id[LEVEL_ID_MAX + 1];    /* the file is levels/<id>.gd (7.2)         */
    level_header_t hdr;
    ed_object_t *objects;
    size_t count;
    size_t cap;
    int next_id;
    bool dirty;               /* changed since it was opened or saved         */
    char **kept;              /* lines the loader couldn't use, saved as they */
    size_t nb_kept;           /* were (7.2): nothing edits them               */
    int layer;                /* the editor layer worked in, or ED_LAYER_ALL: */
                              /* the others fade and can't be picked (11.15)  */
} ed_level_t;

    #define ED_LAYER_ALL (-1)

/* One thing the palette can place: a type and the word its line needs. */
typedef struct ed_entry {
    obj_type_t type;
    bool start;               /* a start position: type and word unused       */
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
/* A copy of any editor object, start positions included, under a new id. */
int ed_add_object(ed_level_t *lv, const ed_object_t *src);
/* A start position as an editor object: a cell around the player's center. */
void ed_start_object(ed_object_t *out, const level_start_t *st);
/* After its rect, turn or kind changed: the hitbox, a start's position. */
void ed_refresh(ed_object_t *o);
size_t ed_starts(const ed_level_t *lv);

bool ed_remove(ed_level_t *lv, int id);
ed_object_t *ed_find(ed_level_t *lv, int id);
/* The same, looking from *hint first and leaving it after the one found: a
** walk over ids in list order is linear. NULL when no object has that id. */
ed_object_t *ed_find_from(ed_level_t *lv, int id, size_t *hint);
/* Puts an object back where it was, with the id it had: an undo (11.7). */
void ed_insert(ed_level_t *lv, const ed_object_t *o, size_t at);

/* In the editor layer worked in: start positions are in all of them. */
bool ed_in_layer(const ed_level_t *lv, const ed_object_t *o);
/* Indices of level objects, sorted as the canvas and the game draw them. */
void ed_sort_drawn(const ed_level_t *lv, size_t *idx, size_t n);

/* The cell a point is in: floor, so it is the cell under the mouse (11.4). */
double ed_snap(double v, double grid);
/* The object the canvas draws on top at a point, of the layer worked in,
** or -1 (11.5). */
int ed_hit(const ed_level_t *lv, vec2_t at);

/* Every entry, from the object, mode and colour tables (11.14, E4). */
int ed_entries(ed_entry_t *out, int max);
/*
** Places an entry in the cell under `at`, in the layer worked in. false when
** that cell already holds the very same object, so a dragged click doesn't
** stack copies (11.13).
*/
bool ed_place(ed_level_t *lv, const ed_entry_t *e, vec2_t at, double grid);
/* The object that would be placed, without placing it; false as above. */
bool ed_place_object(const ed_level_t *lv, const ed_entry_t *e, vec2_t at,
    double grid, ed_object_t *out);

/*
** Two objects the game can't tell apart (object_same), and the same start
** position. The editor layer is the editor's only: `layer_too` asks for it
** as well.
*/
bool ed_same_object(const ed_object_t *a, const ed_object_t *b,
    bool layer_too);

/* The id a new level takes: one past the highest of those that exist, or
** the smallest free one when that one wouldn't fit `size`. */
void ed_next_id(const char *const *ids, size_t count, char *out, size_t size);

#endif
