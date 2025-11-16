#ifndef SS_SELECTOR_H
#define SS_SELECTOR_H

#include "../types.h"

/**
 * Select best storage server for new file
 * Strategy: Round-robin among active servers (simple & balanced)
 * 
 * @return Pointer to selected SS or NULL if none available
 */
StorageServerInfo* select_ss_for_file();

/**
 * Get load statistics for a storage server
 * @param ss_id Storage server ID
 * @return Number of files on this SS
 */
int get_ss_load(int ss_id);

#endif // SS_SELECTOR_H