#include "handle_client.h"
#include "../include/constants.h"

// Client handler function that runs in separate thread
void* handle_client(void* arg) {
    ClientThreadData* data = (ClientThreadData*)arg;
    int client_fd = data->client_fd;
    Connection client_conn = data->client_conn;
    
    // Free the allocated data structure
    free(data);
    
    // Detach thread so resources are automatically freed
    pthread_detach(pthread_self());
    
    printf("\n[NS] Thread created for client %s:%d\n", 
           client_conn.ip_address, client_conn.port);
    
    // Handle client messages
    char buffer[NS_CLIENT_BUFFER_SIZE];
    bool client_connected = true;
    int message_count = 0;

    while (client_connected && running) {
        // Receive message from client
        int bytes_received = recv_message(client_fd, buffer, NS_CLIENT_BUFFER_SIZE);

        if (bytes_received == NET_CLOSED) {
            printf("\n[NS] Client %s:%d disconnected gracefully.\n", 
                   client_conn.ip_address, client_conn.port);
            client_connected = false;
            break;
        }

        if (bytes_received < 0) {
            fprintf(stderr, "[NS ERROR] Failed to receive message from %s:%d: %s\n",
                    client_conn.ip_address, client_conn.port, get_socket_error());
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
            fprintf(stderr, "[NS ERROR] Failed to send acknowledgment to %s:%d: %s\n",
                    client_conn.ip_address, client_conn.port, get_socket_error());
            client_connected = false;
            break;
        }

        printf("[NS] ✓ Acknowledgment sent to client %s:%d\n",
               client_conn.ip_address, client_conn.port);
    }

    // Close client connection
    close_socket(client_fd);
    printf("[NS] Connection with %s:%d closed.\n", 
           client_conn.ip_address, client_conn.port);
    printf("═══════════════════════════════════════════\n");
    
    return NULL;
}