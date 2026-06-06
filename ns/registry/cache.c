#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Global LRU cache
static LRUCache cache;

/**
 * Move entry to head (most recently used)
 */
static void move_to_head(CacheEntry* entry) {
    if (entry == cache.head) return;  // Already at head
    
    // Remove from current position
    if (entry->prev) entry->prev->next = entry->next;
    if (entry->next) entry->next->prev = entry->prev;
    
    if (entry == cache.tail) {
        cache.tail = entry->prev;
    }
    
    // Insert at head
    entry->next = cache.head;
    entry->prev = NULL;
    
    if (cache.head) {
        cache.head->prev = entry;
    }
    
    cache.head = entry;
    
    if (!cache.tail) {
        cache.tail = entry;
    }
}

/**
 * Remove tail (least recently used)
 */
static CacheEntry* remove_tail() {
    if (!cache.tail) return NULL;
    
    CacheEntry* tail = cache.tail;
    
    if (tail->prev) {
        tail->prev->next = NULL;
    }
    
    cache.tail = tail->prev;
    
    if (cache.head == tail) {
        cache.head = NULL;
    }
    
    tail->prev = NULL;
    tail->next = NULL;
    
    return tail;
}

/**
 * Initialize cache
 */
void init_cache() {
    cache.entries = calloc(CACHE_SIZE, sizeof(CacheEntry));
    cache.head = NULL;
    cache.tail = NULL;
    cache.size = 0;
    cache.capacity = CACHE_SIZE;
    cache.total_hits = 0;
    cache.total_misses = 0;
    cache.total_evictions = 0;
    pthread_mutex_init(&cache.mutex, NULL);
    
    // Initialize all entries as invalid
    for (int i = 0; i < CACHE_SIZE; i++) {
        cache.entries[i].valid = false;
    }
    
    printf("[Cache] LRU cache initialized (capacity: %d entries)\n", CACHE_SIZE);
}

/**
 * Get file from cache
 */
FileInfo* cache_get_file(const char* file_path) {
    if (!file_path) return NULL;
    
    pthread_mutex_lock(&cache.mutex);
    
    // Linear search through valid entries
    for (int i = 0; i < CACHE_SIZE; i++) {
        CacheEntry* entry = &cache.entries[i];
        
        if (entry->valid && strcmp(entry->key, file_path) == 0) {
            // Cache hit
            cache.total_hits++;
            entry->last_accessed = time(NULL);
            entry->access_count++;
            
            // Move to head (most recently used)
            move_to_head(entry);
            
            FileInfo* result = entry->data;
            pthread_mutex_unlock(&cache.mutex);
            
            return result;
        }
    }
    
    // Cache miss
    cache.total_misses++;
    pthread_mutex_unlock(&cache.mutex);
    
    return NULL;
}

/**
 * Put file into cache
 */
void cache_put_file(const char* file_path, FileInfo* file) {
    if (!file_path || !file) return;
    
    pthread_mutex_lock(&cache.mutex);
    
    // Check if already exists (update case)
    for (int i = 0; i < CACHE_SIZE; i++) {
        CacheEntry* entry = &cache.entries[i];
        
        if (entry->valid && strcmp(entry->key, file_path) == 0) {
            // Update existing entry
            entry->data = file;
            entry->last_accessed = time(NULL);
            entry->access_count++;
            move_to_head(entry);
            pthread_mutex_unlock(&cache.mutex);
            return;
        }
    }
    
    // Find empty slot or evict LRU
    CacheEntry* new_entry = NULL;
    
    if (cache.size < cache.capacity) {
        // Find first invalid entry
        for (int i = 0; i < CACHE_SIZE; i++) {
            if (!cache.entries[i].valid) {
                new_entry = &cache.entries[i];
                cache.size++;
                break;
            }
        }
    } else {
        // Cache full - evict LRU (tail)
        new_entry = remove_tail();
        cache.total_evictions++;
        
        printf("[Cache] Evicted '%s' (accessed %lu times)\n", 
               new_entry->key, new_entry->access_count);
    }
    
    if (!new_entry) {
        pthread_mutex_unlock(&cache.mutex);
        fprintf(stderr, "[Cache ERROR] Failed to allocate cache entry\n");
        return;
    }
    
    // Fill new entry
    strncpy(new_entry->key, file_path, CACHE_KEY_SIZE - 1);
    new_entry->key[CACHE_KEY_SIZE - 1] = '\0';
    new_entry->data = file;
    new_entry->last_accessed = time(NULL);
    new_entry->access_count = 1;
    new_entry->valid = true;
    
    // Add to head
    move_to_head(new_entry);
    
    pthread_mutex_unlock(&cache.mutex);
}

