#include "conn.h"
#include "handle_client.h"
#include "handle_ss.h"
#include "ss_registry.h"
#include "file_registry.h" 
#include "../include/constants.h"

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
    
    // Initialize SS registry
    init_ss_registry();
    init_file_registry();


    printf("[NS] Storage Server registry initialized\n");
    printf("[NS] File registry initialized\n\n");



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
    

    printf("═══════════════════════════════════════════\n");
    printf("[NS] Storage Server acceptance thread created\n");
    printf("[NS] Now accepting client connections...\n");
    printf("═══════════════════════════════════════════\n\n");


    // Create thread for accepting Client connections
    pthread_t client_accept_thread;
    if (pthread_create(&client_accept_thread, NULL, accept_clients, &client_server_fd) != 0) {
        fprintf(stderr, "[NS] Failed to create client acceptance thread\n");
        shutdown_server(client_server_fd);
        close_socket(ss_server_fd);
        // Signal SS thread to stop
        running = 0;
        pthread_join(ss_accept_thread, NULL);
        return EXIT_FAILURE;
    }
    
    printf("═══════════════════════════════════════════\n");
    printf("[NS] Client acceptance thread created\n");
    printf("[NS] All systems operational\n");
    printf("═══════════════════════════════════════════\n\n");
    
    


    
    // Wait for SS thread to finish
    printf("[NS] Waiting for Client and Storage Server  thread to finish...\n");
    pthread_join(ss_accept_thread, NULL);
    pthread_join(client_accept_thread, NULL);
    
    // Cleanup
    printf("\n[NS] Initiating shutdown sequence...\n");


    
    // Cleanup registry
    cleanup_ss_registry();
    cleanup_file_registry();

    
    shutdown_server(client_server_fd);
    
    printf("[NS] Giving active threads time to finish...\n");
    sleep(2);
    
    printf("[NS] Shutdown complete\n");
    
    return EXIT_SUCCESS;
}