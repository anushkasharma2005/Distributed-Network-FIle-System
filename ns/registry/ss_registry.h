#ifndef SS_REGISTRY_H
#define SS_REGISTRY_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#include "../../include/constants.h"


/**
 * Storage Server information stored by NS
 */
typedef struct StorageServerInfo{
    int ss_id;                      // Unique ID
    int ss_fd;                      // Socket file descriptor
    char ip_address[16];            // IP address
    int nm_port;                    // Port for NS-SS communication
    int client_port;                // Port for client-SS communication
    bool is_active;                 // Connection status
    char **accessible_paths;        // Array of accessible paths. asccessible strings are just file paths
    int num_paths;                  // Number of paths of files stored
    pthread_t thread_id;            // Handler thread ID, id of the thread managing this SS
    time_t first_connected;         // Time when the server first connected
    time_t last_connected;          // Time when the server last connected
    int reconnect_count;            // Number of reconnections
} StorageServerInfo;

/**
 * Hash table node for chaining (collision handling)
 */
typedef struct SSHashNode {
    int key;                        // SS ID (key)
    StorageServerInfo* value;       // Pointer to SS info
    struct SSHashNode* next;        // Next node in chain
} SSHashNode;

/**
 * Hash table for storage server registry
 */
typedef struct {
    SSHashNode* buckets[HASH_TABLE_SIZE];
    int count;                      // Number of registered servers
    pthread_mutex_t mutex;          // Thread safety
} SSHashTable;

/**
 * Global storage server registry (hash table)
 */
extern SSHashTable ss_registry;

/**
 * Initialize the storage server registry
 */
void init_ss_registry();

/**
 * Find storage server by ID (O(1) average case)
 * @param ss_id Storage server ID
 * @return Pointer to StorageServerInfo or NULL if not found
 */
int register_or_reconnect_storage_server(StorageServerInfo* ss_info);

/**
 * Remove storage server from registry
 * @param ss_id Storage server ID to remove
 * @return 0 on success, -1 if not found
 */
int unregister_storage_server(int ss_id);

/**
 * Get all registered storage servers
 * @param servers Array to store pointers to SS info (caller allocates)
 * @param max_size Maximum size of array
 * @return Number of servers copied
 */

 int get_all_storage_servers(StorageServerInfo** servers, int max_size);

/**  
 * Find storage server by IP address and NM port
 * @param ip_address IP address of the storage server
 * @param nm_port NM port of the storage server
 * @return Pointer to StorageServerInfo or NULL if not found
 */
StorageServerInfo* find_storage_server_by_address(const char* ip_address, int nm_port);

/**
 * Find storage server by ID
 * @param ss_id Storage server ID
 * @return Pointer to StorageServerInfo or NULL if not found
 */
StorageServerInfo* find_storage_server(int ss_id);


/**
 * Mark storage server as inactive
 * @param ss_id Storage server ID
 */
void mark_ss_inactive(int ss_id);

/**
 * Cleanup registry and free all resources
 */
void cleanup_ss_registry();

#endif // SS_REGISTRY_H