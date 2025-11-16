#ifndef FILE_REGISTRY_H
#define FILE_REGISTRY_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include "../../include/constants.h"


/**
 * File information stored by NS
 */
typedef struct FileInfo {
    char file_path[256];            // Full path (unique key)
    int ss_id;                      // Which SS has this file
    char ss_ip[16];                 // SS IP address
    int ss_client_port;             // SS client port
    int ss_nm_port;                 // SS NM port, to look up for the ss
    time_t created_at;              // When file was created
    time_t last_accessed;           // Last access time
    char* owner;                    // Owner ID
    bool is_active;                 // Is file still available?
} FileInfo;

/**
 * Hash table node for chaining
 */
typedef struct FileHashNode {
    char* key;                      // File path (key)
    FileInfo* value;                // Pointer to file info
    struct FileHashNode* next;      // Next node in chain
} FileHashNode;

/**
 * Hash table for file registry
 */
typedef struct {
    FileHashNode* buckets[FILE_HASH_TABLE_SIZE];
    int count;                      // Number of registered files
    pthread_mutex_t mutex;          // Thread safety
} FileHashTable;

/**
 * Global file registry
 */
extern FileHashTable file_registry;

/**
 * Initialize the file registry
 */
void init_file_registry();

/**
 * Register a new file in the registry
 * @param file_path Full path of the file
 * @param ss_id Storage server ID
 * @param ss_ip Storage server IP
 * @param ss_client_port Storage server client port
 * @return 0 on success, -1 on failure
 */
int register_file(const char* file_path, int ss_id, const char* ss_ip, int ss_client_port, int ss_nm_port);

/**
 * Find file by path (O(1) average case)
 * @param file_path Full path of the file
 * @return Pointer to FileInfo or NULL if not found
 */
FileInfo* find_file(const char* file_path);

/**
 * Remove file from registry
 * @param file_path Full path of the file
 * @return 0 on success, -1 if not found
 */
int unregister_file(const char* file_path);

/**
 * Get all files managed by a specific SS
 * @param ss_id Storage server ID
 * @param files Array to store pointers to FileInfo (caller allocates)
 * @param max_size Maximum size of array
 * @return Number of files found
 */
int get_files_by_ss(int ss_id, FileInfo** files, int max_size);

/**
 * Mark file as inactive (when SS goes down)
 * @param file_path Full path of the file
 */
void mark_file_inactive(const char* file_path);

/**
 * Cleanup registry and free all resources
 */
void cleanup_file_registry();

#endif // FILE_REGISTRY_H