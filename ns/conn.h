#ifndef CONN_H
#define CONN_H

#include "../api_c_ns/naming_server.h"
#include <signal.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>

// Global flag for graceful shutdown
extern volatile sig_atomic_t running;
extern int NS_CLIENT_PORT;
extern int NS_CLIENT_BACKLOG;
extern int NS_CLIENT_BUFFER_SIZE;

/**
 * Setup and initialize the naming server
 * Loads configuration from .env file and creates server socket
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
 * @param ns_client_port Port number for client connections
 * @param ns_client_backlog Maximum number of pending connections
 * @param ns_client_buffer_size Size of receive/send buffer
 */
void starting_msg(int ns_client_port, int ns_client_backlog, int ns_client_buffer_size);

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
 * Load integer value from .env file
 * Reads key-value pairs from ../.env and returns the value for given key
 * @param key Environment variable key to search for
 * @param default_value Default value to return if key not found
 * @return Integer value from .env file or default_value
 */
int load_env_int(const char *key, int default_value);

#endif // CONN_H