/**
 * Invalidate cache entry
 */
void cache_invalidate_file(const char* file_path) {
    if (!file_path) return;
    
    pthread_mutex_lock(&cache.mutex);
    
    for (int i = 0; i < CACHE_SIZE; i++) {
        CacheEntry* entry = &cache.entries[i];
        
        if (entry->valid && strcmp(entry->key, file_path) == 0) {
            // Remove from LRU list
            if (entry->prev) entry->prev->next = entry->next;
            if (entry->next) entry->next->prev = entry->prev;
            
            if (entry == cache.head) cache.head = entry->next;
            if (entry == cache.tail) cache.tail = entry->prev;
            
            // Mark invalid
            entry->valid = false;
            entry->data = NULL;
            entry->prev = NULL;
            entry->next = NULL;
            
            cache.size--;
            
            printf("[Cache] Invalidated '%s'\n", file_path);
            pthread_mutex_unlock(&cache.mutex);
            return;
        }
    }
    
    pthread_mutex_unlock(&cache.mutex);
}

/**
 * Clear entire cache
 */
void cache_clear() {
    pthread_mutex_lock(&cache.mutex);
    
    for (int i = 0; i < CACHE_SIZE; i++) {
        cache.entries[i].valid = false;
        cache.entries[i].data = NULL;
        cache.entries[i].prev = NULL;
        cache.entries[i].next = NULL;
    }
    
    cache.head = NULL;
    cache.tail = NULL;
    cache.size = 0;
    
    printf("[Cache] Cleared all entries\n");
    pthread_mutex_unlock(&cache.mutex);
}

/**
 * Get cache statistics
 */
CacheStats get_cache_stats() {
    CacheStats stats;
    
    pthread_mutex_lock(&cache.mutex);
    
    stats.hits = cache.total_hits;
    stats.misses = cache.total_misses;
    stats.evictions = cache.total_evictions;
    
    uint64_t total_requests = stats.hits + stats.misses;
    stats.hit_rate = (total_requests > 0) ? 
                     ((double)stats.hits / total_requests) : 0.0;
    
    pthread_mutex_unlock(&cache.mutex);
    
    return stats;
}

/**
 * Print cache statistics
 */
void print_cache_stats() {
    CacheStats stats = get_cache_stats();
    
    printf("\n╔═══════════════════════════════════════════╗\n");
    printf("║       Cache Statistics                    ║\n");
    printf("╠═══════════════════════════════════════════╣\n");
    printf("║ Capacity:    %3d entries                  ║\n", cache.capacity);
    printf("║ Current:     %3d entries                  ║\n", cache.size);
    printf("║ Hits:        %10lu                   ║\n", stats.hits); 
    printf("║ Misses:      %10lu                   ║\n", stats.misses);
    printf("║ Evictions:   %10lu                   ║\n", stats.evictions);
    printf("║ Hit Rate:    %6.2f%%                      ║\n", stats.hit_rate * 100);
    printf("╚═══════════════════════════════════════════╝\n\n");
}

/**
 * Cleanup cache
 */
void cleanup_cache() {
    pthread_mutex_lock(&cache.mutex);
    
    free(cache.entries);
    cache.entries = NULL;
    cache.head = NULL;
    cache.tail = NULL;
    cache.size = 0;
    
    pthread_mutex_unlock(&cache.mutex);
    pthread_mutex_destroy(&cache.mutex);
    
    printf("[Cache] Cleanup complete\n");
}