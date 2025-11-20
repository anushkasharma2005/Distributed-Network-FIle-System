#include "file_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include "ss_registry.h"
#include "../../api_ns_ss/ns_ss_connection.h"
#include "../../api_c_ns/networking.h"
#include "cache.h"


// Global file registry
FileHashTable file_registry;

/**
 * Hash function for file paths (djb2 algorithm)
 */
static unsigned int hash_file_path(const char* str) {
    unsigned int hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash % FILE_HASH_TABLE_SIZE;
}

/**
 * Initialize the file registry
 */
void init_file_registry() {
    memset(&file_registry, 0, sizeof(FileHashTable));
    pthread_mutex_init(&file_registry.mutex, NULL);
    printf("[File-Registry] Hash table initialized (size: %d)\n", FILE_HASH_TABLE_SIZE);
}

/**
 * Register a new file
 */
int register_file(const char* file_path, int ss_id, const char* ss_ip, int ss_client_port, int ss_nm_port, const char* owner) {
    if (!file_path || !ss_ip || !owner) return -1;

    pthread_mutex_lock(&file_registry.mutex);
    
    // Check if file already exists
    unsigned int index = hash_file_path(file_path);
    FileHashNode* current = file_registry.buckets[index];
    
    while (current != NULL) {
        if (strcmp(current->key, file_path) == 0) {
            pthread_mutex_unlock(&file_registry.mutex);
            fprintf(stderr, "[File-Registry ERROR] File already exists: %s\n", file_path);
            return -1;
        }
        current = current->next;
    }
    
    // Create new node
    FileHashNode* new_node = malloc(sizeof(FileHashNode));
    if (!new_node) {
        pthread_mutex_unlock(&file_registry.mutex);
        return -1;
    }
    
    new_node->key = strdup(file_path);
    new_node->value = malloc(sizeof(FileInfo));
    if (!new_node->key || !new_node->value) {
        free(new_node->key);
        free(new_node->value);
        free(new_node);
        pthread_mutex_unlock(&file_registry.mutex);
        return -1;
    }
    
    // Fill file info
    strncpy(new_node->value->file_path, file_path, sizeof(new_node->value->file_path) - 1);
    new_node->value->ss_id = ss_id;
    strncpy(new_node->value->ss_ip, ss_ip, sizeof(new_node->value->ss_ip) - 1);
    new_node->value->ss_client_port = ss_client_port;
    new_node->value->created_at = time(NULL);
    new_node->value->last_accessed = time(NULL);
    new_node->value->is_active = true;
    new_node->value->ss_nm_port = ss_nm_port;
    new_node->value->owner = strdup(owner);
    new_node->value->deleted_at = 0;

    new_node->value->read_users = calloc(MAX_ACCESS_USERS, sizeof(char*));
    new_node->value->write_users = calloc(MAX_ACCESS_USERS, sizeof(char*));
    
    if (!new_node->value->read_users || !new_node->value->write_users) {
        free(new_node->value->read_users);
        free(new_node->value->write_users);
        free(new_node->value->owner);
        free(new_node->value);
        free(new_node->key);
        free(new_node);
        pthread_mutex_unlock(&file_registry.mutex);
        return -1;
    }
    
    new_node->value->read_count = 0;
    new_node->value->read_capacity = MAX_ACCESS_USERS;
    new_node->value->write_count = 0;
    new_node->value->write_capacity = MAX_ACCESS_USERS;
    new_node->value->word_count = -1;
    new_node->value->char_count = -1;
    
    
    // Insert at head
    new_node->next = file_registry.buckets[index];
    file_registry.buckets[index] = new_node;
    
    file_registry.count++;
    
    pthread_mutex_unlock(&file_registry.mutex);
    

    // Invalidate cache (new file registered)
    cache_invalidate_file(file_path);


    printf("[File-Registry] ✓ Registered file '%s' on SS #%d at bucket %u (Total: %d)\n",
           file_path, ss_id, index, file_registry.count);
    
    return 0;
}

/**
 * Find file by path - O(1) average case
 */
