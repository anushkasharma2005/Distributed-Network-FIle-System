#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "handle_client_server.h"
#include "client_commands.h"
#include "../conn.h"
#include "../../include/constants.h"
#include "../../api_c_ns/naming_server.h"
#include "../../api_c_ns/networking.h"
#include "../registry/ss_registry.h"
#include "../registry/file_registry.h"
#include "../registry/ss_selector.h"
#include "../../api_ns_ss/ns_ss_connection.h"
#include "../registry/user_registry.h"



static int first_time = 1;

int setup_client_server() {
    printf("═══════════════════════════════════════════\n");
    printf("\n[NS][Handle_Client] Client Listener Configuration:\n");
    printf("     - NS_CLIENT_PORT: %d\n", NS_CLIENT_PORT);
    printf("     - NS_CLIENT_BACKLOG: %d\n", NS_CLIENT_BACKLOG);
    printf("     - NS_CLIENT_BUFFER_SIZE: %d\n\n", NS_CLIENT_BUFFER_SIZE);
    
    // Initialize server socket for Storage Servers
    printf("[NS][Handle_Client] Initializing Client Server listener on port %d...\n", NS_SS_PORT);
    int server_fd = init_server(NS_CLIENT_PORT, NS_CLIENT_BACKLOG);

    if (server_fd < 0) {
        fprintf(stderr, "[NS ERROR][Handle_Client] Failed to initialize Client server: %s\n", get_socket_error());
        return -1;
    }

    printf("[NS][Handle_Client] Client listener initialized successfully!\n");
    printf("[NS][Handle_Client] Waiting for Client connections...\n");
    printf("═══════════════════════════════════════════\n\n");
    
    // Set to non-blocking mode
    set_socket_nonblocking(server_fd);
    
    return server_fd;
}

// ADD THIS DEBUG:
 void print_protocol_message_layout(void) {
    printf("[DEBUG][NS][Handle_Client] ProtocolMessage layout:\n");
    printf("  - sizeof(ProtocolMessage) = %zu\n", sizeof(ProtocolMessage));
    printf("  - sizeof(int) = %zu\n", sizeof(int));
    printf("  - MAX_BUFFER_SIZE = %d\n", MAX_BUFFER_SIZE);
    printf("  - offset of type: %zu\n", offsetof(ProtocolMessage, type));
    printf("  - offset of status: %zu\n", offsetof(ProtocolMessage, status));
    printf("  - offset of data: %zu\n", offsetof(ProtocolMessage, data));
    printf("  - offset of message: %zu\n", offsetof(ProtocolMessage, message));
}


/*
 * Accept incoming client connections
 * Creates a new thread for each client using handle_client()
 * @param arg Pointer to client server file descriptor (int*)
 */
void* accept_clients(void* arg) { 
    int server_fd = *((int*)arg);
     
    pthread_detach(pthread_self());
    
    printf("═════════════════════════════════════════════════\n");
    printf("[NS][Handle_Client] Client acceptance thread started\n");
    printf("[NS][Handle_Client] Now accepting client connections...\n");
    printf("═════════════════════════════════════════════════\n\n");
    
    while (running) {
        
        // Accept incoming client connection
        Connection client_conn;
        int client_fd = accept_connection(server_fd, &client_conn);

        if (client_fd < 0) {
            if (!running) break;
            
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000);  // Sleep for 100ms to avoid busy waiting
                continue;
            }

            fprintf(stderr, "[NS ERROR][Handle_Client] Failed to accept connection: %s\n", get_socket_error());
            continue;
        }

        printf("\n[NS][Handle_Client] ✓ New client connected from %s:%d\n", 
               client_conn.ip_address, client_conn.port);
        printf("═══════════════════════════════════════════\n");

        // Allocate memory for thread data
        ClientThreadData_NS* thread_data = malloc(sizeof(ClientThreadData_NS));
        if (!thread_data) {
            fprintf(stderr, "[NS ERROR][Handle_Client] Failed to allocate memory for thread data\n");
            close_socket(client_fd);
            continue;
        }

        thread_data->client_fd = client_fd;
        thread_data->client_conn = client_conn;

        // Create a new thread to handle this client
        pthread_t thread_id;
        int result = pthread_create(&thread_id, NULL, handle_client, thread_data);
        
        if (result != 0) {
            fprintf(stderr, "[NS ERROR][Handle_Client] Failed to create thread for client %s:%d: %s\n",
                    client_conn.ip_address, client_conn.port, strerror(result));
            close_socket(client_fd);
            free(thread_data);
            continue;
        }

        printf("═══════════════════════════════════════════\n");
        printf("[NS][Handle_Client] Thread %lu created for client %s:%d\n",
               (unsigned long)thread_id, client_conn.ip_address, client_conn.port);
        printf("[NS][Handle_Client] Waiting for next client...\n");
        printf("═══════════════════════════════════════════\n");

    }
    
    printf("═══════════════════════════════════════════\n");
    printf("[NS-Client][Handle_Client] Client acceptance thread shutting down...\n");
    close_socket(server_fd);
    printf("═══════════════════════════════════════════\n");
    
    return NULL;
}


/**
 * Main client handler thread function
 */
void* handle_client(void* arg) {
    if (first_time) {
        print_protocol_message_layout();
        first_time = 0;
    }

    ClientThreadData_NS* data = (ClientThreadData_NS*)arg;
    int client_fd = data->client_fd;
    Connection client_conn = data->client_conn;
    
    free(data);
    pthread_detach(pthread_self());

    printf("\n[NS-Client][Handle_Client] Thread running for client (%s:%d)\n", 
           client_conn.ip_address, client_conn.port);
    
    char buffer[NS_CLIENT_BUFFER_SIZE];
    bool client_connected = true;
    

    // handle client identification
    char username[MAX_USERNAME_LENGTH] = {0};
    int bytes_received = recv_message(client_fd, buffer, NS_CLIENT_BUFFER_SIZE);
    
    if (bytes_received <= 0) {
        printf("[NS-Client][Handle_Client] Failed to receive username from client\n");
        close_socket(client_fd);
        return NULL;
    }
    // Store username (trim whitespace)
    strncpy(username, buffer, MAX_USERNAME_LENGTH - 1);
    username[strcspn(username, "\r\n")] = 0;  // Remove newlines
    
    register_user(username); // Register user in the user registry

    printf("[NS-Client][Handle_Client] ✓ Client identified as '%s' (%s:%d)\n",
           username, client_conn.ip_address, client_conn.port);

    while (client_connected && running) {
        int bytes_received = recv_message(client_fd, buffer, NS_CLIENT_BUFFER_SIZE);
        
        // GIVE UP ATTITUDE :(
        if (bytes_received <= 0) {
            printf("[NS-Client][Handle_Client] Client (%s:%d) disconnected\n",
                   client_conn.ip_address, client_conn.port);
            client_connected = false;
            break;
        }

        process_client_command(client_fd, buffer, client_conn, username);
    }
    
    close_socket(client_fd);
    printf("[NS-Client][Handle_Client] Connection closed (%s:%d)\n",
           client_conn.ip_address, client_conn.port);
    
    return NULL;
}


