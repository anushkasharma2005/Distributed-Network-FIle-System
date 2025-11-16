#include "ss_selector.h"
#include "ss_registry.h"
#include "file_registry.h"
#include <stdio.h>

// Round-robin counter
static int rr_counter = 0;
static pthread_mutex_t rr_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Select SS using round-robin (simple load balancing)
 */
StorageServerInfo* select_ss_for_file() {
    StorageServerInfo* all_ss[MAX_STORAGE_SERVERS];
    int total = get_all_storage_servers(all_ss, MAX_STORAGE_SERVERS);
    
    if (total == 0) {
        fprintf(stderr, "[SS-Selector ERROR] No storage servers available\n");
        return NULL;
    }
    
    // Filter only active servers
    StorageServerInfo* active_ss[MAX_STORAGE_SERVERS];
    int active_count = 0;
    
    for (int i = 0; i < total; i++) {
        if (all_ss[i]->is_active) {
            active_ss[active_count++] = all_ss[i];
        }
    }
    
    if (active_count == 0) {
        fprintf(stderr, "[SS-Selector ERROR] No active storage servers\n");
        return NULL;
    }
    
    // Round-robin selection
    pthread_mutex_lock(&rr_mutex);
    int selected_index = rr_counter % active_count;
    rr_counter++;
    pthread_mutex_unlock(&rr_mutex);
    
    StorageServerInfo* selected = active_ss[selected_index];
    
    printf("[SS-Selector] Selected SS #%d (%s:%d) using round-robin\n",
           selected->ss_id, selected->ip_address, selected->client_port);
    
    return selected;
}

/**
 * Get load (number of files) on SS
 */
int get_ss_load(int ss_id) {
    FileInfo* files[MAX_FILES];
    return get_files_by_ss(ss_id, files, MAX_FILES);
}