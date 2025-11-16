#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#include "../api_c_ns/naming_server.h"


// Forward declaration for Connection (defined in api_c_ns/naming_server.h)
/**
 * Structure to hold server file descriptors
 */
typedef struct {
    int client_server_fd;
    int ss_server_fd;
} ServerFDs;


// Forward declaration for StorageServerInfo (defined in ss_registry.h)
typedef struct StorageServerInfo StorageServerInfo;

// Forward declaration for FileInfo (defined in file_registry.h)
typedef struct FileInfo FileInfo;

#endif // TYPES_H