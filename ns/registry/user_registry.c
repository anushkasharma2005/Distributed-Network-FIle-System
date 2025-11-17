#include "user_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Global user registry
UserRegistry user_registry;

/**
 * Initialize user registry
 */
void init_user_registry() {
    user_registry.usernames = calloc(MAX_USERS, sizeof(char*));
    user_registry.count = 0;
    user_registry.capacity = MAX_USERS;
    pthread_mutex_init(&user_registry.mutex, NULL);
    
    printf("[User-Registry] Initialized (capacity: %d)\n", MAX_USERS);
}

/**
 * Register a new user - O(n) check for duplicates but n is small
 */
int register_user(const char* username) {
    if (!username || strlen(username) == 0) {
        return -1;
    }
    
    pthread_mutex_lock(&user_registry.mutex);
    
    // Check if user already exists
    for (int i = 0; i < user_registry.count; i++) {
        if (strcmp(user_registry.usernames[i], username) == 0) {
            pthread_mutex_unlock(&user_registry.mutex);
            return 0;  // Already exists, not an error
        }
    }
    
    // Add new user
    if (user_registry.count >= user_registry.capacity) {
        pthread_mutex_unlock(&user_registry.mutex);
        fprintf(stderr, "[User-Registry ERROR] Maximum users reached (%d)\n", user_registry.capacity);
        return -1;
    }
    
    user_registry.usernames[user_registry.count] = strdup(username);
    if (!user_registry.usernames[user_registry.count]) {
        pthread_mutex_unlock(&user_registry.mutex);
        return -1;
    }
    
    user_registry.count++;
    
    pthread_mutex_unlock(&user_registry.mutex);
    
    printf("[User-Registry] ✓ Registered user '%s' (total: %d)\n", username, user_registry.count);
    return 0;
}

/**
 * Check if user exists - O(n) but fast for small n
 */
bool user_exists(const char* username) {
    if (!username) return false;
    
    pthread_mutex_lock(&user_registry.mutex);
    
    for (int i = 0; i < user_registry.count; i++) {
        if (strcmp(user_registry.usernames[i], username) == 0) {
            pthread_mutex_unlock(&user_registry.mutex);
            return true;
        }
    }
    
    pthread_mutex_unlock(&user_registry.mutex);
    return false;
}

/**
 * Get all users - O(1) just copies pointers
 */
int get_all_users(char** users, int max_size) {
    if (!users || max_size <= 0) return 0;
    
    pthread_mutex_lock(&user_registry.mutex);
    
    int count = (user_registry.count < max_size) ? user_registry.count : max_size;
    
    // Just copy pointers, don't duplicate strings
    for (int i = 0; i < count; i++) {
        users[i] = user_registry.usernames[i];
    }
    
    pthread_mutex_unlock(&user_registry.mutex);
    
    return count;
}

/**
 * Get user count - O(1)
 */
int get_user_count() {
    pthread_mutex_lock(&user_registry.mutex);
    int count = user_registry.count;
    pthread_mutex_unlock(&user_registry.mutex);
    return count;
}

/**
 * Cleanup user registry
 */
void cleanup_user_registry() {
    pthread_mutex_lock(&user_registry.mutex);
    
    for (int i = 0; i < user_registry.count; i++) {
        free(user_registry.usernames[i]);
    }
    free(user_registry.usernames);
    
    user_registry.count = 0;
    user_registry.capacity = 0;
    
    pthread_mutex_unlock(&user_registry.mutex);
    pthread_mutex_destroy(&user_registry.mutex);
    
    printf("[User-Registry] Cleanup complete\n");
}