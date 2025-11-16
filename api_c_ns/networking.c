#include "networking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/time.h>


// Thread-local storage for error messages
static __thread char error_buffer[256] = {0};


// ============================================================================
// COMMON OPERATIONS
// ============================================================================


int create_tcp_socket(){
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to create TCP socket: %s", strerror(errno));
        return NET_ERROR;
    }
    return sock_fd;
}


int close_socket(int socket_fd) {
    if (socket_fd >= 0) {
        close(socket_fd);
        return NET_SUCCESS;
    }
    snprintf(error_buffer, sizeof(error_buffer), "Invalid socket fd: %d", socket_fd);
    return NET_ERROR;
}


int send_data(int socket_fd, const void *data, size_t len) {
    if (data == NULL || len == 0) {
        snprintf(error_buffer, sizeof(error_buffer), "Invalid data or length");
        return NET_ERROR;
    }
    
    size_t total_sent = 0;
    const char *ptr = (const char*)data;
    
    while (total_sent < len) {
        ssize_t sent = send(socket_fd, ptr + total_sent, len - total_sent, 0);
        
        if (sent < 0) {
            if (errno == EINTR) {
                continue; // Interrupted, retry
            }
            snprintf(error_buffer, sizeof(error_buffer), 
                    "Failed to send data: %s", strerror(errno));
            return NET_ERROR;
        }
        
        if (sent == 0) {
            snprintf(error_buffer, sizeof(error_buffer), "Connection closed by peer");
            return NET_CLOSED;
        }
        
        total_sent += sent;
    }
    
    return total_sent;
}

int recv_data(int socket_fd, void *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        snprintf(error_buffer, sizeof(error_buffer), "Invalid buffer or length");
        return NET_ERROR;
    }
    
    ssize_t received = recv(socket_fd, buffer, len, 0);
    
    if (received < 0) {
        if (errno == EINTR) {
            return 0; // Interrupted, can retry
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return NET_TIMEOUT;
        }
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to receive data: %s", strerror(errno));
        return NET_ERROR;
    }
    
    if (received == 0) {
        return NET_CLOSED;
    }
    
    return received;
}

int send_message(int socket_fd, const char *message) {
    if (message == NULL) {
        snprintf(error_buffer, sizeof(error_buffer), "NULL message pointer");
        return NET_ERROR;
    }
    
    size_t msg_len = strlen(message);
    
    // Send message length first (4 bytes)
    uint32_t net_len = htonl(msg_len);
    if (send_data(socket_fd, &net_len, sizeof(net_len)) != sizeof(net_len)) {
        return NET_ERROR;
    }
    
    // Send actual message
    return send_data(socket_fd, message, msg_len);
}

int recv_message(int socket_fd, char *buffer, size_t max_len) {
    if (buffer == NULL || max_len == 0) {
        snprintf(error_buffer, sizeof(error_buffer), "Invalid buffer or length");
        return NET_ERROR;
    }
    
    // Receive message length first (4 bytes)
    uint32_t net_len;
    int result = recv_data(socket_fd, &net_len, sizeof(net_len));
    if (result <= 0) {
        return result;
    }
    
    size_t msg_len = ntohl(net_len);
    
    // Check if buffer is large enough
    if (msg_len >= max_len) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Message too large: %zu bytes (buffer: %zu bytes)", msg_len, max_len);
        return NET_ERROR;
    }
    
    // Receive actual message
    size_t total_received = 0;
    while (total_received < msg_len) {
        result = recv_data(socket_fd, buffer + total_received, msg_len - total_received);
        if (result <= 0) {
            return result;
        }
        total_received += result;
    }
    
    // Null-terminate the string
    buffer[total_received] = '\0';
    
    return total_received;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

int set_socket_timeout(int socket_fd, int seconds) {
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    
    // Set both send and receive timeouts
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to set receive timeout: %s", strerror(errno));
        return NET_ERROR;
    }
    
    if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to set send timeout: %s", strerror(errno));
        return NET_ERROR;
    }
    
    return NET_SUCCESS;
}

int set_socket_nonblocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to get socket flags: %s", strerror(errno));
        return NET_ERROR;
    }
    
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to set non-blocking mode: %s", strerror(errno));
        return NET_ERROR;
    }
    
    return NET_SUCCESS;
}

int set_socket_reuse(int socket_fd) {
    int optval = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to set SO_REUSEADDR: %s", strerror(errno));
        return NET_ERROR;
    }
    
    return NET_SUCCESS;
}

const char* get_socket_error() {
    return error_buffer;
}

int get_peer_info(int socket_fd, Connection *conn) {
    if (conn == NULL) {
        snprintf(error_buffer, sizeof(error_buffer), "NULL connection pointer");
        return NET_ERROR;
    }
    
    socklen_t addr_len = sizeof(conn->address);
    if (getpeername(socket_fd, (struct sockaddr*)&conn->address, &addr_len) < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to get peer info: %s", strerror(errno));
        return NET_ERROR;
    }
    
    conn->socket_fd = socket_fd;
    conn->port = ntohs(conn->address.sin_port);
    inet_ntop(AF_INET, &(conn->address.sin_addr), 
              conn->ip_address, INET_ADDRSTRLEN);
    
    return NET_SUCCESS;
}


/**
 * Send binary protocol message
 */
bool send_protocol_message(int fd, const ProtocolMessage* msg) {
    ssize_t sent = send(fd, msg, sizeof(ProtocolMessage), 0);
    if (sent != sizeof(ProtocolMessage)) {
        printf("[ERROR][Networking] Failed to send protocol message: sent %zd of %zu bytes\n", 
               sent, sizeof(ProtocolMessage));
        return false;
    }
    return true;
}

/**
 * Receive binary protocol message
 */
bool recv_protocol_message(int fd, ProtocolMessage* msg) {
    ssize_t received = recv(fd, msg, sizeof(ProtocolMessage), 0);
    if (received != sizeof(ProtocolMessage)) {
        printf("[ERROR][Networking] Failed to receive protocol message: got %zd of %zu bytes\n", 
               received, sizeof(ProtocolMessage));
        return false;
    }
    return true;
}