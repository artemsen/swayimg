// SPDX-License-Identifier: MIT

#include "browser.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

struct browser {
    char** files;
    int max;
    int total;
    int current;
    bool recursive;
};

struct browser browser;

typedef struct {
    char** files;
    int max;
    int total;
} loader;

static void add_file(loader* loader, const char* file)
{
    if (loader->max == loader->total) {
        loader->max += 256;
        loader->files = realloc(loader->files, loader->max * sizeof(char*));
        if (!loader->files) {
            fprintf(stderr, "Not enough memory\n");
            return;
        }
    }
    const size_t len = strlen(file);
    loader->files[loader->total] = malloc(len + 1);
    memcpy(loader->files[loader->total], file, len);
    loader->files[loader->total][len] = '\0';
    loader->total++;
}

static int compare(const FTSENT** a, const FTSENT** b)
{
    return (strcmp((*a)->fts_name, (*b)->fts_name));
}

static bool is_directory(const char *path)
{
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

static void load_directory(loader* loader, const char* dir, bool recursive)
{
    FTS* file_system = NULL;
    FTSENT* child = NULL;
    FTSENT* parent = NULL;

    char* const dirs[2] = { (char* const)dir, NULL };
    file_system = fts_open(dirs, FTS_COMFOLLOW | FTS_NOCHDIR, &compare);

    if (file_system) {
        char file[PATH_MAX];
        while( (parent = fts_read(file_system))) {
            child = fts_children(file_system, 0);
            if (errno != 0) {
                fprintf(stderr, "Unable to load directory %s: %s\n", dir, strerror(errno));
                break;
            }

            while (child && child->fts_link) {
                child = child->fts_link;
                if (child->fts_info == FTS_F && (recursive || child->fts_level == 1)) {
                    snprintf(file, PATH_MAX, "%s%s", child->fts_path, child->fts_name);
                    add_file(loader, file);
                }
            }
        }
        fts_close(file_system);
    } else {
        fprintf(stderr, "Unable to traverse directory %s: %s\n", dir, strerror(errno));
    }
}

bool create_browser(const char** paths, size_t paths_num, bool recursive)
{
    loader loader;
    loader.max = 128;
    loader.total = 0;
    loader.files = (char**)malloc(loader.max * sizeof(char*));
    if (!loader.files) {
        fprintf(stderr, "Not enough memory\n");
        return false;
    }

    if (paths_num > 0) {
        for (size_t i = 0; i < paths_num; i++) {
            if (is_directory(paths[i])) {
                load_directory(&loader, paths[i], recursive);
            } else {
                add_file(&loader, paths[i]);
            }
        }
    } else {
        char cwd[PATH_MAX];
        if (getcwd(cwd, PATH_MAX)) {
            fprintf(stderr, "Unable to get current directory: %s\n", strerror(errno));
            return false;
        }
        load_directory(&loader, cwd, recursive);
    }

    browser.total = loader.total;
    browser.current = -1;
    browser.files = (char**)realloc(loader.files, loader.total * sizeof(char*));
    return true;
}

void destroy_browser()
{
    for (int i = 0; i < browser.total; i++) {
        if (browser.files[i]) {
            free(browser.files[i]);
        }
    }
    free(browser.files);
}

const char* get_next_file(bool forward)
{
    if (browser.total == 0) {
        return NULL;
    }
    int initial = browser.current;
    const int delta = forward ? 1 : -1;
    do {
        browser.current += delta;
        if (browser.current == browser.total) {
            browser.current = 0;
        } else if (browser.current < 0) {
            browser.current = browser.total - 1;
        }
        if (browser.current == initial) {
            // we have looped around all files without finding anything
            return NULL;
        }
    } while (!browser.files[browser.current]);
    return browser.files[browser.current];
}

const char* get_current_file()
{
    return browser.files[browser.current];
}

void skip_current_file()
{
    free(browser.files[browser.current]);
    browser.files[browser.current] = NULL;
}
