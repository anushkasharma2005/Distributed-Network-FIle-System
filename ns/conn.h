#ifndef CONN_H
#define CONN_H

#include "../api_c_ns/naming_server.h"
#include "../include/constants.h"
#include <signal.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>


#include "../include/constants.h"
#include "ss_registry.h"
#include "file_registry.h"
#include "handle_client.h"
#include "handle_ss.h"


typedef struct {
    int client_server_fd;
    int ss_server_fd;
} ServerFDs;


// Global flag for graceful shutdown
extern volatile sig_atomic_t running;

/**
 * Setup and initialize the naming server for client connections
 * Uses configuration from constants.h and creates server socket
 * @return Server socket file descriptor on success, -1 on failure
 */
ServerFDs setup_server();

/**
 * Shutdown the naming server gracefully
 * Closes server socket and displays shutdown message
 * @param server_fd Server socket file descriptor to close
 */
void shutdown_main_server(ServerFDs fds);


/**
 * Setup signal handlers for graceful shutdown
 * Registers handlers for SIGINT and SIGTERM signals
 */
void setup_signal_handlers();

/**
 * Signal handler function for shutdown signals
 * Sets global 'running' flag to 0 for graceful shutdown
 * @param signum Signal number received
 */
void signal_handler(int signum);


/**
 * Create threads for accepting client and storage server connections
 * @param fds Pointer to ServerFDs structure containing server FDs
 * @return 0 on success, -1 on failure
 */
int create_acceptance_threads(ServerFDs *fds);

/**
 * Shutdown the specified server
 * @param server_fd Server socket file descriptor to close
 */
void shutdown_server(int server_fd);

#endif // CONN_H