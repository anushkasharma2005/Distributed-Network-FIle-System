#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "handle_ss.h"
#include "../../include/constants.h"
#include "../registry/ss_registry.h"
#include "conn.h"


// Counter for assigning unique IDs to storage servers
static int ss_id_counter = 0;
static pthread_mutex_t ss_id_mutex = PTHREAD_MUTEX_INITIALIZER;


/**
 * Setup and initialize the naming server for Storage Server connections
 * Uses configuration from constants.h and creates server socket
 * @return Server socket file descriptor on success, -1 on failure
 */
int setup_ss_server() {
    printf("═══════════════════════════════════════════\n");
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


/**
 * Thread function to accept incoming Storage Server connections
 * @param arg Pointer to the server socket file descriptor
 * @return NULL
 */
void* accept_storage_servers(void* arg) {

    int server_fd = *((int*)arg);
    
    pthread_detach(pthread_self());

    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║ [NS-SS] Storage Server acceptance thread started ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

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
        

        printf("════════════════════════════════════════════════════════════════\n");
        printf(" [NS-SS] ✓ New connection from %s:%d (pending identification)\n", 
            ss_conn.ip_address, ss_conn.port);
        printf("════════════════════════════════════════════════════════════════\n");


        // Allocate memory for thread data structure 
        SSThreadData* thread_data = malloc(sizeof(SSThreadData));
        if (!thread_data) {
            fprintf(stderr, "[NS-SS ERROR] Failed to allocate memory for SS thread data\n");
            close_socket(ss_fd);
            continue;
        }
        
        thread_data->ss_fd = ss_fd;
        thread_data->ss_conn = ss_conn;
        thread_data->ss_id = -1;  // Temporary ID, will be assigned upon registration
        
        // Create a new thread to handle this Storage Server
        pthread_t thread_id;
        int result = pthread_create(&thread_id, NULL, handle_storage_server, thread_data);
        
        if (result != 0) {
            fprintf(stderr, "[NS-SS ERROR] Failed to create thread: %s\n", strerror(result));
            close_socket(ss_fd);
            free(thread_data);
            continue;
        }

        printf("[NS-SS] Handler thread created for new connection\n");
        printf("[NS-SS] Waiting for next Storage Server...\n");
        printf("═══════════════════════════════════════════\n\n");

    }
    
    printf("[NS-SS] Storage Server acceptance thread shutting down...\n");
    close_socket(server_fd);
    
    return NULL;
}


/** 
 * Thread function to handle communication with a Storage Server
 */

