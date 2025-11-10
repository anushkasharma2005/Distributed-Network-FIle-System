#include "conn.h"

// Global flag for graceful shutdown
volatile sig_atomic_t running = 1;

int setup_server(){
    
    starting_msg(); // Display starting message
    setup_signal_handlers();  // for graceful shutdown

    // Initialize naming server
    printf("[NS] Initializing server on port %d...\n", NS_CLIENT_PORT);
    int server_fd = init_server(NS_CLIENT_PORT, NS_CLIENT_BACKLOG);

    if (server_fd < 0) {
        fprintf(stderr, "[NS ERROR] Failed to initialize server: %s\n", get_socket_error());
        return -1;
    }

    printf("[NS] Server initialized successfully!\n");
    printf("[NS] Waiting for client connections...\n");
    printf("═══════════════════════════════════════════\n\n");

    // Set server socket to non-blocking mode for graceful shutdown
    set_socket_nonblocking(server_fd);

    return server_fd;
}

void starting_msg() {
    printf("╔════════════════════════════════════════╗\n");
    printf("║    NAMING SERVER (NS) STARTED          ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    printf("[NS] Configuration loaded:\n");
    printf("     - NS_CLIENT_PORT: %d\n", NS_CLIENT_PORT);
    printf("     - NS_CLIENT_BACKLOG: %d\n", NS_CLIENT_BACKLOG);
    printf("     - NS_CLIENT_BUFFER_SIZE: %d\n\n", NS_CLIENT_BUFFER_SIZE);
}

void shutdown_server(int server_fd) {
    printf("\n[NS] Shutting down server...\n");
    close_socket(server_fd);
    printf("[NS] Server closed successfully.\n");
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║    NAMING SERVER (NS) STOPPED          ║\n");
    printf("╚════════════════════════════════════════╝\n");
}

void setup_signal_handlers() {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

void signal_handler(int signum) {
    (void)signum;  // Suppress unused parameter warning
    printf("\n[NS] Received shutdown signal. Cleaning up...\n");
    running = 0;
}