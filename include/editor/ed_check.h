/*
** ALEXNEX PROJECT, 2026
** editor/ed_check.h
** File description:
** header file for my_gd project
*/

#ifndef EDITOR_ED_CHECK_H
    #define EDITOR_ED_CHECK_H

    #include "editor/ed_level.h"

/*
** What looks wrong in the level being edited (FEATURES 11.13). Warnings are
** almost always mistakes; hints are often meant. None of it stops a save or
** a playtest. Pure, tested alone.
*/
typedef enum ed_issue_kind {
    ED_ISSUE_TWICE,           /* the same object twice in the same place     */
    ED_ISSUE_UNDERGROUND,     /* wholly below the ground                     */
    ED_ISSUE_BEHIND_START,    /* wholly behind where the level starts        */
    ED_ISSUE_STEEP,           /* a face looking up, steeper than 50 degrees  */
    ED_HINT_SPIKE_IN_BLOCK,   /* a spike's hitbox inside a block's           */
    ED_HINT_ENDS_IN_MODE,     /* the last portal isn't a cube portal         */
    ED_ISSUE_KIND_COUNT
} ed_issue_kind_t;

typedef struct ed_issue {
    ed_issue_kind_t kind;
    int id;                   /* the object it is about                      */
} ed_issue_t;

typedef struct ed_report {
    ed_issue_t *issues;       /* from the level's left to its right          */
    size_t count;
    size_t warnings;
    size_t hints;
} ed_report_t;

/* Replaces what `out` held: zero it before its first use. */
void ed_check(const ed_level_t *lv, ed_report_t *out);
void ed_report_free(ed_report_t *r);

bool ed_issue_is_hint(ed_issue_kind_t kind);
const char *ed_issue_text(ed_issue_kind_t kind);

#endif
