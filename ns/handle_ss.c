#include "handle_ss.h"
#include "../include/constants.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

// Counter for assigning unique IDs to storage servers
static int ss_id_counter = 0;
static pthread_mutex_t ss_id_mutex = PTHREAD_MUTEX_INITIALIZER;

int setup_ss_server() {
    printf("\n[NS-SS] Storage Server Listener Configuration:\n");
    printf("     - NS_SS_PORT: %d\n", NS_SS_PORT);
    printf("     - NS_SS_BACKLOG: %d\n", NS_SS_BACKLOG);
    printf("     - NS_SS_BUFFER_SIZE: %d\n\n", NS_SS_BUFFER_SIZE);
    
    // Initialize server socket for Storage Servers
    printf("[NS-SS] Initializing Storage Server listener on port %d...\n", NS_SS_PORT);
    int server_fd = init_server(NS_SS_PORT, NS_SS_BACKLOG);
    
    if (server_fd < 0) {
        fprintf(stderr, "[NS-SS ERROR] Failed to initialize SS server: %s\n", get_socket_error());
        return -1;
    }
    
    printf("[NS-SS] Storage Server listener initialized successfully!\n");
    printf("[NS-SS] Waiting for Storage Server connections...\n");
    printf("═══════════════════════════════════════════\n\n");
    
    // Set to non-blocking mode
    set_socket_nonblocking(server_fd);
    
    return server_fd;
}

void* handle_storage_server(void* arg) {
    SSThreadData* data = (SSThreadData*)arg;
    int ss_fd = data->ss_fd;
    Connection ss_conn = data->ss_conn;
    int ss_id = data->ss_id;
    
    // Free the allocated data structure
    free(data);
    
    // Detach thread so resources are automatically freed
    pthread_detach(pthread_self());
    
    printf("\n[NS-SS] Thread created for Storage Server #%d (%s:%d)\n", 
           ss_id, ss_conn.ip_address, ss_conn.port);
    
    // Handle SS messages
    char buffer[NS_SS_BUFFER_SIZE];
    bool ss_connected = true;
    int message_count = 0;
    
    while (ss_connected && running) {
        // Receive message from Storage Server
        int bytes_received = recv_message(ss_fd, buffer, NS_SS_BUFFER_SIZE);
        
        if (bytes_received == NET_CLOSED) {
            printf("\n[NS-SS] Storage Server #%d (%s:%d) disconnected gracefully.\n", 
                   ss_id, ss_conn.ip_address, ss_conn.port);
            ss_connected = false;
            break;
        }
        
        if (bytes_received < 0) {
            fprintf(stderr, "[NS-SS ERROR] Failed to receive message from SS #%d (%s:%d): %s\n",
                    ss_id, ss_conn.ip_address, ss_conn.port, get_socket_error());
            ss_connected = false;
            break;
        }
        
        message_count++;
        
        // Print received message
        printf("\n┌─ SS Message #%d from Storage Server #%d (%s:%d) ─────\n", 
               message_count, ss_id, ss_conn.ip_address, ss_conn.port);
        printf("│ Length: %d bytes\n", bytes_received);
        printf("│ Content: %s\n", buffer);
        printf("└────────────────────────────────────────────────────\n");
        
        // TODO: Parse and handle SS registration/commands here
        // For now, just send acknowledgment
        
        // Send acknowledgment back to Storage Server
        char ack[256];
        snprintf(ack, sizeof(ack), "ACK: Message received by NS from SS #%d", ss_id);
        int bytes_sent = send_message(ss_fd, ack);
        
        if (bytes_sent < 0) {
            fprintf(stderr, "[NS-SS ERROR] Failed to send acknowledgment to SS #%d: %s\n",
                    ss_id, get_socket_error());
            ss_connected = false;
            break;
        }
        
        printf("[NS-SS] ✓ Acknowledgment sent to Storage Server #%d\n", ss_id);
    }
    
    // Close SS connection
    close_socket(ss_fd);
    printf("[NS-SS] Connection with Storage Server #%d (%s:%d) closed.\n", 
           ss_id, ss_conn.ip_address, ss_conn.port);
    printf("═══════════════════════════════════════════\n");
    
    return NULL;
}

void* accept_storage_servers(void* arg) {
    int server_fd = *((int*)arg);
    
    pthread_detach(pthread_self());
    
    printf("[NS-SS] Storage Server acceptance thread started\n");
    printf("═══════════════════════════════════════════\n\n");
    
    while (running) {
        // Accept incoming Storage Server connection
        Connection ss_conn;
        int ss_fd = accept_connection(server_fd, &ss_conn);
        
        if (ss_fd < 0) {
            if (!running) break;  // Exit if shutdown signal received
            
            // Check if it's just a non-blocking wait (no SS yet)
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000);  // Sleep for 100ms to avoid busy waiting
                continue;
            }
            
            fprintf(stderr, "[NS-SS ERROR] Failed to accept SS connection: %s\n", get_socket_error());
            continue;
        }
        
        // Assign unique ID to this Storage Server
        pthread_mutex_lock(&ss_id_mutex);
        int ss_id = ++ss_id_counter;
        pthread_mutex_unlock(&ss_id_mutex);
        
        printf("\n[NS-SS] ✓ New Storage Server #%d connected from %s:%d\n", 
               ss_id, ss_conn.ip_address, ss_conn.port);
        printf("═══════════════════════════════════════════\n");
        
        // Allocate memory for thread data
        SSThreadData* thread_data = malloc(sizeof(SSThreadData));
        if (!thread_data) {
            fprintf(stderr, "[NS-SS ERROR] Failed to allocate memory for SS thread data\n");
            close_socket(ss_fd);
            continue;
        }
        
        thread_data->ss_fd = ss_fd;
        thread_data->ss_conn = ss_conn;
        thread_data->ss_id = ss_id;
        
        // Create a new thread to handle this Storage Server
        pthread_t thread_id;
        int result = pthread_create(&thread_id, NULL, handle_storage_server, thread_data);
        
        if (result != 0) {
            fprintf(stderr, "[NS-SS ERROR] Failed to create thread for SS #%d: %s\n",
                    ss_id, strerror(result));
            close_socket(ss_fd);
            free(thread_data);
            continue;
        }
        
        printf("[NS-SS] Thread %lu created for Storage Server #%d\n",
               (unsigned long)thread_id, ss_id);
        printf("[NS-SS] Waiting for next Storage Server...\n\n");
    }
    
    printf("[NS-SS] Storage Server acceptance thread shutting down...\n");
    close_socket(server_fd);
    
    return NULL;
}