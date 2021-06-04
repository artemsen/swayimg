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

browser* create_browser(const char** paths, size_t paths_num, bool recursive)
{
    loader loader;
    loader.max = 128;
    loader.total = 0;
    loader.files = (char**)malloc(loader.max * sizeof(char*));
    if (!loader.files) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
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
            return NULL;
        }
        load_directory(&loader, cwd, recursive);
    }

    browser* browser = malloc(sizeof(browser));
    if (!browser) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }
    browser->total = loader.total;
    browser->current = -1;
    browser->files = (char**)realloc(loader.files, loader.total * sizeof(char*));
    return browser;
}

void destroy_browser(browser* context)
{
    for (int i = 0; i < context->total; i++) {
        if (context->files[i]) {
            free(context->files[i]);
        }
    }
    free(context->files);
}

const char* next_file(browser* context, bool forward)
{
    if (context->total == 0) {
        return NULL;
    }
    int initial = context->current;
    const int delta = forward ? 1 : -1;
    do {
        context->current += delta;
        if (context->current == context->total) {
            context->current = 0;
        } else if (context->current < 0) {
            context->current = context->total - 1;
        }
        if (context->current == initial) {
            // we have looped around all files without finding anything
            return NULL;
        }
    } while (!context->files[context->current]);
    return context->files[context->current];
}

const char* current_file(browser* context)
{
    return context->files[context->current];
}

void skip_current_file(browser* context)
{
    free(context->files[context->current]);
    context->files[context->current] = NULL;
}