FileInfo* find_file(const char* file_path) {
    if (!file_path) return NULL;
    

    // TRY CACHE FIRST
    FileInfo* cached = cache_get_file(file_path);
    if (cached) {
        // Update last accessed time even for cached lookups
        pthread_mutex_lock(&file_registry.mutex);
        cached->last_accessed = time(NULL);
        pthread_mutex_unlock(&file_registry.mutex);
        return cached;
    }

    // CACHE MISS - do actual search
    pthread_mutex_lock(&file_registry.mutex);
    
    unsigned int index = hash_file_path(file_path);
    FileHashNode* current = file_registry.buckets[index];
    
    while (current != NULL) {
        if (strcmp(current->key, file_path) == 0) {
            FileInfo* result = current->value;
            result->last_accessed = time(NULL);
            pthread_mutex_unlock(&file_registry.mutex);
            
            // STORE IN CACHE for next time
            cache_put_file(file_path, result);

            return result;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&file_registry.mutex);
    return NULL;
}

/**
 * Remove file from registry
 */
int unregister_file(const char* file_path) {
    if (!file_path) return -1;
    
    pthread_mutex_lock(&file_registry.mutex);
    
    unsigned int index = hash_file_path(file_path);
    FileHashNode* current = file_registry.buckets[index];
    FileHashNode* prev = NULL;
    
    while (current != NULL) {
        if (strcmp(current->key, file_path) == 0) {
            // Remove from chain
            if (prev == NULL) {
                file_registry.buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }
            
            // Free memory
            for (int i = 0; i < current->value->read_count; i++) {
                free(current->value->read_users[i]);
            }
            free(current->value->read_users);
            
            for (int i = 0; i < current->value->write_count; i++) {
                free(current->value->write_users[i]);
            }
            free(current->value->write_users);
            
            free(current->key);
            free(current->value->owner);
            free(current->value);
            free(current);
    
            
            file_registry.count--;
            
            pthread_mutex_unlock(&file_registry.mutex);
            
            // Invalidate cache
            cache_invalidate_file(file_path);

            printf("[File-Registry] Unregistered file '%s' (Total: %d)\n",
                   file_path, file_registry.count);
            
            return 0;
        }
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&file_registry.mutex);
    return -1;
}

/**
 * Get all files by SS
 */
int get_files_by_ss(int ss_id, FileInfo** files, int max_size) {
    pthread_mutex_lock(&file_registry.mutex);
    
    int count = 0;
    for (int i = 0; i < FILE_HASH_TABLE_SIZE && count < max_size; i++) {
        FileHashNode* current = file_registry.buckets[i];
        while (current != NULL && count < max_size) {
            if (current->value->ss_id == ss_id) {
                files[count++] = current->value;
            }
            current = current->next;
        }
    }
    
    pthread_mutex_unlock(&file_registry.mutex);
    return count;
}

/**
 * Mark file as inactive
 */
void mark_file_inactive(const char* file_path) {
    FileInfo* file = find_file(file_path);
    if (file) {
        pthread_mutex_lock(&file_registry.mutex);
        file->is_active = false;
        pthread_mutex_unlock(&file_registry.mutex);
        printf("[File-Registry] Marked file '%s' as inactive\n", file_path);
    }
}

/**
 * Cleanup registry
 */
void cleanup_file_registry() {
    pthread_mutex_lock(&file_registry.mutex);
    
    for (int i = 0; i < FILE_HASH_TABLE_SIZE; i++) {
        FileHashNode* current = file_registry.buckets[i];
        while (current != NULL) {
            FileHashNode* temp = current;
            current = current->next;

            for (int j = 0; j < temp->value->read_count; j++) {
                free(temp->value->read_users[j]);
            }
            free(temp->value->read_users);
            
            for (int j = 0; j < temp->value->write_count; j++) {
                free(temp->value->write_users[j]);
            }
            free(temp->value->write_users);
            
            free(temp->key);
            free(temp->value->owner);
            free(temp->value);
            free(temp);
        }
        file_registry.buckets[i] = NULL;
    }
    
    file_registry.count = 0;
    
    pthread_mutex_unlock(&file_registry.mutex);
    pthread_mutex_destroy(&file_registry.mutex);
    
    printf("[File-Registry] Cleanup complete\n");
}



/**
 * Add read access for a user
 */
int add_read_access(const char* file_path, const char* username) {
    if (!file_path || !username) return -1;
    
    FileInfo* file = find_file(file_path);
    if (!file) {
        fprintf(stderr, "[File-Registry] File not found: %s\n", file_path);
        return -1;
    }
    
    pthread_mutex_lock(&file_registry.mutex);
    
    // Check if user is owner
    if (strcmp(file->owner, username) == 0) {
        pthread_mutex_unlock(&file_registry.mutex);
        printf("[File-Registry] User '%s' is already owner of '%s'\n", username, file_path);
        return 0;
    }
    
    // Check if already in write_users (write includes read)
    for (int i = 0; i < file->write_count; i++) {
        if (strcmp(file->write_users[i], username) == 0) {
            pthread_mutex_unlock(&file_registry.mutex);
            printf("[File-Registry] User '%s' already has write access to '%s'\n", username, file_path);
            return 0;
        }
    }
    
    // Check if already in read_users
    for (int i = 0; i < file->read_count; i++) {
        if (strcmp(file->read_users[i], username) == 0) {
            pthread_mutex_unlock(&file_registry.mutex);
            printf("[File-Registry] User '%s' already has read access to '%s'\n", username, file_path);
            return 0;
        }
    }
    
    // Add to read_users
    if (file->read_count >= file->read_capacity) {
        pthread_mutex_unlock(&file_registry.mutex);
        fprintf(stderr, "[File-Registry] Max read users reached for '%s'\n", file_path);
        return -1;
    }
    
    file->read_users[file->read_count] = strdup(username);
    if (!file->read_users[file->read_count]) {
        pthread_mutex_unlock(&file_registry.mutex);
        return -1;
    }
    
    file->read_count++;
    
    pthread_mutex_unlock(&file_registry.mutex);
    
    printf("[File-Registry] ✓ Added read access for '%s' to '%s'\n", username, file_path);
    return 0;
}

/**
 * Add write access for a user (includes read)
 */
int add_write_access(const char* file_path, const char* username) {
    if (!file_path || !username) return -1;
    
    FileInfo* file = find_file(file_path);
    if (!file) {
        fprintf(stderr, "[File-Registry] File not found: %s\n", file_path);
        return -1;
    }
    
    pthread_mutex_lock(&file_registry.mutex);
    
    // Check if user is owner
    if (strcmp(file->owner, username) == 0) {
        pthread_mutex_unlock(&file_registry.mutex);
        printf("[File-Registry] User '%s' is already owner of '%s'\n", username, file_path);
        return 0;
    }
    
    // Check if already in write_users
    for (int i = 0; i < file->write_count; i++) {
        if (strcmp(file->write_users[i], username) == 0) {
            pthread_mutex_unlock(&file_registry.mutex);
            printf("[File-Registry] User '%s' already has write access to '%s'\n", username, file_path);
            return 0;
        }
    }
    
    // Remove from read_users if present (upgrading to write)
    for (int i = 0; i < file->read_count; i++) {
        if (strcmp(file->read_users[i], username) == 0) {
            free(file->read_users[i]);
            // Shift remaining elements
            for (int j = i; j < file->read_count - 1; j++) {
                file->read_users[j] = file->read_users[j + 1];
            }
            file->read_count--;
            printf("[File-Registry] Upgraded '%s' from read to write access\n", username);
            break;
        }
    }
    
    // Add to write_users
    if (file->write_count >= file->write_capacity) {
        pthread_mutex_unlock(&file_registry.mutex);
        fprintf(stderr, "[File-Registry] Max write users reached for '%s'\n", file_path);
        return -1;
    }
    
    file->write_users[file->write_count] = strdup(username);
    if (!file->write_users[file->write_count]) {
        pthread_mutex_unlock(&file_registry.mutex);
        return -1;
    }
    
    file->write_count++;
    
    pthread_mutex_unlock(&file_registry.mutex);
    
    printf("[File-Registry] ✓ Added write access for '%s' to '%s'\n", username, file_path);
    return 0;
}

/**
 * Remove all access for a user
 */
int remove_access(const char* file_path, const char* username) {
    if (!file_path || !username) return -1;
    
    FileInfo* file = find_file(file_path);
    if (!file) {
        fprintf(stderr, "[File-Registry] File not found: %s\n", file_path);
        return -1;
    }
    
    pthread_mutex_lock(&file_registry.mutex);
    
    // Cannot remove owner's access
    if (strcmp(file->owner, username) == 0) {
        pthread_mutex_unlock(&file_registry.mutex);
        fprintf(stderr, "[File-Registry] Cannot remove owner's access\n");
        return -1;
    }
    
    bool found = false;
    
    // Remove from read_users
    for (int i = 0; i < file->read_count; i++) {
        if (strcmp(file->read_users[i], username) == 0) {
            free(file->read_users[i]);
            // Shift remaining elements
            for (int j = i; j < file->read_count - 1; j++) {
                file->read_users[j] = file->read_users[j + 1];
            }
            file->read_count--;
            found = true;
            break;
        }
    }
    
    // Remove from write_users
    for (int i = 0; i < file->write_count; i++) {
        if (strcmp(file->write_users[i], username) == 0) {
            free(file->write_users[i]);
            // Shift remaining elements
            for (int j = i; j < file->write_count - 1; j++) {
                file->write_users[j] = file->write_users[j + 1];
            }
            file->write_count--;
            found = true;
            break;
        }
    }
    
    pthread_mutex_unlock(&file_registry.mutex);
    
    if (found) {
        printf("[File-Registry] ✓ Removed access for '%s' from '%s'\n", username, file_path);
        return 0;
    } else {
        printf("[File-Registry] User '%s' had no access to '%s'\n", username, file_path);
        return -1;
    }
}

/**
 * Check if user has read access to file
 */
bool has_read_access(const char* file_path, const char* username) {
    if (!file_path || !username) return false;
    
    FileInfo* file = find_file(file_path);
    if (!file || !file->is_active) return false;
    
    pthread_mutex_lock(&file_registry.mutex);
    
    // Owner always has access
    if (strcmp(file->owner, username) == 0) {
        pthread_mutex_unlock(&file_registry.mutex);
        return true;
    }
    
    // Check read_users
    for (int i = 0; i < file->read_count; i++) {
        if (strcmp(file->read_users[i], username) == 0) {
            pthread_mutex_unlock(&file_registry.mutex);
            return true;
        }
    }
    
    // Check write_users (write includes read)
    for (int i = 0; i < file->write_count; i++) {
        if (strcmp(file->write_users[i], username) == 0) {
            pthread_mutex_unlock(&file_registry.mutex);
            return true;
        }
    }
    
    pthread_mutex_unlock(&file_registry.mutex);
    return false;
}

/**
 * Check if user has write access to file
 */
bool has_write_access(const char* file_path, const char* username) {
    if (!file_path || !username) return false;
    
    FileInfo* file = find_file(file_path);
    if (!file || !file->is_active) return false;
    
    pthread_mutex_lock(&file_registry.mutex);
    
    // Owner always has write access
    if (strcmp(file->owner, username) == 0) {
        pthread_mutex_unlock(&file_registry.mutex);
        return true;
    }
    
    // Check write_users
    for (int i = 0; i < file->write_count; i++) {
        if (strcmp(file->write_users[i], username) == 0) {
            pthread_mutex_unlock(&file_registry.mutex);
            return true;
        }
    }
    
    pthread_mutex_unlock(&file_registry.mutex);
    return false;
}

/**
 * Check if user is the owner of file
 */
bool is_file_owner(const char* file_path, const char* username) {
    if (!file_path || !username) return false;
    
    FileInfo* file = find_file(file_path);
    if (!file) return false;
    
    pthread_mutex_lock(&file_registry.mutex);
    bool is_owner = (strcmp(file->owner, username) == 0);
    pthread_mutex_unlock(&file_registry.mutex);
    
    return is_owner;
}

/**
 * Get formatted access list for INFO command
 */
int format_access_list(FileInfo* file_info, char* buffer, size_t buffer_size) {
    if (!file_info || !buffer || buffer_size == 0) return 0;
    
    int offset = 0;
    
    // Add owner
    offset += snprintf(buffer + offset, buffer_size - offset, "%s (RW)", file_info->owner);
    
    // Add write users
    for (int i = 0; i < file_info->write_count && offset < (int)buffer_size - 20; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset, ", %s (RW)", file_info->write_users[i]);
    }
    
    // Add read users
    for (int i = 0; i < file_info->read_count && offset < (int)buffer_size - 20; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset, ", %s (R)", file_info->read_users[i]);
    }
    
    return offset;
}

