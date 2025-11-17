#ifndef HANDLE_SS_H
#define HANDLE_SS_H


#include "../types.h"
#include <stdlib.h>

/**
 * Structure to pass data to Storage Server handler threads
 * Contains SS socket file descriptor and connection information
 */
typedef struct {
    int ss_fd;               // Storage Server socket file descriptor
    Connection ss_conn;      // SS connection details (IP, port)
    int ss_id;              // Unique ID for this storage server
} SSThreadData;


typedef struct {
    int ss_id;
    int ss_fd;
    char ss_ip[16];
    int nm_port;
    volatile bool should_monitor;
} HeartbeatMonitorArgs;



/**
 * Storage Server handler function that runs in a separate thread
 * Handles registration and ongoing communication with a single SS
 * Thread automatically detaches and cleans up resources on completion
 * @param arg Pointer to SSThreadData structure containing SS information
 * @return NULL on thread completion
 */
void* handle_storage_server(void* arg);

/**
 * Initialize the Storage Server listener
 * Creates a socket and binds it to NS_SS_PORT
 * @return Server socket file descriptor on success, -1 on failure
 */
int setup_ss_server();

/**
 * Main loop to accept Storage Server connections
 * Runs in a separate thread, creates new thread for each SS connection
 * @param arg Pointer to server_fd (int*)
 * @return NULL on completion
 */
void* accept_storage_servers(void* arg);

/**
 * Assign a new unique ID to a Storage Server
 * @return New unique SS ID
 */
int assign_new_ss_id();

/**
 * Monitor the heartbeat of a Storage Server
 * Runs in a separate thread, periodically checks for heartbeats
 * @param arg Pointer to HeartbeatMonitorArgs structure
 * @return NULL on thread completion
 */
void* monitor_ss_connection(void* arg);

#endif // HANDLE_SS_H