#include "file_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

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


    // Insert at head
    new_node->next = file_registry.buckets[index];
    file_registry.buckets[index] = new_node;
    
    file_registry.count++;
    
    pthread_mutex_unlock(&file_registry.mutex);
    
    printf("[File-Registry] ✓ Registered file '%s' on SS #%d at bucket %u (Total: %d)\n",
           file_path, ss_id, index, file_registry.count);
    
    return 0;
}

/**
 * Find file by path - O(1) average case
 */
FileInfo* find_file(const char* file_path) {
    if (!file_path) return NULL;
    
    pthread_mutex_lock(&file_registry.mutex);
    
    unsigned int index = hash_file_path(file_path);
    FileHashNode* current = file_registry.buckets[index];
    
    while (current != NULL) {
        if (strcmp(current->key, file_path) == 0) {
            FileInfo* result = current->value;
            result->last_accessed = time(NULL);
            pthread_mutex_unlock(&file_registry.mutex);
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
            free(current->key);
            free(current->value);
            free(current);
            
            file_registry.count--;
            
            pthread_mutex_unlock(&file_registry.mutex);
            
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
            free(temp->key);
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