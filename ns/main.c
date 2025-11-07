#include "conn.h"
#include "handle_client.h"
#include "handle_ss.h"

// Default values
#define DEFAULT_NS_CLIENT_PORT 9090
#define DEFAULT_NS_CLIENT_BACKLOG 10
#define DEFAULT_NS_CLIENT_BUFFER_SIZE 4096

int main() {
    
    // Setup client server
    int client_server_fd = setup_server();
    if (client_server_fd < 0) {
        fprintf(stderr, "[NS] Failed to setup client server\n");
        return EXIT_FAILURE;
    }
    
    // Setup Storage Server listener
    int ss_server_fd = setup_ss_server();
    if (ss_server_fd < 0) {
        fprintf(stderr, "[NS] Failed to setup Storage Server listener\n");
        shutdown_server(client_server_fd);
        return EXIT_FAILURE;
    }
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║   NAMING SERVER FULLY INITIALIZED      ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("  Client Port:         %d\n", NS_CLIENT_PORT);
    printf("  Storage Server Port: %d\n", NS_SS_PORT);
    printf("═══════════════════════════════════════════\n\n");
    
    // Create thread for accepting Storage Server connections
    pthread_t ss_accept_thread;
    if (pthread_create(&ss_accept_thread, NULL, accept_storage_servers, &ss_server_fd) != 0) {
        fprintf(stderr, "[NS] Failed to create SS acceptance thread\n");
        shutdown_server(client_server_fd);
        close_socket(ss_server_fd);
        return EXIT_FAILURE;
    }
    
    printf("[NS] Storage Server acceptance thread created\n");
    printf("[NS] Now accepting client connections...\n\n");
    
    // Main loop for accepting client connections
    while (running) {
        
        // Accept incoming client connection
        Connection client_conn;
        int client_fd = accept_connection(client_server_fd, &client_conn);

        if (client_fd < 0) {
            if (!running) break;
            
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000);
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
    printf("\n[NS] Initiating shutdown sequence...\n");
    
    // Wait for SS thread to finish
    printf("[NS] Waiting for Storage Server thread to finish...\n");
    pthread_join(ss_accept_thread, NULL);
    
    shutdown_server(client_server_fd);
    
    printf("[NS] Giving active threads time to finish...\n");
    sleep(2);
    
    printf("[NS] Shutdown complete\n");
    
    return EXIT_SUCCESS;
}