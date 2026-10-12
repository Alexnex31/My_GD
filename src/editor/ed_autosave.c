/*
** ALEXNEX PROJECT, 2026
** editor/ed_autosave.c
** File description:
** the editor's autosave: written beside the level, offered back (FEATURES 11.12)
*/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "editor/ed_autosave.h"

void ed_autosave_path(const char *dir, const char *id, char *out, size_t size)
{
    snprintf(out, size, "%s/%s.gd", dir, id);
}

/* mkdir -p: every folder of the path, those that exist left alone. */
static void make_dirs(const char *dir)
{
    char path[512];

    snprintf(path, sizeof(path), "%s", dir);
    for (char *p = path + 1; *p != '\0'; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        mkdir(path, 0755);
        *p = '/';
    }
    mkdir(path, 0755);
}

int ed_autosave_write(const ed_level_t *lv, const char *dir)
{
    char path[512];
    level_doc_t doc;
    int ret;

    make_dirs(dir);
    ed_autosave_path(dir, lv->id, path, sizeof(path));
    ed_level_doc(lv, &doc);
    ret = level_write(path, &doc, &lv->hdr);
    ed_doc_free(&doc);
    return ret;
}

static bool newer(const struct stat *a, const struct stat *b)
{
    if (a->st_mtim.tv_sec != b->st_mtim.tv_sec)
        return a->st_mtim.tv_sec > b->st_mtim.tv_sec;
    return a->st_mtim.tv_nsec > b->st_mtim.tv_nsec;
}

bool ed_autosave_pending(const char *dir, const char *id,
    const char *level_path, time_t *when)
{
    char path[512];
    struct stat saved;
    struct stat level;

    ed_autosave_path(dir, id, path, sizeof(path));
    if (stat(path, &saved) != 0 || !S_ISREG(saved.st_mode))
        return false;
    if (when != NULL)
        *when = saved.st_mtim.tv_sec;
    return stat(level_path, &level) != 0 || newer(&saved, &level);
}

int ed_autosave_restore(ed_level_t *lv, const char *dir)
{
    char path[512];
    ed_level_t from;

    ed_autosave_path(dir, lv->id, path, sizeof(path));
    if (ed_level_open(&from, path) != 0)
        return -1;
    ed_level_free(lv);
    *lv = from;
    lv->dirty = true;
    return 0;
}

void ed_autosave_remove(const char *dir, const char *id)
{
    char path[512];

    ed_autosave_path(dir, id, path, sizeof(path));
    unlink(path);
}
