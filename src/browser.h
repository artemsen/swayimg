#include <stdlib.h>
#include <stdbool.h>

typedef struct browser browser;

/**
 * Initialize browser.
 * @param[in] paths list of files or directories
 * @param[in] paths_num number of paths
 * @return browser instance or NULL on error
 */
struct browser* create_browser(const char** paths, size_t paths_num, bool recursive);

/**
 * Get next (or previous) file path.
 * @param[in] context browser context
 * @param[in] forward whether to iterate forwards (iterates backwards when false)
 * @return path of next file
 */
const char* next_file(browser* context, bool forward);

/**
 * Get current file path.
 * @param[in] context browser context
 * @return path of current file
 */
const char* current_file(browser* context);

/**
 * Remove current file from browser. It won't be returned on subsequent iterations.
 * @param[in] context browser context
 */
void delete_current_file(browser* context);
