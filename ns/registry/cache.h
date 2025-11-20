#ifndef CACHE_H
#define CACHE_H

#include "file_registry.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#define CACHE_SIZE 100
#define CACHE_KEY_SIZE 512

/**
 * Cache entry structure
 */
typedef struct CacheEntry {
    char key[CACHE_KEY_SIZE];       // Search key (e.g., file path)
    FileInfo* data;                 // Cached FileInfo pointer
    time_t last_accessed;           // Last access timestamp
    uint64_t access_count;          // Number of times accessed
    bool valid;                     // Is this entry valid?
    struct CacheEntry* prev;        // LRU doubly-linked list
    struct CacheEntry* next;
} CacheEntry;

/**
 * Cache statistics
 */
typedef struct {
    uint64_t hits;                  // Cache hits
    uint64_t misses;                // Cache misses
    uint64_t evictions;             // Number of evictions
    double hit_rate;                // Hit rate (hits / total requests)
} CacheStats;

/**
 * LRU Cache structure
 */
typedef struct {
    CacheEntry* entries;            // Array of cache entries
    CacheEntry* head;               // Most recently used
    CacheEntry* tail;               // Least recently used
    int size;                       // Current number of entries
    int capacity;                   // Maximum capacity
    uint64_t total_hits;
    uint64_t total_misses;
    uint64_t total_evictions;
    pthread_mutex_t mutex;          // Thread safety
} LRUCache;

/**
 * Initialize the cache
 */
void init_cache();

/**
 * Get file from cache
 * @param file_path File path to lookup
 * @return FileInfo pointer if found, NULL if not
 */
FileInfo* cache_get_file(const char* file_path);

/**
 * Put file into cache
 * @param file_path File path (key)
 * @param file FileInfo pointer to cache
 */
void cache_put_file(const char* file_path, FileInfo* file);

/**
 * Invalidate cache entry for a file
 * @param file_path File path to invalidate
 */
void cache_invalidate_file(const char* file_path);

/**
 * Clear entire cache
 */
void cache_clear();

/**
 * Get cache statistics
 * @return CacheStats structure
 */
CacheStats get_cache_stats();

/**
 * Print cache statistics
 */
void print_cache_stats();

/**
 * Cleanup cache
 */
void cleanup_cache();

#endif // CACHE_H