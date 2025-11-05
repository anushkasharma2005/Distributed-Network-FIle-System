#ifndef CONN_H
#define CONN_H

#include "../api_c_ns/naming_server.h"
#include "../include/constants.h"
#include <signal.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>

// Global flag for graceful shutdown
extern volatile sig_atomic_t running;

/**
 * Setup and initialize the naming server for client connections
 * Uses configuration from constants.h and creates server socket
 * @return Server socket file descriptor on success, -1 on failure
 */
int setup_server();

/**
 * Shutdown the naming server gracefully
 * Closes server socket and displays shutdown message
 * @param server_fd Server socket file descriptor to close
 */
void shutdown_server(int server_fd);

/**
 * Display starting message with server configuration
 * Shows NS_CLIENT_PORT, NS_CLIENT_BACKLOG, and NS_CLIENT_BUFFER_SIZE
 */
void starting_msg();

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

#endif // CONN_H