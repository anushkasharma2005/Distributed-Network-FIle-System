#include "conn.h"


// Global flag for graceful shutdown
volatile sig_atomic_t running = 1;


/**
 * Setup and initialize the naming server for client connections
 * Uses configuration from constants.h and creates server socket
 * @return Server socket file descriptor on success, -1 on failure
 */
ServerFDs setup_server(){
    
    ServerFDs fds = {-1, -1};

    printf("╔════════════════════════════════════════╗\n");
    printf("║    NAMING SERVER (NS) STARTED          ║\n");
    printf("╚════════════════════════════════════════╝\n\n");


    setup_signal_handlers();  // for graceful shutdown

    // Initialize naming server
    printf("[NS] Initializing client and storage servers on ports %d and %d respectively...\n", NS_CLIENT_PORT, NS_SS_PORT);
    
    // Setup client server
    fds.client_server_fd = setup_client_server();
    if (fds.client_server_fd < 0) {
        fprintf(stderr, "[NS] Failed to setup client server\n");
        return fds;
    }
    
    // Setup Storage Server listener
    fds.ss_server_fd = setup_ss_server();
    if (fds.ss_server_fd < 0) {
        fprintf(stderr, "[NS] Failed to setup Storage Server listener\n");
        shutdown_server(fds.client_server_fd);
        return fds;
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

    return fds;
}


/**
 * Create threads for accepting client and storage server connections
 * @param fds Pointer to ServerFDs structure containing server socket FDs
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on failure
 */
int create_acceptance_threads(ServerFDs *fds) {

    printf("═══════════════════════════════════════════\n");
    // Create thread for accepting Storage Server connections
    pthread_t ss_accept_thread;
    if (pthread_create(&ss_accept_thread, NULL, accept_storage_servers, &fds->ss_server_fd) != 0) {
        fprintf(stderr, "[NS] Failed to create SS acceptance thread\n");
        shutdown_server(fds->client_server_fd);
        close_socket(fds->ss_server_fd);
        return EXIT_FAILURE;
    }


    printf("[NS] Storage Server acceptance thread created\n");
    printf("[NS] Now accepting client connections...\n");


    // Create thread for accepting Client connections
    pthread_t client_accept_thread;
    if (pthread_create(&client_accept_thread, NULL, accept_clients, &fds->client_server_fd) != 0) {
        fprintf(stderr, "[NS] Failed to create client acceptance thread\n");
        shutdown_server(fds->client_server_fd);
        close_socket(fds->ss_server_fd);
        // Signal SS thread to stop
        running = 0;
        pthread_join(ss_accept_thread, NULL);
        return EXIT_FAILURE;
    }

    printf("[NS] Client acceptance thread created\n");
    printf("[NS] All systems operational\n");
    printf("═══════════════════════════════════════════\n\n");

    // Wait for SS thread to finish
    pthread_join(client_accept_thread, NULL);
    pthread_join(ss_accept_thread, NULL);

    printf("[NS] Waiting for Client and Storage Server  thread to finish...\n");

    return EXIT_SUCCESS;

}

/**
 * Shutdown the naming server gracefully
 * Closes server socket and displays shutdown message
 * @param server_fd Server socket file descriptor to close
 */
void shutdown_main_server(ServerFDs fds) {
    printf("\n[NS] Shutting down ns server...\n");
    close_socket(fds.client_server_fd);
    close_socket(fds.ss_server_fd);


    printf("[NS] Server closed successfully.\n");
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║    NAMING SERVER (NS) STOPPED          ║\n");
    printf("╚════════════════════════════════════════╝\n");

    printf("[NS] Giving active threads time to finish...\n");
    sleep(2);
    printf("[NS] Shutdown complete\n");
}

/**
 * Setup signal handlers for graceful shutdown
 * Registers handlers for SIGINT and SIGTERM signals
 */
void setup_signal_handlers() {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

/**
 * Signal handler function for shutdown signals
 * Sets global 'running' flag to 0 for graceful shutdown
 * @param signum Signal number received
 */
void signal_handler(int signum) {
    (void)signum;  // Suppress unused parameter warning
    printf("\n[NS] Received shutdown signal. Cleaning up...\n");
    running = 0;
}

/**
 * Shutdown the specified server
 * @param server_fd Server socket file descriptor to close
 */
void shutdown_server(int server_fd) {
    close_socket(server_fd);
    printf("[NS] Server socket %d closed successfully.\n", server_fd);
}