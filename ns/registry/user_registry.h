#ifndef USER_REGISTRY_H
#define USER_REGISTRY_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#define MAX_USERS 1000

/**
 * Simple array-based user registry
 * Stores unique usernames
 */
typedef struct {
    char** usernames;           // Array of username strings
    int count;                  // Current number of users
    int capacity;               // Maximum capacity
    pthread_mutex_t mutex;      // Thread safety
} UserRegistry;

extern UserRegistry user_registry;

/**
 * Initialize the user registry
 */
void init_user_registry();

/**
 * Register a new user (adds if not exists)
 * @param username Username to register
 * @return 0 on success (new or existing), -1 on error
 */
int register_user(const char* username);

/**
 * Check if user exists
 * @param username Username to check
 * @return true if exists, false otherwise
 */
bool user_exists(const char* username);

/**
 * Get all registered users
 * @param users Array to store username pointers (don't free these!)
 * @param max_size Maximum size of array
 * @return Number of users copied
 */
int get_all_users(char** users, int max_size);

/**
 * Get total user count
 * @return Number of registered users
 */
int get_user_count();

/**
 * Cleanup user registry
 */
void cleanup_user_registry();

#endif // USER_REGISTRY_H