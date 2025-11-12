#include "handle_client.h"
#include "../include/constants.h"
#include "ss_registry.h"
#include "file_registry.h"
#include "ss_selector.h"
#include "../api_ns_ss/ns_ss_connection.h"  
#include <string.h>
#include <arpa/inet.h>


// Message types from ns_ss_connection.h
#define MSG_CREATE_FILE 2
#define MSG_DELETE_FILE 3
#define MSG_FILE_OP_ACK 3


int setup_client_server() {
    printf("═══════════════════════════════════════════\n");
    printf("\n[NS-Client] Client Listener Configuration:\n");
    printf("     - NS_CLIENT_PORT: %d\n", NS_CLIENT_PORT);
    printf("     - NS_CLIENT_BACKLOG: %d\n", NS_CLIENT_BACKLOG);
    printf("     - NS_CLIENT_BUFFER_SIZE: %d\n\n", NS_CLIENT_BUFFER_SIZE);
    
    // Initialize server socket for Storage Servers
    printf("[NS-SS] Initializing Storage Server listener on port %d...\n", NS_SS_PORT);
    int server_fd = init_server(NS_CLIENT_PORT, NS_CLIENT_BACKLOG);

    if (server_fd < 0) {
        fprintf(stderr, "[NS-Client ERROR] Failed to initialize Client server: %s\n", get_socket_error());
        return -1;
    }

    printf("[NS-Client] Client listener initialized successfully!\n");
    printf("[NS-Client] Waiting for Client connections...\n");
    printf("═══════════════════════════════════════════\n\n");
    
    // Set to non-blocking mode
    set_socket_nonblocking(server_fd);
    
    return server_fd;
}

// ADD THIS DEBUG:
static void print_protocol_message_layout(void) {
    printf("[DEBUG] ProtocolMessage layout:\n");
    printf("  - sizeof(ProtocolMessage) = %zu\n", sizeof(ProtocolMessage));
    printf("  - sizeof(int) = %zu\n", sizeof(int));
    printf("  - MAX_BUFFER_SIZE = %d\n", MAX_BUFFER_SIZE);
    printf("  - offset of type: %zu\n", offsetof(ProtocolMessage, type));
    printf("  - offset of status: %zu\n", offsetof(ProtocolMessage, status));
    printf("  - offset of data: %zu\n", offsetof(ProtocolMessage, data));
    printf("  - offset of message: %zu\n", offsetof(ProtocolMessage, message));
}