int get_accessible_files(const char* username, FileInfo** files, int max_size, bool include_all) {
    if (!files || max_size <= 0) return 0;
    
    // // ADD DEBUG
    // printf("[DEBUG][GET_FILES] Called with username='%s', include_all=%d\n", 
    //        username, include_all);
    
    pthread_mutex_lock(&file_registry.mutex);
    
    int count = 0;
    int total_active = 0;  // ADD: Count total active files
    
    // Scan all buckets
    for (int i = 0; i < FILE_HASH_TABLE_SIZE && count < max_size; i++) {
        FileHashNode* current = file_registry.buckets[i];
        
        while (current != NULL && count < max_size) {
            FileInfo* file = current->value;
            
            // // ADD DEBUG: Print each file examined
            // printf("[DEBUG][GET_FILES] Examining file: '%s', is_active=%d, owner='%s'\n",
            //        file->file_path, file->is_active, file->owner);
            
            // Skip inactive files
            if (!file->is_active) {
                // printf("[DEBUG][GET_FILES]   -> Skipped (inactive)\n");
                current = current->next;
                continue;
            }
            
            total_active++;  // ADD: Count active files
            
            // If include_all flag, add all active files
            if (include_all) {
                // printf("[DEBUG][GET_FILES]   -> Added (include_all=true)\n");
                files[count++] = file;
                current = current->next;
                continue;
            }
            
            // Otherwise, check if user has access
            bool has_access = false;
            
            // Check if owner
            if (strcmp(file->owner, username) == 0) {
                has_access = true;
                // printf("[DEBUG][GET_FILES]   -> Has access (owner)\n");
            }
            
            // Check read_users
            if (!has_access) {
                for (int j = 0; j < file->read_count; j++) {
                    if (strcmp(file->read_users[j], username) == 0) {
                        has_access = true;
                        // printf("[DEBUG][GET_FILES]   -> Has access (read_users)\n");
                        break;
                    }
                }
            }
            
            // Check write_users
            if (!has_access) {
                for (int j = 0; j < file->write_count; j++) {
                    if (strcmp(file->write_users[j], username) == 0) {
                        has_access = true;
                        // printf("[DEBUG][GET_FILES]   -> Has access (write_users)\n");
                        break;
                    }
                }
            }
            
            if (has_access) {
                files[count++] = file;
            } else {
                // printf("[DEBUG][GET_FILES]   -> Skipped (no access)\n");
            }
            
            current = current->next;
        }
    }
    
    pthread_mutex_unlock(&file_registry.mutex);
    
    // ADD DEBUG
    // printf("[DEBUG][GET_FILES] Returning %d files (total_active=%d, include_all=%d)\n",
        //    count, total_active, include_all);
    
    return count;
}



