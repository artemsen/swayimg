// SPDX-License-Identifier: MIT

#include <stdlib.h>
#include <stdbool.h>

struct browser;

extern struct browser browser;

/**
 * Initialize browser.
 * @param[in] paths list of files or directories
 * @param[in] paths_num number of paths
 * @return browser instance or NULL on error
 */
bool init_browser(const char** paths, size_t paths_num, bool recursive);
void destroy_browser();

/**
 * Get next (or previous) file path.
 * @param[in] context browser context
 * @param[in] forward whether to iterate forwards (iterates backwards when false)
 * @return path of next file
 */
const char* get_next_file(bool forward);

/**
 * Get current file path.
 * @param[in] context browser context
 * @return path of current file
 */
const char* get_current_file();

/**
 * Remove current file from browser. It won't be returned on subsequent iterations.
 * @param[in] context browser context
 */
void skip_current_file();
