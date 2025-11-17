#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#include "client_commands.h"
#include "../registry/ss_selector.h"
#include "../registry/file_registry.h"
#include "../registry/ss_registry.h"
#include "../registry/user_registry.h"
#include "../../api_c_ns/networking.h"
#include "./handle_client_server.h"

/**
 * Handle CREATE command from client
 */
void handle_create_command(int client_fd, const char* file_path, const char* owner) {
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
                    selected_ss->client_port,
                    selected_ss->nm_port,
                    owner) == 0) {

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
    
    pthread_mutex_lock(&ss->socket_mutex);
    
    ProtocolMessage ns_msg;
    memset(&ns_msg, 0, sizeof(ProtocolMessage));
    ns_msg.type = htonl(MSG_CREATE_FILE);
    ns_msg.status = 0;
    strncpy(ns_msg.data, file_path, sizeof(ns_msg.data) - 1);
    
    bool result = send_protocol_message(ss->ss_fd, &ns_msg);
    
    if (!result) {
        // Send failed - mark SS inactive
        printf("[NS][Client_commands] Failed to send to SS #%d\n", ss->ss_id);
        pthread_mutex_unlock(&ss->socket_mutex);
        return false;
    }
    
    // Keep mutex locked - wait_for_ss_response will unlock
    return true;

}