void* handle_storage_server(void* arg) {

    SSThreadData* data = (SSThreadData*)arg; 
    int ss_fd = data->ss_fd;
    Connection ss_conn = data->ss_conn;
    int initial_ss_id = data->ss_id;  // CHANGE: may not be final ID

    // Free the allocated data structure
    free(data);
    
    // Detach thread so resources are automatically freed
    pthread_detach(pthread_self());

    char buffer[NS_SS_BUFFER_SIZE];
    int bytes_received = recv_message(ss_fd, buffer, NS_SS_BUFFER_SIZE);
        
    if (bytes_received <= 0) {
        close_socket(ss_fd);
        return NULL;
    }

    char cmd[32], ss_ip[16];
    int nm_port, client_port, num_paths;
    
    if (sscanf(buffer, "%s %s %d %d %d", cmd, ss_ip, &nm_port, &client_port, &num_paths) != 5) {
        fprintf(stderr, "[NS-SS ERROR] Invalid registration format\n");
        close_socket(ss_fd);
        return NULL;
    }
    



    printf("\n┌─ SS Message #1 from Storage Server (temp %s:%d) ─────\n", 
           ss_conn.ip_address, ss_conn.port);
    printf("│ Length: %d bytes\n", bytes_received);
    printf("│ Content: %s\n", buffer);
    printf("│ Extracted NM Port: %d ← USING THIS FOR IDENTIFICATION\n", nm_port);
    printf("└────────────────────────────────────────────────────\n");
    

    printf("\n┌─ Registration from %s:%d ─────\n", ss_conn.ip_address, ss_conn.port);
    printf("│ IP: %s, NM Port: %d, Client Port: %d\n", ss_ip, nm_port, client_port);
    printf("└────────────────────────────────────────────────────\n");

    // Step 2: Check if this is a reconnection
    StorageServerInfo* existing = find_storage_server_by_address(ss_ip, nm_port);
    
    int ss_id;
    bool is_reconnection = (existing != NULL);
    
    if (is_reconnection) {
        // REUSE old ID for reconnection
        ss_id = existing->ss_id;
        printf("[NS-SS] ⟳ RECONNECTION detected! Reusing ID #%d\n", ss_id);
        
        // Update existing entry
        existing->ss_fd = ss_fd;
        existing->client_port = client_port;
        existing->is_active = true;
        existing->last_connected = time(NULL);
        existing->reconnect_count++;
        existing->thread_id = pthread_self();
        
    } else {
        // NEW server - assign fresh ID
        ss_id = assign_new_ss_id();
        printf("[NS-SS] NEW server! Assigned ID #%d\n", ss_id);
        
        // Create new entry
        StorageServerInfo ss_info = {
            .ss_id = ss_id,
            .ss_fd = ss_fd,
            .nm_port = nm_port,
            .client_port = client_port,
            .is_active = true,
            .accessible_paths = NULL,
            .num_paths = 0,
            .thread_id = pthread_self(),
            .first_connected = time(NULL),
            .last_connected = time(NULL),
            .reconnect_count = 0
        };
        strncpy(ss_info.ip_address, ss_ip, sizeof(ss_info.ip_address) - 1);
        
        int result = register_or_reconnect_storage_server(&ss_info);
        if (result < 0) {
            fprintf(stderr, "[NS-SS ERROR] Failed to register new SS\n");
            close_socket(ss_fd);
            return NULL;
        }
    }

    printf("[NS-SS] Storage Server #%d (%s:%d) ready\n", ss_id, ss_ip, nm_port);
               
    // Send ACK
    const char* ack = "ACK";
    send_message(ss_fd, ack);
    printf("[NS-SS] ✓ Acknowledgment sent to Storage Server #%d\n", ss_id);
    
    // Handle remaining messages
    bool ss_connected = true;
    int message_count = 1;  // Already got first message
    
    while (ss_connected && running) {
        // Receive message from Storage Server
        int bytes_received = recv_message(ss_fd, buffer, NS_SS_BUFFER_SIZE);
        
        if (bytes_received == NET_CLOSED) {
            printf("\n[NS-SS] Storage Server #%d (%s:%d) disconnected gracefully.\n", 
                   ss_id, ss_ip, nm_port);
            ss_connected = false;
            break;
        }
        
        if (bytes_received < 0) {
            fprintf(stderr, "[NS-SS ERROR] Failed to receive message from SS #%d (%s:%d): %s\n",
                    ss_id, ss_ip, nm_port, get_socket_error());
            ss_connected = false;
            break;
        }
        
        message_count++;
        
        // Print received message
        printf("\n┌─ SS Message #%d from Storage Server #%d (%s:%d) ─────\n", 
               message_count, ss_id, ss_ip, nm_port);
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
    // On disconnect, unregister
    mark_ss_inactive(ss_id);

    close_socket(ss_fd);
    printf("[NS-SS] Connection with Storage Server #%d (%s:%d) closed.\n", 
           ss_id, ss_ip, nm_port);
    printf("═══════════════════════════════════════════\n");
    
    return NULL;
}


/**
 * Helper function to assign new SS ID (only for truly new servers)
 */
int assign_new_ss_id() {
    pthread_mutex_lock(&ss_id_mutex);
    int new_id = ++ss_id_counter;
    pthread_mutex_unlock(&ss_id_mutex);
    return new_id;
}