/**
 * Soft delete a file
 */
int soft_delete_file(const char* file_path) {
    if (!file_path) return -1;
    
    FileInfo* file = find_file(file_path);
    if (!file) {
        return -1;
    }
    
    pthread_mutex_lock(&file_registry.mutex);
    
    if (!file->is_active) {
        pthread_mutex_unlock(&file_registry.mutex);
        return -1;  // Already deleted
    }
    
    file->is_active = false;
    file->deleted_at = time(NULL);
    
    pthread_mutex_unlock(&file_registry.mutex);

    // Invalidate cache
    cache_invalidate_file(file_path);
    
    printf("[File-Registry] ✓ Soft deleted '%s' (can restore within %d minutes)\n",
           file_path, DELETE_EXPIRY_MINUTES);
    
    return 0;
}

/**
 * Restore a soft-deleted file
 */
int restore_file(const char* file_path) {
    if (!file_path) return -1;
    
    FileInfo* file = find_file(file_path);
    if (!file) {
        return -1;
    }
    
    pthread_mutex_lock(&file_registry.mutex);
    
    if (file->is_active) {
        pthread_mutex_unlock(&file_registry.mutex);
        return -1;  // Not deleted
    }
    
    // Check if expired
    if (is_delete_expired(file)) {
        pthread_mutex_unlock(&file_registry.mutex);
        printf("[File-Registry] ✗ Cannot restore '%s' - expired\n", file_path);
        return -2;  // Expired
    }
    
    file->is_active = true;
    file->deleted_at = 0;
    
    pthread_mutex_unlock(&file_registry.mutex);
    
    // Invalidate cache
    cache_invalidate_file(file_path);
    
    printf("[File-Registry] ✓ Restored file '%s'\n", file_path);
    
    return 0;
}

