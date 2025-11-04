#include "conn.h"
#include "handle_client.h"


// Default values (used if .env file is not found or values are missing)
#define DEFAULT_NS_CLIENT_PORT 9090
#define DEFAULT_NS_CLIENT_BACKLOG 10
#define DEFAULT_NS_CLIENT_BUFFER_SIZE 4096


int main() {
    
    int server_fd = setup_server();

    set_socket_nonblocking(server_fd);  // Set server socket to non-blocking mode for graceful shutdown

    // Main server loop
    while (running) {
        
        // Accept incoming client connection
        Connection client_conn;
        int client_fd = accept_connection(server_fd, &client_conn);

        if (client_fd < 0) {
            if (!running) break;  // Exit if shutdown signal received
            
            // Check if it's just a non-blocking wait (no client yet)
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000);  // Sleep for 100ms to avoid busy waiting
                continue;
            }
            
            fprintf(stderr, "[NS ERROR] Failed to accept connection: %s\n", get_socket_error());
            continue;
        }

        printf("\n[NS] ✓ New client connected from %s:%d\n", 
               client_conn.ip_address, client_conn.port);
        printf("═══════════════════════════════════════════\n");

        // Allocate memory for thread data
        ClientThreadData* thread_data = malloc(sizeof(ClientThreadData));
        if (!thread_data) {
            fprintf(stderr, "[NS ERROR] Failed to allocate memory for thread data\n");
            close_socket(client_fd);
            continue;
        }

        thread_data->client_fd = client_fd;
        thread_data->client_conn = client_conn;

        // Create a new thread to handle this client
        pthread_t thread_id;
        int result = pthread_create(&thread_id, NULL, handle_client, thread_data);
        
        if (result != 0) {
            fprintf(stderr, "[NS ERROR] Failed to create thread for client %s:%d: %s\n",
                    client_conn.ip_address, client_conn.port, strerror(result));
            close_socket(client_fd);
            free(thread_data);
            continue;
        }

        printf("[NS] Thread %lu created for client %s:%d\n",
               (unsigned long)thread_id, client_conn.ip_address, client_conn.port);
        printf("[NS] Waiting for next client...\n\n");
    }

    // Cleanup
    shutdown_server(server_fd);
    
    return 0;
}