/**
 * Wait for and process Storage Server response
 */
 bool wait_for_ss_response(StorageServerInfo* ss, char* error_msg, size_t error_msg_size) {
    
    // Mutex is already locked from send_create_request_to_ss
    
    ProtocolMessage ss_response;
    ssize_t received = recv(ss->ss_fd, &ss_response, sizeof(ProtocolMessage), 0);
    
    // Unlock immediately after recv
    pthread_mutex_unlock(&ss->socket_mutex);


    if (received == 0) {
        // Connection closed
        printf("[NS-SS] SS #%d disconnected during recv\n", ss->ss_id);
        mark_ss_inactive(ss->ip_address, ss->nm_port);
        snprintf(error_msg, error_msg_size, "ERROR: Storage server disconnected");
        return false;
    }

    
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
 void process_client_command(int client_fd, const char* buffer, Connection client_conn, const char* username) {
    char command[32], file_path[256];
    

    int parsed = sscanf(buffer, "%s %s", command, file_path);

    printf("[NS-Client][Client_commands]\n");
    printf("\n┌─ Client Request from '%s' (%s:%d) ─────\n",
           username, client_conn.ip_address, client_conn.port);
    printf("│ Content: %s\n", buffer);
    printf("└──────────────────────────────\n");
    

    if (strcmp(command, "LIST") == 0 || strcmp(command, "LIST_USERS") == 0) {
        handle_list_command(client_fd);
        return;
    }
    

    if (parsed < 2) {
        const char* error = "ERROR: Invalid command format";
        send_message(client_fd, error);
        return;
    }
    
  
    if (strcmp(command, "CREATE") == 0) {

        // If the request is CREATE then handle create command
        handle_create_command(client_fd, file_path,username);

    }else if (strcmp(command, "WRITE_LOCK") == 0) {

        handle_write_command(client_fd, file_path);

    }else if(strcmp(command, "INFO") == 0){

        // If the request is INFO then handle file operation command. 
        handle_info_command(client_fd, file_path);

    }else if (strcmp(command, "READ") == 0 || 
               strcmp(command, "DELETE") == 0) {

        // If the request is READ/WRITE/DELETE/INFO then handle file operation command. 
        handle_file_operation_command(client_fd, file_path, command);
    } else {
        const char* response = "ERROR: Unknown command";
        send_message(client_fd, response);
    }
}


/**
 * Handle WRITE command from client
 * NS returns SS info, client writes directly to SS
 */
void handle_write_command(int client_fd, const char* file_path) {
    printf("[NS-Client][WRITE] Client requested write access to '%s'\n", file_path);
    
    // 1. Look up file in registry
    FileInfo* file_info = find_file(file_path);
    
    if (file_info == NULL) {
        const char* error = "ERROR: File not found";
        send_message(client_fd, error);
        printf("[NS-Client][WRITE] ✗ File '%s' not found\n", file_path);
        return;
    }
    
    // 🔍 DEBUG: Print file info
    printf("[DEBUG][WRITE] File found:\n");
    printf("  - file_path: %s\n", file_info->file_path);
    printf("  - ss_id: %d\n", file_info->ss_id);
    printf("  - ss_ip: %s\n", file_info->ss_ip);
    printf("  - ss_client_port: %d\n", file_info->ss_client_port);
    printf("  - is_active: %s\n", file_info->is_active ? "TRUE" : "FALSE");
    



    if (!file_info->is_active) {
        const char* error = "ERROR: File has been deleted by the owner";
        send_message(client_fd, error);
        printf("[NS-Client][WRITE] ✗ File '%s' has been deleted by the owner\n", file_path);
        return;
    }
    
    // 2. Get SS information
    StorageServerInfo* ss = find_storage_server_by_address(file_info->ss_ip, file_info->ss_nm_port);

    // 🔍 DEBUG: Check if SS was found
    if (ss == NULL) {
        printf("[DEBUG][WRITE] ✗ find_storage_server(%d) returned NULL!\n", file_info->ss_id);
        const char* error = "ERROR: Storage server not found";
        send_message(client_fd, error);
        return;
    }




    if (ss == NULL || !ss->is_active) {
        const char* error = "ERROR: Storage server not available";
        send_message(client_fd, error);
        printf("[NS-Client][WRITE] ✗ SS #%d not available\n", file_info->ss_id);
        return;
    }
    
    // 3. Send SS details to client
    char response[512];
    snprintf(response, sizeof(response),
             "SS_INFO %s %d",
             ss->ip_address,
             ss->client_port);
    
    send_message(client_fd, response);
    
    printf("[NS-Client][WRITE] ✓ Directed client to SS #%d (%s:%d) for '%s'\n",
           ss->ss_id, ss->ip_address, ss->client_port, file_path);
    
    // 4. Update last accessed time
    file_info->last_accessed = time(NULL);
}


void handle_info_command(int client_fd, const char* file_path) {
    printf("[NS-Client][INFO] Client requested info for '%s'\n", file_path);
    
    // 1. Look up file in registry
    FileInfo* file_info = find_file(file_path);
    
    if (file_info == NULL) {
        const char* error = "ERROR: File not found";
        send_message(client_fd, error);
        printf("[NS-Client][INFO] ✗ File '%s' not found\n", file_path);
        return;
    }
    
    if (!file_info->is_active) {
        const char* error = "ERROR: File is not currently available";
        send_message(client_fd, error);
        printf("[NS-Client][INFO] ✗ File '%s' is inactive\n", file_path);
        return;
    }
    
    // 2. Format timestamps
    char created_str[64], accessed_str[64];
    struct tm* created_tm = localtime(&file_info->created_at);
    struct tm* accessed_tm = localtime(&file_info->last_accessed);
    
    strftime(created_str, sizeof(created_str), "%Y-%m-%d %H:%M", created_tm);
    strftime(accessed_str, sizeof(accessed_str), "%Y-%m-%d %H:%M", accessed_tm);
    
    // 3. Build response in the format client expects
    char response[MAX_BUFFER_SIZE];
    snprintf(response, MAX_BUFFER_SIZE,
             "--> File: %s\n"
             "--> Owner: %s\n"
             "--> Created: %s\n"
             "--> Last Modified: %s\n"
             "--> Size: N/A\n"
             "--> Access: %s (RW)\n"
             "--> Last Accessed: %s by %s",
             file_path,
             file_info->owner ? file_info->owner : "unknown",
             created_str,
             created_str,  // Using created time as modified time (no separate field yet)
             file_info->owner ? file_info->owner : "unknown",
             accessed_str,
             file_info->owner ? file_info->owner : "unknown");
    
    // 4. Send response to client
    send_message(client_fd, response);
    
    printf("[NS-Client][INFO] ✓ Sent file info to client\n");
    printf("  - File: %s\n", file_path);
    printf("  - Owner: %s\n", file_info->owner);
    printf("  - Created: %s\n", created_str);
    printf("  - Last Accessed: %s\n", accessed_str);
}

/**
 * Handle LIST command - returns list of all users/owners
 */
void handle_list_command(int client_fd) {
    printf("[NS-Client][LIST] Client requested user list\n");
    
    int user_count = get_user_count();
    
    if (user_count == 0) {
        const char* msg = "No users registered";
        send_message(client_fd, msg);
        printf("[NS-Client][LIST] ✗ No users found\n");
        return;
    }
    
    // Get user pointers (don't free these - they point to registry's internal data)
    char* users[MAX_USERS];
    int count = get_all_users(users, MAX_USERS);
    
    // Build response string
    char response[MAX_BUFFER_SIZE];
    int offset = 0;
    
    for (int i = 0; i < count; i++) {
        offset += snprintf(response + offset, MAX_BUFFER_SIZE - offset, 
                          "--> %s\n", users[i]);
        
        if (offset >= MAX_BUFFER_SIZE - 100) {
            fprintf(stderr, "[NS-Client][LIST] Warning: Response truncated\n");
            break;
        }
    }
    
    // Remove trailing newline
    if (offset > 0 && response[offset - 1] == '\n') {
        response[offset - 1] = '\0';
    }
    
    // Send response
    send_message(client_fd, response);
    
    printf("[NS-Client][LIST] ✓ Sent %d users to client\n", count);
}