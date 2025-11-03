#include "naming_server.h"


// Thread-local storage for error messages
static __thread char error_buffer[256] = {0};


// ============================================================================
// SERVER FUNCTIONS
// ============================================================================

int init_server(int port, int backlog) {
    int server_fd;
    struct sockaddr_in server_addr;
    
    server_fd = create_tcp_socket();

    if (server_fd < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to create socket: %s", strerror(errno));
        return NET_ERROR;
    }
    
    // Set socket to reuse address
    if (set_socket_reuse(server_fd) != NET_SUCCESS) {
        if(close_socket(server_fd) != NET_SUCCESS) {
            printf("[SERVER] Server socket could not be closed after reuse failure (fd: %d)\n", server_fd);
        }
        return NET_ERROR;
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind socket to address
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to bind to port %d: %s", port, strerror(errno));
        
        if(close_socket(server_fd) != NET_SUCCESS) {
            printf("[SERVER] Server socket could not be closed after bind failure (fd: %d)\n", server_fd);
        }
        return NET_ERROR;
    }
    
    // Start listening
    if (listen(server_fd, backlog) < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to listen on socket: %s", strerror(errno));

        if (close_socket(server_fd) != NET_SUCCESS) {
            printf("[SERVER] Server socket could not be closed after listen failure (fd: %d)\n", server_fd);
        }
        return NET_ERROR;
    }
    
    printf("[SERVER] Listening on port %d (backlog: %d)\n", port, backlog);
    return server_fd;
}


int accept_connection(int server_fd, Connection *client_conn) {
    if (client_conn == NULL) {
        snprintf(error_buffer, sizeof(error_buffer), "NULL client_conn pointer");
        return NET_ERROR;
    }
    
    socklen_t addr_len = sizeof(client_conn->address);
    client_conn->socket_fd = accept(server_fd, 
                                     (struct sockaddr*)&client_conn->address, 
                                     &addr_len);
    
    if (client_conn->socket_fd < 0) {
        snprintf(error_buffer, sizeof(error_buffer), 
                "Failed to accept connection: %s", strerror(errno));
        return NET_ERROR;
    }
    
    // Store client information
    client_conn->port = ntohs(client_conn->address.sin_port);
    inet_ntop(AF_INET, &(client_conn->address.sin_addr), 
              client_conn->ip_address, INET_ADDRSTRLEN);
    
    printf("[SERVER] Accepted connection from %s:%d (fd: %d)\n", 
           client_conn->ip_address, client_conn->port, client_conn->socket_fd);
    
    return client_conn->socket_fd;
}

void close_server(int server_fd) {
    if (server_fd >= 0) {
        close_socket(server_fd);
        printf("[SERVER] Server socket closed (fd: %d)\n", server_fd);
    }
}