/**
 * Check if deleted file has expired
 */
bool is_delete_expired(FileInfo* file) {
    if (!file || file->is_active || file->deleted_at == 0) {
        return false;
    }
    
    time_t now = time(NULL);
    int minutes_elapsed = (now - file->deleted_at) / 60;
    
    return (minutes_elapsed >= DELETE_EXPIRY_MINUTES);
}

/**
 * Permanently delete file from NS and SS
 */
int permanently_delete_file(const char* file_path) {
    if (!file_path) return -1;
    
    FileInfo* file = find_file(file_path);
    if (!file) {
        return -1;
    }
    
    // Get SS info before unregistering
    StorageServerInfo* ss = find_storage_server_by_address(file->ss_ip, file->ss_nm_port);
    
    if (ss && ss->is_active) {
        // Send DELETE command to SS
        pthread_mutex_lock(&ss->socket_mutex);
        
        ProtocolMessage msg;
        memset(&msg, 0, sizeof(ProtocolMessage));
        msg.type = htonl(MSG_DELETE_FILE);
        strncpy(msg.data, file_path, sizeof(msg.data) - 1);
        
        send_protocol_message(ss->ss_fd, &msg);
        
        pthread_mutex_unlock(&ss->socket_mutex);
        
        printf("[File-Registry] Sent DELETE request to SS for '%s'\n", file_path);
    }
    
    // Remove from NS registry
    int result = unregister_file(file_path);
    
    if (result == 0) {
        printf("[File-Registry] ✓ Permanently deleted '%s'\n", file_path);
    }
    
    return result;
}