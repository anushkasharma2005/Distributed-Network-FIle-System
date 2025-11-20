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
 * Parse and dispatch client command
 */
 void process_client_command(int client_fd, const char* buffer, Connection client_conn, const char* username) {
    char command[32], arg1[256],  arg2[256], arg3[256];
    

    int parsed = sscanf(buffer, "%s %s %s %s", command, arg1, arg2, arg3);

    printf("[NS-Client][Client_commands]\n");
    printf("\n┌─ Client Request from '%s' (%s:%d) ─────\n",
           username, client_conn.ip_address, client_conn.port);
    printf("│ Content: %s\n", buffer);
    printf("└──────────────────────────────\n");
    

    if (strcmp(command, "LIST") == 0 || strcmp(command, "LIST_USERS") == 0) {
        handle_list_command(client_fd);
        return;
    }
    

    // ADDACCESS: arg1=filename, arg2=username, arg3=access_type
    if (strcmp(command, "ADDACCESS") == 0) {
        if (parsed < 4) {
            send_message(client_fd, "ERROR: Usage: ADDACCESS <filename> <username> <R|W>");
            return;
        }
        handle_addaccess_command(client_fd, arg1, arg2, arg3[0], username);
        return;
    }
    
    // REMACCESS: arg1=filename, arg2=username
    if (strcmp(command, "REMACCESS") == 0) {
        if (parsed < 3) {
            send_message(client_fd, "ERROR: Usage: REMACCESS <filename> <username>");
            return;
        }
        handle_remaccess_command(client_fd, arg1, arg2, username);
        return;
    }


    if (parsed < 2) {
        const char* error = "ERROR: Invalid command format";
        send_message(client_fd, error);
        return;
    }
    
    // For clarity, create file_path alias pointing to arg1
    const char* file_path = arg1;   

    if (strcmp(command, "CREATEFOLDER") == 0) {
        if (parsed < 2) {
            send_message(client_fd, "ERROR: Usage: CREATEFOLDER <folderpath>");
            return;
        }
        handle_createfolder_command(client_fd, arg1, username);
        return;
    }
    
    if (strcmp(command, "MOVE") == 0) {
        if (parsed < 3) {
            send_message(client_fd, "ERROR: Usage: MOVE <filename> <folderpath>");
            return;
        }
        handle_move_command(client_fd, arg1, arg2, username);
        return;
    }
    
    if (strcmp(command, "VIEWFOLDER") == 0) {
        if (parsed < 2) {
            send_message(client_fd, "ERROR: Usage: VIEWFOLDER <folderpath>");
            return;
        }
        handle_viewfolder_command(client_fd, arg1, username);
        return;
    }
    
    if (strcmp(command, "CREATE") == 0) {

        // If the request is CREATE then handle create command
        handle_create_command(client_fd, file_path,username);

    }else if (strcmp(command, "WRITE_LOCK") == 0) {

        if (!has_write_access(file_path, username)) {
            send_message(client_fd, "ERROR: Write access denied");
            printf("[NS-Client][WRITE] ✗ Write access denied for '%s' by '%s'\n", file_path, username);
            return;
        }

        handle_write_command(client_fd, file_path);

    }else if (strcmp(command, "VIEW") == 0) {
        // Client sends: "VIEW flag_all flag_list"
        // Examples: "VIEW 0 0", "VIEW 1 0", "VIEW 0 1", "VIEW 1 1"
        char flags[32] = "";
        
        if (parsed >= 3) {
            // Both flags present: arg1=flag_all, arg2=flag_list
            snprintf(flags, sizeof(flags), "%s %s", arg1, arg2);
        } else if (parsed >= 2) {
            // Only one flag: arg1=flag_all
            snprintf(flags, sizeof(flags), "%s 0", arg1);
        } else {
            // No flags: default to "0 0"
            strcpy(flags, "0 0");
        }
        
        handle_view_command(client_fd, flags, username);

        return;

    }else if(strcmp(command, "INFO") == 0){

        // If the request is INFO then handle file operation command. 
        handle_info_command(client_fd, file_path);

    }else if (strcmp(command, "DELETE") == 0) {
        // Only owner can delete
        if (!is_file_owner(file_path, username)) {
            send_message(client_fd, "ERROR: Only file owner can delete");
            return;
        }
        handle_file_operation_command(client_fd, file_path, command);


    }else if (strcmp(command, "READ") == 0 || 
               strcmp(command, "STREAM") == 0) {

        if (!has_read_access(file_path, username)) {
            send_message(client_fd, "ERROR: Read access denied");
            printf("[NS-Client][READ] ✗ Read access denied for '%s' by '%s'\n", file_path, username);
            return;
        }

        // If the request is READ/STREAM then handle file operation command. 
        handle_file_operation_command(client_fd, file_path, command);
    }else if ( strcmp(command, "UNDO") == 0) {

        if (!has_write_access(file_path, username)) {
            send_message(client_fd, "ERROR: Undo access denied");
            printf("[NS-Client][WRITE] ✗ Undo access denied for '%s' by '%s'\n", file_path, username);
            return;
        }
        
        handle_file_operation_command(client_fd, file_path, command);
    }else {
        const char* response = "ERROR: Unknown command";
        send_message(client_fd, response);
    }
}





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
 * Handle READ/STREAM/UNDO commands from client
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
    
    // //  DEBUG: Print file info
    // printf("[DEBUG][WRITE] File found:\n");
    // printf("  - file_path: %s\n", file_info->file_path);
    // printf("  - ss_id: %d\n", file_info->ss_id);
    // printf("  - ss_ip: %s\n", file_info->ss_ip);
    // printf("  - ss_client_port: %d\n", file_info->ss_client_port);
    // printf("  - is_active: %s\n", file_info->is_active ? "TRUE" : "FALSE");
    



    if (!file_info->is_active) {
        const char* error = "ERROR: File has been deleted by the owner";
        send_message(client_fd, error);
        printf("[NS-Client][WRITE] ✗ File '%s' has been deleted by the owner\n", file_path);
        return;
    }
    
    // 2. Get SS information
    StorageServerInfo* ss = find_storage_server_by_address(file_info->ss_ip, file_info->ss_nm_port);

    //  DEBUG: Check if SS was found
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

    char access_list[512];
    format_access_list(file_info, access_list, sizeof(access_list));
    


    
    // 3. Build response in the format client expects
    char response[MAX_BUFFER_SIZE];
    snprintf(response, MAX_BUFFER_SIZE,
             "--> File: %s\n"
             "--> Owner: %s\n"
             "--> Created: %s\n"
             "--> Last Modified: %s\n"
             "--> Size: N/A\n"
             "--> Access: %s\n"
             "--> Last Accessed: %s by %s",
             file_path,
             file_info->owner ? file_info->owner : "unknown",
             created_str,
             created_str,  // Using created time as modified time (no separate field yet)
             access_list,
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


/**
 * Handle ADDACCESS command
 */
void handle_addaccess_command(int client_fd, const char* file_path, 
                              const char* username, char access_type, 
                              const char* requester) {
    printf("[NS-Client][ADDACCESS] %s requesting %c access for %s on '%s'\n",
           requester, access_type, username, file_path);
    
    // 1. Check if file exists
    FileInfo* file = find_file(file_path);
    if (!file) {
        send_message(client_fd, "ERROR: File not found");
        return;
    }
    
    // 2. Check if requester is the owner
    if (!is_file_owner(file_path, requester)) {
        send_message(client_fd, "ERROR: Only file owner can modify access");
        return;
    }
    
    // 3. Validate access type
    if (access_type != 'R' && access_type != 'W') {
        send_message(client_fd, "ERROR: Invalid access type. Use R or W");
        return;
    }
    
    // 4. Check if user exists
    if (!user_exists(username)) {
        send_message(client_fd, "ERROR: User does not exist");
        return;
    }
    
    // 5. Add access
    int result;
    if (access_type == 'R') {
        result = add_read_access(file_path, username);
    } else {
        result = add_write_access(file_path, username);
    }
    
    if (result == 0) {
        char response[256];
        snprintf(response, sizeof(response), 
                "SUCCESS: %s access granted to %s for '%s'",
                access_type == 'R' ? "Read" : "Write", username, file_path);
        send_message(client_fd, response);
        printf("[NS-Client][ADDACCESS] ✓ Access granted\n");
    } else {
        send_message(client_fd, "ERROR: Failed to add access");
    }
}

/**
 * Handle REMACCESS command
 */
void handle_remaccess_command(int client_fd, const char* file_path, 
                              const char* username, const char* requester) {
    printf("[NS-Client][REMACCESS] %s removing access for %s on '%s'\n",
           requester, username, file_path);
    
    // 1. Check if file exists
    FileInfo* file = find_file(file_path);
    if (!file) {
        send_message(client_fd, "ERROR: File not found");
        return;
    }
    
    // 2. Check if requester is the owner
    if (!is_file_owner(file_path, requester)) {
        send_message(client_fd, "ERROR: Only file owner can modify access");
        return;
    }
    
    // 3. Cannot remove owner's access
    if (strcmp(username, requester) == 0) {
        send_message(client_fd, "ERROR: Cannot remove owner's access");
        return;
    }
    
    // 4. Remove access
    int result = remove_access(file_path, username);
    
    if (result == 0) {
        char response[256];
        snprintf(response, sizeof(response), 
                "SUCCESS: Access removed for %s from '%s'", username, file_path);
        send_message(client_fd, response);
        printf("[NS-Client][REMACCESS] ✓ Access removed\n");
    } else {
        send_message(client_fd, "ERROR: User had no access to remove");
    }
}

/**
 * Handle VIEW command
 */
void handle_view_command(int client_fd, const char* flags, const char* username) {
    printf("[NS-Client][VIEW] Request from '%s' with flags: '%s'\n", username, flags);
    
    // Parse flags - client sends "flag_all flag_list" as "0" or "1"
    bool flag_all = false;
    bool flag_list = false;
    
    int all_val = 0, list_val = 0;
    sscanf(flags, "%d %d", &all_val, &list_val);
    
    flag_all = (all_val == 1);
    flag_list = (list_val == 1);
    
    printf("[NS-Client][VIEW] Parsed: flag_all=%d, flag_list=%d\n", flag_all, flag_list);
    
    // Get accessible files
    FileInfo* files[MAX_FILES];
    int file_count = get_accessible_files(username, files, MAX_FILES, flag_all);
    
    if (file_count == 0) {
        send_message(client_fd, "No files found");
        return;
    }
    
    char response[MAX_BUFFER_SIZE];
    int offset = 0;
    int i;
    
    if (flag_list) {
        // Detailed table format
        offset += snprintf(response + offset, MAX_BUFFER_SIZE - offset,
                          "---------------------------------------------------------\n");
        offset += snprintf(response + offset, MAX_BUFFER_SIZE - offset,
                          "|  Filename  | Words | Chars | Last Access Time | Owner |\n");
        offset += snprintf(response + offset, MAX_BUFFER_SIZE - offset,
                          "|------------|-------|-------|------------------|-------|\n");
        
        for (i = 0; i < file_count && offset < MAX_BUFFER_SIZE - 200; i++) {
            FileInfo* file = files[i];
            
            // Format last access time
            char time_str[32];
            struct tm* tm_info = localtime(&file->last_accessed);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
            
            // Format word and char counts
            char words[10], chars[10];
            if (file->word_count >= 0) {
                snprintf(words, sizeof(words), "%d", file->word_count);
            } else {
                strcpy(words, "N/A");
            }
            
            if (file->char_count >= 0) {
                snprintf(chars, sizeof(chars), "%d", file->char_count);
            } else {
                strcpy(chars, "N/A");
            }
            
            // Add table row with proper spacing
            offset += snprintf(response + offset, MAX_BUFFER_SIZE - offset,
                              "| %-10s | %5s | %5s | %16s | %5s |\n",
                              file->file_path, words, chars, time_str, file->owner);
        }
        
        offset += snprintf(response + offset, MAX_BUFFER_SIZE - offset,
                          "---------------------------------------------------------");
        
        if (i < file_count) {
            int remaining = file_count - i;
            offset += snprintf(response + offset, MAX_BUFFER_SIZE - offset,
                              "\n... and %d more files (truncated)", remaining);
        }
        
    } else {
        // Simple list format
        for (i = 0; i < file_count && offset < MAX_BUFFER_SIZE - 100; i++) {
            offset += snprintf(response + offset, MAX_BUFFER_SIZE - offset,
                              "--> %s\n", files[i]->file_path);
        }
        
        // Remove trailing newline
        if (offset > 0 && response[offset - 1] == '\n') {
            response[offset - 1] = '\0';
        }
        
        if (i < file_count) {
            int remaining = file_count - i;
            offset += snprintf(response + offset, MAX_BUFFER_SIZE - offset,
                              "\n... and %d more files (truncated)", remaining);
        }
    }
    
    send_message(client_fd, response);
    printf("[NS-Client][VIEW] ✓ Sent %d files to client\n", file_count);
}

/**
 * Handle CREATEFOLDER command
 */
void handle_createfolder_command(int client_fd, const char* folderpath, const char* username) {
    printf("[NS-Client][CREATEFOLDER] User '%s' creating folder '%s'\n", username, folderpath);
    
    // Get all active storage servers
    StorageServerInfo* ss_list[MAX_STORAGE_SERVERS];
    int ss_count = get_all_active_storage_servers(ss_list, MAX_STORAGE_SERVERS);
    
    if (ss_count == 0) {
        send_message(client_fd, "ERROR: No storage servers available");
        return;
    }
    
    int success_count = 0;
    
    // Broadcast to all storage servers
    for (int i = 0; i < ss_count; i++) {
        StorageServerInfo* ss = ss_list[i];
        
        pthread_mutex_lock(&ss->socket_mutex);
        
        ProtocolMessage msg;
        memset(&msg, 0, sizeof(ProtocolMessage));
        msg.type = htonl(MSG_CREATE_FOLDER);
        msg.status = 0;
        snprintf(msg.data, sizeof(msg.data), "%s|%s", folderpath, username);
        
        // Send request
        if (!send_protocol_message(ss->ss_fd, &msg)) {
            pthread_mutex_unlock(&ss->socket_mutex);
            printf("[NS-Client][CREATEFOLDER] ✗ Failed to send to SS #%d\n", ss->ss_id);
            continue;
        }
        
        // **FIX: Use recv_protocol_message() instead of raw recv()**
        ProtocolMessage ss_response;
        if (!recv_protocol_message(ss->ss_fd, &ss_response)) {
            pthread_mutex_unlock(&ss->socket_mutex);
            printf("[NS-Client][CREATEFOLDER] ✗ Failed to receive response from SS #%d\n", ss->ss_id);
            continue;
        }
        
        pthread_mutex_unlock(&ss->socket_mutex);
        
        // **FIX: Check response status correctly**
        int response_status = ntohl(ss_response.status);
        
        if (response_status == 0) {
            success_count++;
            printf("[NS-Client][CREATEFOLDER] ✓ Created on SS #%d\n", ss->ss_id);
        } else {
            printf("[NS-Client][CREATEFOLDER] ✗ Failed on SS #%d (status: %d, msg: %s)\n", 
                   ss->ss_id, response_status, ss_response.message);
        }
    }
    
    if (success_count > 0) {
        char response[256];
        snprintf(response, sizeof(response), 
                "SUCCESS: Folder '%s' created on %d storage server(s)", 
                folderpath, success_count);
        send_message(client_fd, response);
        printf("[NS-Client][CREATEFOLDER] ✓ Folder created on %d/%d servers\n", success_count, ss_count);
    } else {
        send_message(client_fd, "ERROR: Failed to create folder on any storage server");
    }
}

/**
 * Handle MOVE command
 */
void handle_move_command(int client_fd, const char* filename, 
                         const char* folderpath, const char* username) {
    printf("[NS-Client][MOVE] User '%s' moving '%s' to '%s'\n", 
           username, filename, folderpath);
    
    // Find file in registry
    FileInfo* file = find_file(filename);
    if (!file) {
        send_message(client_fd, "ERROR: File not found");
        printf("[NS-Client][MOVE] ✗ File '%s' not found\n", filename);
        return;
    }
    
    if (!file->is_active) {
        send_message(client_fd, "ERROR: File is not currently available");
        printf("[NS-Client][MOVE] ✗ File '%s' is inactive\n", filename);
        return;
    }
    
    // Check ownership
    if (!is_file_owner(filename, username)) {
        send_message(client_fd, "ERROR: Only file owner can move files");
        printf("[NS-Client][MOVE] ✗ Access denied for user '%s'\n", username);
        return;
    }
    
    // Get SS info
    StorageServerInfo* ss = find_storage_server_by_address(file->ss_ip, file->ss_nm_port);
    if (!ss || !ss->is_active) {
        send_message(client_fd, "ERROR: Storage server not available");
        printf("[NS-Client][MOVE] ✗ SS not available for file '%s'\n", filename);
        return;
    }
    
    // Send MOVE request to SS
    pthread_mutex_lock(&ss->socket_mutex);
    
    ProtocolMessage msg;
    memset(&msg, 0, sizeof(ProtocolMessage));
    msg.type = htonl(MSG_MOVE_FILE);
    msg.status = 0;
    snprintf(msg.data, sizeof(msg.data), "%s|%s|%s", filename, folderpath, username);
    
    if (!send_protocol_message(ss->ss_fd, &msg)) {
        pthread_mutex_unlock(&ss->socket_mutex);
        send_message(client_fd, "ERROR: Failed to communicate with storage server");
        return;
    }
    
    // Wait for response
    ProtocolMessage response;
    memset(&response, 0, sizeof(ProtocolMessage));
    
    if (!recv_protocol_message(ss->ss_fd, &response)) {
        pthread_mutex_unlock(&ss->socket_mutex);
        mark_ss_inactive(ss->ip_address, ss->nm_port);
        const char* error = "ERROR: Failed to receive response from storage server";
        send_message(client_fd, error);
        printf("[NS-Client][MOVE] ✗ Failed to recv from SS\n");
        return;
    }
    
    pthread_mutex_unlock(&ss->socket_mutex);

    int response_status = ntohl(response.status);
    
    if (response_status == 0) {
        char reply[256];
        snprintf(reply, sizeof(reply), 
                "SUCCESS: File '%s' moved to folder '%s'", filename, folderpath);
        send_message(client_fd, reply);
        printf("[NS-Client][MOVE] ✓ File moved successfully\n");
    } else {
        // Send error from SS
        if (strlen(response.message) > 0) {
            send_message(client_fd, response.message);
        } else {
            const char* error = "ERROR: Failed to move file";
            send_message(client_fd, error);
        }
        printf("[NS-Client][MOVE] ✗ Move failed: %s\n", response.message);
    }
}

/**
 * Handle VIEWFOLDER command
 */
void handle_viewfolder_command(int client_fd, const char* folderpath, const char* username) {
    printf("[NS-Client][VIEWFOLDER] User '%s' viewing folder '%s'\n", username, folderpath);
    
    char full_folderpath[512];
    if (strncmp(folderpath, "root/", 5) == 0 || strcmp(folderpath, "root") == 0) {
        // Already has root prefix
        strncpy(full_folderpath, folderpath, sizeof(full_folderpath) - 1);
    } else {
        // Add root prefix
        snprintf(full_folderpath, sizeof(full_folderpath), "root/%s", folderpath);
    }
    full_folderpath[sizeof(full_folderpath) - 1] = '\0';
    
    printf("[NS-Client][VIEWFOLDER] Full path: '%s'\n", full_folderpath);
    
    // Get any active SS (they all have same folder structure)
    StorageServerInfo* ss = select_ss_for_file();
    if (!ss) {
        send_message(client_fd, "ERROR: No storage servers available");
        printf("[NS-Client][VIEWFOLDER] ✗ No storage servers available\n");
        return;
    }
    
    // Send VIEWFOLDER request to SS
    pthread_mutex_lock(&ss->socket_mutex);
    
    ProtocolMessage msg;
    memset(&msg, 0, sizeof(ProtocolMessage));
    msg.type = htonl(MSG_VIEW_FOLDER);
    msg.status = 0;
    snprintf(msg.data, sizeof(msg.data), "%s|%s", full_folderpath, username);
    
    // snprintf(msg.data, sizeof(msg.data), "%s|%s", folderpath, username);
    
    if (!send_protocol_message(ss->ss_fd, &msg)) {
        pthread_mutex_unlock(&ss->socket_mutex);
        const char* error = "ERROR: Failed to communicate with storage server";
        send_message(client_fd, error);
        printf("[NS-Client][VIEWFOLDER] ✗ Failed to send to SS\n");
        return;
    }
    
    // Wait for response
    ProtocolMessage response;
    memset(&response, 0, sizeof(ProtocolMessage));

    // ssize_t received = recv(ss->ss_fd, &response, sizeof(ProtocolMessage), 0);
    if (!recv_protocol_message(ss->ss_fd, &response)) {
        pthread_mutex_unlock(&ss->socket_mutex);
        const char* error = "ERROR: Failed to receive response from storage server";
        send_message(client_fd, error);
        printf("[NS-Client][VIEWFOLDER] ✗ Failed to recv from SS\n");
        return;
    }
    
    pthread_mutex_unlock(&ss->socket_mutex);
    
    // if (received == 0) {
    //     printf("[NS-Client][VIEWFOLDER] SS #%d disconnected\n", ss->ss_id);
    //     mark_ss_inactive(ss->ip_address, ss->nm_port);
    //     send_message(client_fd, "ERROR: Storage server disconnected");
    //     return;
    // }
    
    // if (received != sizeof(ProtocolMessage)) {
    //     const char* error = "ERROR: Invalid response from storage server";
    //     send_message(client_fd, error);
    //     printf("[NS-Client][VIEWFOLDER] ✗ Invalid response size: %zd (expected %zu)\n", 
    //            received, sizeof(ProtocolMessage));
    //     return;
    // }
    
    int response_status = ntohl(response.status);

    // DEBUG: Print what we received
    printf("[NS-Client][VIEWFOLDER] Received from SS:\n");
    printf("  - status (after ntohl): %d\n", response_status);
    printf("  - message length: %zu\n", strlen(response.message));
    printf("  - message (first 100 chars): '%.100s'\n", response.message);
    printf("  - message (hex first 32 bytes): ");
    for (int i = 0; i < 32 && i < (int)sizeof(response.message); i++) {
        printf("%02x ", (unsigned char)response.message[i]);
    }
    printf("\n");

    if (response_status == 0) {
        // Check if message is actually empty or garbage
        if (strlen(response.message) == 0) {
            const char* empty = "ERROR: Empty response from storage server";
            send_message(client_fd, empty);
            printf("[NS-Client][VIEWFOLDER] ✗ Empty message from SS\n");
        } else {
            // Send the actual message content
            send_message(client_fd, response.message);
            printf("[NS-Client][VIEWFOLDER] ✓ Sent folder contents (%zu bytes)\n", 
                   strlen(response.message));
        }
    } else {
        // Error - send error message
        if (strlen(response.message) > 0) {
            send_message(client_fd, response.message);
        } else {
            const char* error = "ERROR: Failed to view folder";
            send_message(client_fd, error);
        }
        printf("[NS-Client][VIEWFOLDER] ✗ Error status %d: %s\n", 
               response_status, response.message);
    }
}