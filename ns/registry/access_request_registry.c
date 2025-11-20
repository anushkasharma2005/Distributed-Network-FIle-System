#include "access_request_registry.h"
#include "file_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

AccessRequestRegistry access_request_registry;

/**
 * Initialize registry
 */
void init_access_request_registry() {
    access_request_registry.pending_requests = NULL;
    access_request_registry.count = 0;
    pthread_mutex_init(&access_request_registry.mutex, NULL);
    printf("[Access-Request-Registry] Initialized\n");
}

/**
 * Create new access request
 */
int create_access_request(const char* file_path, const char* requester,
                         const char* owner, const char* access_type) {
    if (!file_path || !requester || !owner || !access_type) {
        return -1;
    }
    
    pthread_mutex_lock(&access_request_registry.mutex);
    
    // Check for duplicate
    AccessRequest* current = access_request_registry.pending_requests;
    while (current) {
        if (strcmp(current->file_path, file_path) == 0 &&
            strcmp(current->requester, requester) == 0 &&
            strcmp(current->access_type, access_type) == 0 &&
            strcmp(current->status, "PENDING") == 0) {
            pthread_mutex_unlock(&access_request_registry.mutex);
            printf("[Access-Request] Duplicate request ignored\n");
            return -2;  // Duplicate
        }
        current = current->next;
    }
    
    // Create new request
    AccessRequest* new_req = malloc(sizeof(AccessRequest));
    if (!new_req) {
        pthread_mutex_unlock(&access_request_registry.mutex);
        return -1;
    }
    
    strncpy(new_req->file_path, file_path, sizeof(new_req->file_path) - 1);
    strncpy(new_req->requester, requester, sizeof(new_req->requester) - 1);
    strncpy(new_req->owner, owner, sizeof(new_req->owner) - 1);
    strncpy(new_req->access_type, access_type, sizeof(new_req->access_type) - 1);
    new_req->requested_at = time(NULL);
    strcpy(new_req->status, "PENDING");
    
    // Insert at head
    new_req->next = access_request_registry.pending_requests;
    access_request_registry.pending_requests = new_req;
    access_request_registry.count++;
    
    pthread_mutex_unlock(&access_request_registry.mutex);
    
    printf("[Access-Request] ✓ Created request: %s wants %s access to '%s'\n",
           requester, access_type, file_path);
    
    return 0;
}

/**
 * Get pending requests for owner
 */
int get_pending_requests_for_owner(const char* owner, AccessRequest** requests, int max_size) {
    if (!owner || !requests || max_size <= 0) {
        return 0;
    }
    
    pthread_mutex_lock(&access_request_registry.mutex);
    
    int count = 0;
    AccessRequest* current = access_request_registry.pending_requests;
    
    while (current && count < max_size) {
        if (strcmp(current->owner, owner) == 0 &&
            strcmp(current->status, "PENDING") == 0) {
            requests[count++] = current;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&access_request_registry.mutex);
    
    return count;
}

/**
 * Approve request
 */
int approve_access_request(const char* file_path, const char* requester, const char* access_type) {
    if (!file_path || !requester || !access_type) {
        return -1;
    }
    
    pthread_mutex_lock(&access_request_registry.mutex);
    
    AccessRequest* current = access_request_registry.pending_requests;
    while (current) {
        if (strcmp(current->file_path, file_path) == 0 &&
            strcmp(current->requester, requester) == 0 &&
            strcmp(current->access_type, access_type) == 0 &&
            strcmp(current->status, "PENDING") == 0) {
            
            strcpy(current->status, "APPROVED");
            pthread_mutex_unlock(&access_request_registry.mutex);
            
            // Actually grant access
            int result;
            if (strcmp(access_type, "READ") == 0) {
                result = add_read_access(file_path, requester);
            } else {
                result = add_write_access(file_path, requester);
            }
            
            printf("[Access-Request] ✓ Approved: %s now has %s access to '%s'\n",
                   requester, access_type, file_path);
            
            return result;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&access_request_registry.mutex);
    return -1;  // Not found
}

/**
 * Reject request
 */
int reject_access_request(const char* file_path, const char* requester, const char* access_type) {
    if (!file_path || !requester || !access_type) {
        return -1;
    }
    
    pthread_mutex_lock(&access_request_registry.mutex);
    
    AccessRequest* current = access_request_registry.pending_requests;
    while (current) {
        if (strcmp(current->file_path, file_path) == 0 &&
            strcmp(current->requester, requester) == 0 &&
            strcmp(current->access_type, access_type) == 0 &&
            strcmp(current->status, "PENDING") == 0) {
            
            strcpy(current->status, "REJECTED");
            pthread_mutex_unlock(&access_request_registry.mutex);
            
            printf("[Access-Request] ✗ Rejected: %s's %s request for '%s'\n",
                   requester, access_type, file_path);
            
            return 0;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&access_request_registry.mutex);
    return -1;  // Not found
}

/**
 * Check if request exists
 */
bool request_exists(const char* file_path, const char* requester, const char* access_type) {
    pthread_mutex_lock(&access_request_registry.mutex);
    
    AccessRequest* current = access_request_registry.pending_requests;
    while (current) {
        if (strcmp(current->file_path, file_path) == 0 &&
            strcmp(current->requester, requester) == 0 &&
            strcmp(current->access_type, access_type) == 0 &&
            strcmp(current->status, "PENDING") == 0) {
            pthread_mutex_unlock(&access_request_registry.mutex);
            return true;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&access_request_registry.mutex);
    return false;
}

/**
 * Clear pending request (auto-approve when ADDACCESS used)
 */
void clear_pending_request(const char* file_path, const char* requester, const char* access_type) {
    pthread_mutex_lock(&access_request_registry.mutex);
    
    AccessRequest* current = access_request_registry.pending_requests;
    while (current) {
        if (strcmp(current->file_path, file_path) == 0 &&
            strcmp(current->requester, requester) == 0 &&
            strcmp(current->access_type, access_type) == 0 &&
            strcmp(current->status, "PENDING") == 0) {
            
            strcpy(current->status, "APPROVED_MANUAL");
            printf("[Access-Request] Auto-approved via ADDACCESS\n");
            break;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&access_request_registry.mutex);
}

/**
 * Cleanup
 */
void cleanup_access_request_registry() {
    pthread_mutex_lock(&access_request_registry.mutex);
    
    AccessRequest* current = access_request_registry.pending_requests;
    while (current) {
        AccessRequest* temp = current;
        current = current->next;
        free(temp);
    }
    
    access_request_registry.pending_requests = NULL;
    access_request_registry.count = 0;
    
    pthread_mutex_unlock(&access_request_registry.mutex);
    pthread_mutex_destroy(&access_request_registry.mutex);
    
    printf("[Access-Request-Registry] Cleanup complete\n");
}