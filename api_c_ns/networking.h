#ifndef NETWORKING_H
#define NETWORKING_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/time.h>

// #include "../api_ns_ss/ns_ss_connection.h"


// Connection structure to hold socket information of the peer requesting connection
typedef struct {
    int socket_fd;
    struct sockaddr_in address;
    int port;
    char ip_address[INET_ADDRSTRLEN];
} Connection;

// Return codes
#define NET_SUCCESS 0
#define NET_ERROR -1
#define NET_TIMEOUT -2
#define NET_CLOSED -3


// Common operations

int create_tcp_socket();    // Create a TCP socket, domain==AF_INET, type==SOCK_STREAM and protocol==0
int close_socket(int socket_fd);

/**
 * Send raw data over socket
 * @param socket_fd Socket file descriptor
 * @param data Pointer to data to send
 * @param len Length of data in bytes
 * @return Number of bytes sent on success, NET_ERROR on failure
 */
int send_data(int socket_fd, const void *data, size_t len);

/**
 * Receive raw data from socket
 * @param socket_fd Socket file descriptor
 * @param buffer Buffer to store received data
 * @param len Maximum number of bytes to receive
 * @return Number of bytes received on success, NET_ERROR on failure, NET_CLOSED if connection closed
 */
int recv_data(int socket_fd, void *buffer, size_t len);

/**
 * Send a null-terminated string message
 * @param socket_fd Socket file descriptor
 * @param message Null-terminated string to send
 * @return Number of bytes sent on success, NET_ERROR on failure
 */
int send_message(int socket_fd, const char *message);

/**
 * Receive a null-terminated string message
 * @param socket_fd Socket file descriptor
 * @param buffer Buffer to store received message
 * @param max_len Maximum buffer size
 * @return Number of bytes received on success, NET_ERROR on failure, NET_CLOSED if connection closed
 */
int recv_message(int socket_fd, char *buffer, size_t max_len);

// Utility functions
/**
 * Set socket read/write timeout
 * @param socket_fd Socket file descriptor
 * @param seconds Timeout in seconds
 * @return NET_SUCCESS on success, NET_ERROR on failure
 */
int set_socket_timeout(int socket_fd, int seconds);

/**
 * Set socket to non-blocking mode
 * @param socket_fd Socket file descriptor
 * @return NET_SUCCESS on success, NET_ERROR on failure
 */
int set_socket_nonblocking(int socket_fd);

/**
 * Set socket to reuse address (avoid "Address already in use" errors)
 * @param socket_fd Socket file descriptor
 * @return NET_SUCCESS on success, NET_ERROR on failure
 */
int set_socket_reuse(int socket_fd);

/**
 * Get the last socket error message
 * @return Pointer to error string
 */
const char* get_socket_error();

/**
 * Get peer information from a connected socket
 * @param socket_fd Socket file descriptor
 * @param conn Pointer to Connection structure to store peer info
 * @return NET_SUCCESS on success, NET_ERROR on failure
 */
int get_peer_info(int socket_fd, Connection *conn);


/**
 * Send binary protocol message
 */
// bool send_protocol_message(int fd, const ProtocolMessage* msg);


// bool recv_protocol_message(int fd, ProtocolMessage* msg);

#endif // NETWORKING_H
