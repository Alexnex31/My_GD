/*
** ALEXNEX PROJECT, 2026
** sim/save_file.c
** File description:
** atomic saves and whole reads, for the progress and the settings (6.4, FEATURES 2.4)
*/

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sim/alloc.h"
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
** The old file's permissions, or a new file's as fopen would give it. The
** umask is read by setting it back: nothing else makes files meanwhile.
*/
static mode_t file_mode(const char *path)
{
    struct stat st;
    mode_t mask = 0;

    if (stat(path, &st) == 0)
        return st.st_mode & 07777;
    mask = umask(0);
    umask(mask);
    return 0666 & ~mask;
}

static int discard(FILE *f, const char *tmp)
{
    fclose(f);
    remove(tmp);
    return -1;
}

/*
** The new content is complete and on disk before the rename makes it
** visible, so a reader never sees half of it. The temporary file's name is
** unique: two writers never share one. A failed write may leave fflush and
** fclose happy once its buffer is gone, so ferror is asked first.
*/
int save_atomic(const char *path, save_write_fn write, const void *data)
{
    char real[PATH_MAX];
    char tmp[PATH_MAX];
    FILE *f = NULL;
    int fd = -1;

    if (realpath(path, real) != NULL)
        path = real;                         /* a symlink stays one */
    if (make_parent_dir(path) != 0)
        return -1;
    if (snprintf(tmp, sizeof(tmp), "%s.XXXXXX", path) >= (int)sizeof(tmp))
        return -1;
    fd = mkstemp(tmp);
    if (fd < 0)
        return -1;
    if (fchmod(fd, file_mode(path)) != 0 || (f = fdopen(fd, "w")) == NULL) {
        close(fd);
        return (void)remove(tmp), -1;
    }
    write(f, data);
    if (ferror(f) || fflush(f) != 0 || fsync(fileno(f)) != 0)
        return discard(f, tmp);
    if (fclose(f) != 0)                      /* write errors surface here too */
        return (void)remove(tmp), -1;
    return rename(tmp, path) == 0 ? 0 : (remove(tmp), -1);
}

/* Everything up to the end of the file, unless it's bigger than a save. */
static char *read_all(FILE *f, size_t *len)
{
    size_t cap = 4096;
    char *buf = sim_xcalloc(cap + 1, 1);
    char *bigger = NULL;
    size_t got = 0;

    *len = 0;
    while ((got = fread(buf + *len, 1, cap - *len, f)) > 0) {
        *len += got;
        if (*len > SAVE_FILE_MAX)
            return free(buf), NULL;
        if (*len < cap)
            continue;
        bigger = sim_xcalloc(cap * 2 + 1, 1);
        memcpy(bigger, buf, *len);
        free(buf);
        buf = bigger;
        cap *= 2;
    }
    if (ferror(f))
        return free(buf), NULL;
    return buf;
}

/*
** Only "no such file" is a missing file: anything else that stops the read
** (no permission, a folder, an I/O error) is unreadable, and the caller
** never saves over it.
*/
save_read_t save_read(const char *path, char **text)
{
    FILE *f = fopen(path, "r");
    struct stat st;
    size_t len = 0;

    *text = NULL;
    if (f == NULL)
        return errno == ENOENT ? SAVE_MISSING : SAVE_UNREADABLE;
    if (fstat(fileno(f), &st) != 0 || !S_ISREG(st.st_mode))
        return fclose(f), SAVE_UNREADABLE;
    *text = read_all(f, &len);
    fclose(f);
    if (*text == NULL)
        return SAVE_UNREADABLE;
    for (char *nul = memchr(*text, '\0', len); nul != NULL;
        nul = memchr(nul, '\0', len - (size_t)(nul - *text)))
        *nul = '\n';
    return SAVE_READ;
}

char *save_next_line(char **cursor)
{
    char *line = *cursor;
    char *nl = NULL;
    size_t n = 0;

    if (line == NULL || *line == '\0')
        return NULL;
    nl = strchr(line, '\n');
    if (nl != NULL) {
        *nl = '\0';
        *cursor = nl + 1;
    } else
        *cursor = line + strlen(line);
    n = strlen(line);
    if (n > 0 && line[n - 1] == '\r')
        line[n - 1] = '\0';
    return line;
}
