/*
** ALEXNEX PROJECT, 2026
** sim/progress.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_PROGRESS_H
    #define SIM_PROGRESS_H

    #include "sim/sim_types.h"

    #define SAVE_DIR   "save"
    #define SAVE_PATH  "save/progress.txt"

/*
** One level's records (6.4). "extra" keeps the key=value fields this build
** doesn't know, so an older binary never destroys a newer one's data.
*/
typedef struct progress_entry {
    char id[24];              /* the level file's digits, e.g. "10280" (7.2)  */
    int attempts;
    float best;
    float practice_best;      /* Phase 13: a stat only                        */
    uint64_t level_hash;      /* the level file when best was set, 0 = unknown */
    char song[128];           /* FEATURES 4.6: player's override, "" = none   */
    char *extra;              /* unknown fields, written back unchanged       */
} progress_entry_t;

typedef struct progress {
    progress_entry_t *entries;
    size_t count;
    size_t cap;
    char path[256];
} progress_t;

/* A missing file is an empty store, not an error. Returns 0, or -1 on a read error. */
int progress_load(progress_t *p, const char *path);

/* Writes through a .tmp + rename, so a crash never truncates the store. */
int progress_save(const progress_t *p);

/* The entry for this id, created empty if the store has none. NULL if invalid. */
progress_entry_t *progress_get(progress_t *p, const char *id);

/* The entry for this id, or NULL: a lookup that never adds one (6.4). */
progress_entry_t *progress_find(const progress_t *p, const char *id);

/*
** A percentage for a "%.2f": below 100 it never prints as "100.00", which
** reads back as a level beaten (6.4).
*/
float progress_printable(float pct);

void progress_free(progress_t *p);

/* Digits only, 1 to LEVEL_ID_MAX of them: the level file's name (7.2). */
bool progress_valid_id(const char *id);

#endif
