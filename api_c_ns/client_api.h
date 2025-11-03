#ifndef CLIENT_API_H
#define CLIENT_API_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <stddef.h>
#include "networking.h"


/**
 * Initialize a client connection to a server
 * @param server_ip Server IP address (e.g., "127.0.0.1")
 * @param port Server port number
 * @return Client socket file descriptor on success, NET_ERROR on failure
 */
int init_client(const char *server_ip, int port);

/**
 * Close client socket
 * @param client_fd Client socket file descriptor
 */
void close_client(int client_fd);


#endif // CLIENT_API_H