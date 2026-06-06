#ifndef ACCESS_REQUEST_REGISTRY_H
#define ACCESS_REQUEST_REGISTRY_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include "../../include/constants.h"

/**
 * Access request structure
 */
typedef struct AccessRequest {
    char file_path[256];
    char requester[MAX_USERNAME_LENGTH];
    char owner[MAX_USERNAME_LENGTH];
    char access_type[10];           // "READ" or "WRITE"
    time_t requested_at;
    char status[20];                // "PENDING", "APPROVED", "REJECTED"
    struct AccessRequest* next;
} AccessRequest;

/**
 * Access request registry
 */
typedef struct {
    AccessRequest* pending_requests;
    int count;
    pthread_mutex_t mutex;
} AccessRequestRegistry;

extern AccessRequestRegistry access_request_registry;

/**
 * Initialize access request registry
 */
void init_access_request_registry();

/**
 * Create a new access request
 * @return 0 on success, -1 on error
 */
int create_access_request(const char* file_path, const char* requester,
                         const char* owner, const char* access_type);

/**
 * Get all pending requests for a specific owner
 * @param owner Username of file owner
 * @param requests Array to store request pointers (caller allocates)
 * @param max_size Maximum array size
 * @return Number of pending requests found
 */
int get_pending_requests_for_owner(const char* owner, AccessRequest** requests, int max_size);

/**
 * Approve an access request
 * @return 0 on success, -1 on error
 */
int approve_access_request(const char* file_path, const char* requester, const char* access_type);

/**
 * Reject an access request
 * @return 0 on success, -1 on error
 */
int reject_access_request(const char* file_path, const char* requester, const char* access_type);

/**
 * Check if a request already exists
 * @return true if pending request exists
 */
bool request_exists(const char* file_path, const char* requester, const char* access_type);

/**
 * Clear pending request (called when ADDACCESS is used directly)
 */
void clear_pending_request(const char* file_path, const char* requester, const char* access_type);

/**
 * Cleanup registry
 */
void cleanup_access_request_registry();

#endif // ACCESS_REQUEST_REGISTRY_H