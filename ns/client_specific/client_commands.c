#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "client_commands.h"
#include "../registry/ss_selector.h"
#include "../registry/file_registry.h"
#include "../registry/ss_registry.h"
#include "../../api_c_ns/networking.h"


/**
 * Handle CREATE command from client
 */
 void handle_create_command(int client_fd, const char* file_path) {
    FileInfo* existing = find_file(file_path);
    
    if (existing != NULL && existing->is_active) {
        char response[512];
        snprintf(response, sizeof(response),
                "FILE_EXISTS %s %d",
                existing->ss_ip, existing->ss_client_port);
        
        send_message(client_fd, response);
        
        printf("[NS-Client][Client_commands] ⚠ File '%s' already exists on SS #%d\n",
               file_path, existing->ss_id);
        return;
    }
    
    StorageServerInfo* selected_ss = select_ss_for_file();
    
    if (selected_ss == NULL) {
        const char* error = "ERROR: No storage servers available";
        send_message(client_fd, error);
        return;
    }
    
    printf("[NS-Client][Client_commands] Selected SS #%d for file '%s'\n", 
           selected_ss->ss_id, file_path);
    
    // Send create request to SS
    if (!send_create_request_to_ss(selected_ss, file_path)) {
        const char* error = "ERROR: Failed to communicate with storage server";
        send_message(client_fd, error);
        return;
    }
    
    // Wait for SS response
    char error_msg[512];
    if (!wait_for_ss_response(selected_ss, error_msg, sizeof(error_msg))) {
        send_message(client_fd, error_msg);
        return;
    }
    
    // Register file and send success response
    if (register_file(file_path, selected_ss->ss_id,
                    selected_ss->ip_address,
                    selected_ss->client_port) == 0) {
        
        char response[512];
        snprintf(response, sizeof(response),
                "SS_INFO %s %d",
                selected_ss->ip_address,
                selected_ss->client_port);
        
        send_message(client_fd, response);
        
        printf("[NS-Client][Client_commands] ✓ File '%s' created on SS #%d\n",
               file_path, selected_ss->ss_id);
    } else {
        const char* error = "ERROR: Failed to register file";
        send_message(client_fd, error);
    }
}

/**
 * Send CREATE request to Storage Server
 */
 bool send_create_request_to_ss(StorageServerInfo* ss, const char* file_path) {
    ProtocolMessage ns_msg;
    memset(&ns_msg, 0, sizeof(ProtocolMessage));
    ns_msg.type = htonl(MSG_CREATE_FILE);
    ns_msg.status = 0;
    strncpy(ns_msg.data, file_path, sizeof(ns_msg.data) - 1);
    
    printf("[DEBUG NS] About to send ProtocolMessage:\n");
    printf("  - sizeof(ProtocolMessage) = %zu bytes\n", sizeof(ProtocolMessage));
    printf("  - ns_msg.type (raw) = %d\n", ns_msg.type);
    printf("  - ns_msg.data = '%s' (length: %zu)\n", ns_msg.data, strlen(ns_msg.data));
    
    printf("[NS→SS #%d] Sending CREATE for '%s'\n", ss->ss_id, file_path);
    
    ssize_t sent = send(ss->ss_fd, &ns_msg, sizeof(ProtocolMessage), 0);
    
    printf("[DEBUG NS] send() returned: %zd bytes\n", sent);
    
    return (sent == sizeof(ProtocolMessage));
}

/**
 * Wait for and process Storage Server response
 */
 bool wait_for_ss_response(StorageServerInfo* ss, char* error_msg, size_t error_msg_size) {
    ProtocolMessage ss_response;
    ssize_t received = recv(ss->ss_fd, &ss_response, sizeof(ProtocolMessage), 0);
    
    if (received != sizeof(ProtocolMessage)) {
        printf("[NS-Client ERROR] Invalid response from SS #%d\n", ss->ss_id);
        snprintf(error_msg, error_msg_size, "ERROR: Storage server communication failed");
        return false;
    }
    
    int response_status = ntohl(ss_response.status);
    
    printf("[NS←SS #%d] Response: status=%d, msg=%s\n", 
           ss->ss_id, response_status, ss_response.message);
    
    if (response_status != 0) {
        snprintf(error_msg, error_msg_size, "ERROR: %s", ss_response.message);
        return false;
    }
    
    return true;
}

/**
 * Handle READ/WRITE/DELETE/INFO commands from client
 */
 void handle_file_operation_command(int client_fd, const char* file_path, const char* command) {
    FileInfo* file_info = find_file(file_path);
    
    if (file_info == NULL || !file_info->is_active) {
        const char* error = "ERROR: File not found";
        send_message(client_fd, error);
        printf("[NS-Client] File '%s' not found\n", file_path);
        return;
    }
    
    char response[512];
    snprintf(response, sizeof(response),
            "SS_INFO %s %d",
            file_info->ss_ip, file_info->ss_client_port);
    
    send_message(client_fd, response);
    
    printf("[NS-Client] ✓ Returned SS info for '%s' (command: %s)\n", file_path, command);
}

/**
 * Parse and dispatch client command
 */
 void process_client_command(int client_fd, const char* buffer, Connection client_conn) {
    char command[32], file_path[256];
    
    if (sscanf(buffer, "%s %s", command, file_path) < 2) {
        const char* error = "ERROR: Invalid command format";
        send_message(client_fd, error);
        return;
    }
    
    printf("[NS-Client][Client_commands]\n");
    printf("\n┌─ Client Request (%s:%d) ─────\n", 
           client_conn.ip_address, client_conn.port);
    printf("│ Content: %s\n", buffer);
    printf("└──────────────────────────────\n");
    
    if (strcmp(command, "CREATE") == 0) {

        // If the request is CREATE then handle create command
        handle_create_command(client_fd, file_path);

    } else if (strcmp(command, "READ") == 0 || 
               strcmp(command, "WRITE") == 0 ||
               strcmp(command, "DELETE") == 0 ||
               strcmp(command, "INFO") == 0) {
        // If the request is READ/WRITE/DELETE/INFO then handle file operation command. 
        handle_file_operation_command(client_fd, file_path, command);
    } else {
        const char* response = "ERROR: Unknown command";
        send_message(client_fd, response);
    }
}
