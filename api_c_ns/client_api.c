#include "client_api.h"


// ============================================================================
// CLIENT FUNCTIONS
// ============================================================================

// Thread-local storage for error messages
static __thread char error_buffer[256] = {0};


int init_client(const char *server_ip, int port) {
    int client_fd;
    struct sockaddr_in server_addr;
    
    if (server_ip == NULL) {
        snprintf(error_buffer, sizeof(error_buffer), "NULL server IP address");
        return NET_ERROR;
    }
    

    client_fd = create_tcp_socket();
    if (client_fd < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to create socket: %s", strerror(errno));
        return NET_ERROR;
    }
    

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // Convert IP address from string to binary
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Invalid IP address: %s", server_ip);
        close(client_fd);
        return NET_ERROR;
    }
    
    // Connect to server
    if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to connect to %s:%d: %s", server_ip, port, strerror(errno));
        close(client_fd);
        return NET_ERROR;
    }
    
    printf("[CLIENT] Connected to %s:%d (fd: %d)\n", server_ip, port, client_fd);
    return client_fd;
}


void close_client(int client_fd) {

    if (client_fd >= 0) {
        if(close_socket(client_fd) == NET_SUCCESS) {
            printf("[CLIENT] Client socket closed (fd: %d)\n", client_fd);
        }
    }
}