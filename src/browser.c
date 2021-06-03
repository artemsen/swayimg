#include "browser.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <errno.h>
#include <limits.h>

struct browser {
    /** File list. */
    char** files;
    int max;
    int total;
    int current;
    bool recursive;   ///< Recurse into subdirectories
};

void add_file(browser* browser, const char* file)
{
    if (browser->max == browser->total) {
        browser->max *= 2;
        browser->files = realloc(browser->files, browser->max * sizeof(char*));
        if (!browser->files) {
            fprintf(stderr, "Not enough memory\n");
            return;
        }
    }
    size_t len = strlen(file);
    browser->files[browser->total] = malloc(len + 1);
    memcpy(browser->files[browser->total], file, len);
    browser->files[browser->total][len] = '\0';
    browser->total++;
}

int compare(const FTSENT** a, const FTSENT** b)
{
    return (strcmp((*a)->fts_name, (*b)->fts_name));
}

int is_directory(const char *path)
{
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

void load_directory(browser* context, const char *dir) {
    FTS* file_system = NULL;
    FTSENT* child = NULL;
    FTSENT* parent = NULL;

    file_system = fts_open((char* const*)&dir, FTS_COMFOLLOW | FTS_NOCHDIR, &compare);

    if (file_system) {
        while( (parent = fts_read(file_system))) {
            child = fts_children(file_system, 0);
            if (errno != 0) {
                fprintf(stderr, "Unable to load directory %s: %s\n", dir, strerror(errno));
            }

            char* file = malloc(PATH_MAX);
            while (child && child->fts_link) {
                child = child->fts_link;
                if (child->fts_info == FTS_F && (context->recursive || child->fts_level == 1)) {
                    snprintf(file, PATH_MAX, "%s%s", child->fts_path, child->fts_name);
                    add_file(context, file);
                }
            }
            free(file);
        }
        fts_close(file_system);
    }
}

browser* create_browser(const char** paths, size_t paths_num, bool recursive)
{
    browser* browser = malloc(sizeof(browser));
    if (!browser) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }
    browser->max = 1024;
    browser->total = 0;
    browser->current = -1;
    browser->recursive = recursive;
    browser->files = (char**)malloc(browser->max * sizeof(char*));
    if (!browser->files) {
        fprintf(stderr, "Not enough memory\n");
        return NULL;
    }

    for (size_t i = 0; i < paths_num; i++) {
        if (is_directory(paths[i])) {
            load_directory(browser, paths[i]);
        } else {
            add_file(browser, paths[i]);
        }
    }
    return browser;
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

void delete_current_file(browser* context)
{
    free(context->files[context->current]);
    context->files[context->current] = NULL;
}