void* handle_client(void* arg) {

    static int first_time = 1;
    if (first_time) {
        print_protocol_message_layout();
        first_time = 0;
    }


    ClientThreadData* data = (ClientThreadData*)arg;
    int client_fd = data->client_fd;
    Connection client_conn = data->client_conn;
    
    free(data);
    pthread_detach(pthread_self());
    
    printf("\n[NS-Client] Thread created for client (%s:%d)\n", 
           client_conn.ip_address, client_conn.port);
    
    char buffer[NS_CLIENT_BUFFER_SIZE];
    bool client_connected = true;
    
    while (client_connected && running) {
        int bytes_received = recv_message(client_fd, buffer, NS_CLIENT_BUFFER_SIZE);
        
        if (bytes_received <= 0) {
            printf("[NS-Client] Client (%s:%d) disconnected\n",
                   client_conn.ip_address, client_conn.port);
            client_connected = false;
            break;
        }
        
        printf("\n┌─ Client Request (%s:%d) ─────\n", 
               client_conn.ip_address, client_conn.port);
        printf("│ Content: %s\n", buffer);
        printf("└──────────────────────────────\n");
        
        // Parse command
        char command[32], file_path[256];
        if (sscanf(buffer, "%s %s", command, file_path) < 2) {
            const char* error = "ERROR: Invalid command format";
            send_message(client_fd, error);
            continue;
        }
        
        if (strcmp(command, "CREATE") == 0) {
            FileInfo* existing = find_file(file_path);
            
            if (existing != NULL && existing->is_active) {
                char response[512];
                snprintf(response, sizeof(response),
                        "FILE_EXISTS %s %d",
                        existing->ss_ip, existing->ss_client_port);
                
                send_message(client_fd, response);
                
                printf("[NS-Client] ⚠ File '%s' already exists on SS #%d\n",
                       file_path, existing->ss_id);
                       
            } else {
                StorageServerInfo* selected_ss = select_ss_for_file();
                
                if (selected_ss == NULL) {
                    const char* error = "ERROR: No storage servers available";
                    send_message(client_fd, error);
                    continue;
                }
                
                printf("[NS-Client] Selected SS #%d for file '%s'\n", 
                       selected_ss->ss_id, file_path);
                
                // Create ProtocolMessage matching SS structure
                ProtocolMessage ns_msg;
                memset(&ns_msg, 0, sizeof(ProtocolMessage));
                ns_msg.type = htonl(MSG_CREATE_FILE);
                ns_msg.status = 0;
                strncpy(ns_msg.data, file_path, sizeof(ns_msg.data) - 1);
                
                // DEBUG: Print what we're about to send
                printf("[DEBUG NS] About to send ProtocolMessage:\n");
                printf("  - sizeof(ProtocolMessage) = %zu bytes\n", sizeof(ProtocolMessage));
                printf("  - ns_msg.type (raw) = %d\n", ns_msg.type);
                printf("  - ns_msg.type (after htonl) = %d\n", htonl(MSG_CREATE_FILE));
                printf("  - ns_msg.status = %d\n", ns_msg.status);
                printf("  - ns_msg.data = '%s' (length: %zu)\n", ns_msg.data, strlen(ns_msg.data));
                printf("  - First 32 bytes of data field (hex): ");
                for (int i = 0; i < 32 && i < (int)sizeof(ns_msg.data); i++) {
                    printf("%02x ", (unsigned char)ns_msg.data[i]);
                }
                printf("\n");
                
                printf("[NS→SS #%d] Sending CREATE for '%s'\n", 
                       selected_ss->ss_id, file_path);
                
                // Send full ProtocolMessage
                ssize_t sent = send(selected_ss->ss_fd, &ns_msg, sizeof(ProtocolMessage), 0);
                

                printf("[DEBUG NS] send() returned: %zd bytes\n", sent);
                if (sent != sizeof(ProtocolMessage)) {
                    printf("[NS-Client ERROR] Failed to send to SS #%d\n", selected_ss->ss_id);
                    const char* error = "ERROR: Failed to communicate with storage server";
                    send_message(client_fd, error);
                    continue;
                }
                
                // Wait for SS response
                ProtocolMessage ss_response;
                ssize_t received = recv(selected_ss->ss_fd, &ss_response, sizeof(ProtocolMessage), 0);
                
                if (received != sizeof(ProtocolMessage)) {
                    printf("[NS-Client ERROR] Invalid response from SS #%d\n", selected_ss->ss_id);
                    const char* error = "ERROR: Storage server communication failed";
                    send_message(client_fd, error);
                    continue;
                }
                
                int response_status = ntohl(ss_response.status);
                
                printf("[NS←SS #%d] Response: status=%d, msg=%s\n", 
                       selected_ss->ss_id, response_status, ss_response.message);
                
                // Check success (status == 0)
                if (response_status == 0) {
                    if (register_file(file_path, selected_ss->ss_id,
                                    selected_ss->ip_address,
                                    selected_ss->client_port) == 0) {
                        
                        char response[512];
                        snprintf(response, sizeof(response),
                                "SS_INFO %s %d",
                                selected_ss->ip_address,
                                selected_ss->client_port);
                        
                        send_message(client_fd, response);
                        
                        printf("[NS-Client] ✓ File '%s' created on SS #%d\n",
                               file_path, selected_ss->ss_id);
                    } else {
                        const char* error = "ERROR: Failed to register file";
                        send_message(client_fd, error);
                    }
                } else {
                    char error[512];
                    snprintf(error, sizeof(error),
                            "ERROR: %s", ss_response.message);
                    send_message(client_fd, error);
                }
            }
            
        } else if (strcmp(command, "READ") == 0 || 
                   strcmp(command, "WRITE") == 0 ||
                   strcmp(command, "DELETE") == 0 ||
                   strcmp(command, "INFO") == 0) {
            
            FileInfo* file_info = find_file(file_path);
            
            if (file_info == NULL || !file_info->is_active) {
                const char* error = "ERROR: File not found";
                send_message(client_fd, error);
                printf("[NS-Client] File '%s' not found\n", file_path);
            } else {
                char response[512];
                snprintf(response, sizeof(response),
                        "SS_INFO %s %d",
                        file_info->ss_ip, file_info->ss_client_port);
                
                send_message(client_fd, response);
                
                printf("[NS-Client] ✓ Returned SS info for '%s'\n", file_path);
            }
            
        } else {
            const char* response = "ERROR: Unknown command";
            send_message(client_fd, response);
        }
    }
    
    close_socket(client_fd);
    printf("[NS-Client] Connection closed (%s:%d)\n",
           client_conn.ip_address, client_conn.port);
    
    return NULL;
}



/*
 * Accept incoming client connections
 * Creates a new thread for each client using handle_client()
 * @param arg Pointer to client server file descriptor (int*)
 */
void* accept_clients(void* arg) { 
    int server_fd = *((int*)arg);
     
    pthread_detach(pthread_self());
    
    printf("═══════════════════════════════════════════\n");
    printf("[NS-Client] Client acceptance thread started\n");
    printf("[NS-Client] Now accepting client connections...\n");
    printf("═══════════════════════════════════════════\n\n");
    
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

        printf("═══════════════════════════════════════════\n");
        printf("[NS] Thread %lu created for client %s:%d\n",
               (unsigned long)thread_id, client_conn.ip_address, client_conn.port);
        printf("[NS] Waiting for next client...\n");
        printf("═══════════════════════════════════════════\n");

    }
    
    printf("═══════════════════════════════════════════\n");
    printf("[NS-Client] Client acceptance thread shutting down...\n");
    close_socket(server_fd);
    printf("═══════════════════════════════════════════\n");
    
    return NULL;
}
