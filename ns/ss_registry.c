#include "ss_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Global storage server registry
SSHashTable ss_registry;




/**
 * Hash function - uses IP address and port combination
 */
static unsigned int hash_ss_key(const char* ip_address, int nm_port) {
    unsigned int hash = 5381;
    const char* str = ip_address;
    while (*str) {
        hash = ((hash << 5) + hash) + (unsigned char)(*str++);
    }
    hash = ((hash << 5) + hash) + (unsigned int)nm_port;
    return hash % HASH_TABLE_SIZE;
}



/**
 * Hash function - simple modulo hash
 */
static unsigned int hash(int key) {
    return (unsigned int)key % HASH_TABLE_SIZE;
}




/**
 * Initialize the storage server registry
 */
void init_ss_registry() {
    memset(&ss_registry, 0, sizeof(SSHashTable));
    pthread_mutex_init(&ss_registry.mutex, NULL);
    printf("[SS-Registry] Hash table initialized (size: %d)\n", HASH_TABLE_SIZE);
}



/**
 * Find storage server by IP and port
 */
StorageServerInfo* find_storage_server_by_address(const char* ip_address, int nm_port) {
    pthread_mutex_lock(&ss_registry.mutex);
    
    unsigned int index = hash_ss_key(ip_address, nm_port);
    SSHashNode* current = ss_registry.buckets[index];
    
    while (current != NULL) {
        if (strcmp(current->value->ip_address, ip_address) == 0 && 
            current->value->nm_port == nm_port) {
            StorageServerInfo* result = current->value;
            pthread_mutex_unlock(&ss_registry.mutex);
            return result;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&ss_registry.mutex);
    return NULL;
}





/**
 * Register a new storage server in the registry or reconnect an existing one - O(1) average case
 */
int register_or_reconnect_storage_server(StorageServerInfo* ss_info) {
    if (!ss_info) return -1;
    
    pthread_mutex_lock(&ss_registry.mutex);
    
    // Check if SS already exists by IP+Port
    unsigned int index = hash_ss_key(ss_info->ip_address, ss_info->nm_port);
    SSHashNode* current = ss_registry.buckets[index];
    
    while (current != NULL) {
        if (strcmp(current->value->ip_address, ss_info->ip_address) == 0 && 
            current->value->nm_port == ss_info->nm_port) {
            
            // RECONNECTION - Update existing entry
            StorageServerInfo* existing = current->value;
            existing->ss_fd = ss_info->ss_fd;
            existing->is_active = true;
            existing->thread_id = ss_info->thread_id;
            existing->client_port = ss_info->client_port;
            existing->last_connected = time(NULL);
            existing->reconnect_count++;
            
            int ss_id = existing->ss_id;
            pthread_mutex_unlock(&ss_registry.mutex);
            
            printf("[SS-Registry] ✓ SS #%d RECONNECTED (%s:%d) - Reconnect #%d\n", 
                   ss_id, existing->ip_address, existing->nm_port, existing->reconnect_count);
            
            return ss_id;
        }
        current = current->next;
    }
    
    // NEW REGISTRATION
    if (ss_registry.count >= MAX_STORAGE_SERVERS) {
        pthread_mutex_unlock(&ss_registry.mutex);
        fprintf(stderr, "[SS-Registry ERROR] Maximum storage servers reached\n");
        return -1;
    }
    
    SSHashNode* new_node = malloc(sizeof(SSHashNode));
    if (!new_node) {
        pthread_mutex_unlock(&ss_registry.mutex);
        return -1;
    }
    
    new_node->value = malloc(sizeof(StorageServerInfo));
    if (!new_node->value) {
        free(new_node);
        pthread_mutex_unlock(&ss_registry.mutex);
        return -1;
    }
    
    memcpy(new_node->value, ss_info, sizeof(StorageServerInfo));
    new_node->key = ss_info->ss_id;
    new_node->value->first_connected = time(NULL);
    new_node->value->last_connected = time(NULL);
    new_node->value->reconnect_count = 0;
    
    new_node->next = ss_registry.buckets[index];
    ss_registry.buckets[index] = new_node;
    
    ss_registry.count++;
    
    pthread_mutex_unlock(&ss_registry.mutex);
    
    printf("[SS-Registry] > NEW SS #%d registered (%s:%d) at bucket %u (Total: %d)\n", 
           ss_info->ss_id, ss_info->ip_address, ss_info->nm_port, index, ss_registry.count);
    
    return ss_info->ss_id;
}


/**
 * Find storage server by ID - O(1) average case
 */
StorageServerInfo* find_storage_server(int ss_id) {
    pthread_mutex_lock(&ss_registry.mutex);
    
    unsigned int index = hash(ss_id);
    SSHashNode* current = ss_registry.buckets[index];
    
    // Traverse chain to find matching key
    while (current != NULL) {
        if (current->key == ss_id) {
            StorageServerInfo* result = current->value;
            pthread_mutex_unlock(&ss_registry.mutex);
            return result;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&ss_registry.mutex);
    return NULL;  // Not found
}

/**
 * Remove storage server from registry
 */
int unregister_storage_server(int ss_id) {
    pthread_mutex_lock(&ss_registry.mutex);
    
    unsigned int index = hash(ss_id);
    SSHashNode* current = ss_registry.buckets[index];
    SSHashNode* prev = NULL;
    
    // Find and remove node
    while (current != NULL) {
        if (current->key == ss_id) {
            // Remove from chain
            if (prev == NULL) {
                ss_registry.buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }
            
            // Free memory
            if (current->value->accessible_paths) {
                for (int i = 0; i < current->value->num_paths; i++) {
                    free(current->value->accessible_paths[i]);
                }
                free(current->value->accessible_paths);
            }
            free(current->value);
            free(current);
            
            ss_registry.count--;
            
            pthread_mutex_unlock(&ss_registry.mutex);
            
            printf("[SS-Registry] Unregistered SS #%d (Total: %d)\n", 
                   ss_id, ss_registry.count);
            
            return 0;
        }
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&ss_registry.mutex);
    return -1;  // Not found
}

/**
 * Get all registered storage servers
 */
int get_all_storage_servers(StorageServerInfo** servers, int max_size) {
    pthread_mutex_lock(&ss_registry.mutex);
    
    int count = 0;
    for (int i = 0; i < HASH_TABLE_SIZE && count < max_size; i++) {
        SSHashNode* current = ss_registry.buckets[i];
        while (current != NULL && count < max_size) {
            servers[count++] = current->value;
            current = current->next;
        }
    }
    
    pthread_mutex_unlock(&ss_registry.mutex);
    return count;
}

/**
 * Mark storage server as inactive
 */
void mark_ss_inactive(int ss_id) {
    StorageServerInfo* ss = find_storage_server(ss_id);
    if (ss) {
        pthread_mutex_lock(&ss_registry.mutex);
        ss->is_active = false;
        pthread_mutex_unlock(&ss_registry.mutex);
        printf("[SS-Registry] Marked SS #%d as inactive\n", ss_id);
    }
}

/**
 * Cleanup registry and free all resources
 */
void cleanup_ss_registry() {
    pthread_mutex_lock(&ss_registry.mutex);
    
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        SSHashNode* current = ss_registry.buckets[i];
        while (current != NULL) {
            SSHashNode* temp = current;
            current = current->next;
            
            if (temp->value->accessible_paths) {
                for (int j = 0; j < temp->value->num_paths; j++) {
                    free(temp->value->accessible_paths[j]);
                }
                free(temp->value->accessible_paths);
            }
            free(temp->value);
            free(temp);
        }
        ss_registry.buckets[i] = NULL;
    }
    
    ss_registry.count = 0;
    
    pthread_mutex_unlock(&ss_registry.mutex);
    pthread_mutex_destroy(&ss_registry.mutex);
    
    printf("[SS-Registry] Cleanup complete\n");
}