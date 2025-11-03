#include "../api_c_ns/naming_server.h"
#include <signal.h>
#include <stdbool.h>

#define NS_PORT 9090
#define BACKLOG 10
#define BUFFER_SIZE 4096

// Global flag for graceful shutdown
volatile sig_atomic_t running = 1;

void signal_handler(int signum) {
    (void)signum;  // Suppress unused parameter warning
    printf("\n[NS] Received shutdown signal. Cleaning up...\n");
    running = 0;
}

int main() {
    printf("╔════════════════════════════════════════╗\n");
    printf("║    NAMING SERVER (NS) STARTED          ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize naming server
    printf("[NS] Initializing server on port %d...\n", NS_PORT);
    int server_fd = init_server(NS_PORT, BACKLOG);

    if (server_fd < 0) {
        fprintf(stderr, "[NS ERROR] Failed to initialize server: %s\n", get_socket_error());
        return 1;
    }

    printf("[NS] Server initialized successfully!\n");
    printf("[NS] Waiting for client connections...\n");
    printf("═══════════════════════════════════════════\n\n");

    // Set server socket to non-blocking mode for graceful shutdown
    set_socket_nonblocking(server_fd);

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

        // Handle client messages (keep connection open for multiple messages)
        char buffer[BUFFER_SIZE];
        bool client_connected = true;
        int message_count = 0;

        while (client_connected && running) {
            // Receive message from client (blocking, no timeout)
            int bytes_received = recv_message(client_fd, buffer, BUFFER_SIZE);

            if (bytes_received == NET_CLOSED) {
                printf("\n[NS] Client %s:%d disconnected gracefully.\n", 
                       client_conn.ip_address, client_conn.port);
                client_connected = false;
                break;
            }

            if (bytes_received < 0) {
                fprintf(stderr, "[NS ERROR] Failed to receive message: %s\n", get_socket_error());
                client_connected = false;
                break;
            }

            message_count++;

            // Print received message
            printf("\n┌─ Message #%d from %s:%d ─────────────\n", 
                   message_count, client_conn.ip_address, client_conn.port);
            printf("│ Length: %d bytes\n", bytes_received);
            printf("│ Content: %s\n", buffer);
            printf("└────────────────────────────────────────\n");

            // Send acknowledgment back to client
            const char *ack = "ACK: Message received by NS";
            int bytes_sent = send_message(client_fd, ack);

            if (bytes_sent < 0) {
                fprintf(stderr, "[NS ERROR] Failed to send acknowledgment: %s\n", get_socket_error());
                client_connected = false;
                break;
            }

            printf("[NS] ✓ Acknowledgment sent to client\n");
            printf("[NS] Waiting for next message from client...\n");
        }

        // Close client connection
        close_socket(client_fd);
        printf("[NS] Connection with %s:%d closed.\n", 
               client_conn.ip_address, client_conn.port);
        printf("═══════════════════════════════════════════\n");
        printf("[NS] Waiting for next client...\n\n");
    }

    // Cleanup
    printf("\n[NS] Shutting down server...\n");
    close_server(server_fd);
    printf("[NS] Server closed successfully.\n");
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║    NAMING SERVER (NS) STOPPED            ║\n");
    printf("╚═════════════════════════════════════════╝\n");

    return 0;
}
