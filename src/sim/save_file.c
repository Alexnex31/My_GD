/*
** ALEXNEX PROJECT, 2026
** sim/save_file.c
** File description:
** atomic saves, for the progress and the settings (6.4, FEATURES 2.4)
*/

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sim/save_file.h"

/* The file's own folder, whatever its path: "save" for save/progress.txt. */
static int make_parent_dir(const char *path)
{
    char dir[PATH_MAX];
    char *slash = NULL;

    snprintf(dir, sizeof(dir), "%s", path);
    slash = strrchr(dir, '/');
    if (slash == NULL || slash == dir)
        return 0;                            /* the working directory, or / */
    *slash = '\0';
    return mkdir(dir, 0755) != 0 && errno != EEXIST ? -1 : 0;
}

/*
** The new content is complete and on disk before the rename makes it
** visible, so a reader never sees half of it.
*/
int save_atomic(const char *path, save_write_fn write, const void *data)
{
    char tmp[PATH_MAX];
    FILE *f = NULL;

    if (make_parent_dir(path) != 0)
        return -1;
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp))
        return -1;
    f = fopen(tmp, "w");
    if (f == NULL)
        return -1;
    write(f, data);
    if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
        fclose(f);
        return (void)remove(tmp), -1;
    }
    if (fclose(f) != 0)                      /* write errors surface here */
        return (void)remove(tmp), -1;
    return rename(tmp, path) == 0 ? 0 : (remove(tmp), -1);
}
