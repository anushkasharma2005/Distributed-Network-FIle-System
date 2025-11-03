#include "networking.h"


// Server functions
/**
 * Initialize a server socket
 * @param port Port number to bind to
 * @param backlog Maximum number of pending connections
 * @return Server socket file descriptor on success, NET_ERROR on failure
 */
int init_server(int port, int backlog);

/**
 * Accept an incoming client connection
 * @param server_fd Server socket file descriptor
 * @param client_conn Pointer to Connection structure to store client info
 * @return Client socket file descriptor on success, NET_ERROR on failure
 */
int accept_connection(int server_fd, Connection *client_conn);

/**
 * Close server socket
 * @param server_fd Server socket file descriptor
 */
void close_server(int server_fd);

