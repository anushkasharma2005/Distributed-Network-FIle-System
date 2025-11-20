#ifndef CLIENT_COMMANDS_H
#define CLIENT_COMMANDS_H

#include "types.h"
#include <stdbool.h>
#include "../../api_ns_ss/ns_ss_connection.h"

/**
 * Processes a command received from the client
 * @param client_fd Client socket file descriptor
 * @param buffer Buffer containing the received command
 * @param client_conn Connection structure with client details
 * @param username Username of the connected client
 */
void process_client_command(int client_fd, const char* buffer, Connection client_conn, const char* username);

// =======================CREATE COMMAND============================
/**
 * This function handles the CREATE command from a client
 * It checks if the file already exists, selects a storage server,
 * @param client_fd Client socket file descriptor
 * @param file_path Path of the file to create
 */
void handle_create_command(int client_fd, const char* file_path, const char* owner);

/**
 * Sends a CREATE request to the specified storage server for the given file path
 * @param ss Pointer to StorageServerInfo structure of the target storage server
 * @param file_path Path of the file to create
 * @return true on success, false on failure
 */
bool send_create_request_to_ss(StorageServerInfo* ss, const char* file_path);

/**
 * Waits for a response from the storage server after sending a CREATE request
 * @param ss Pointer to StorageServerInfo structure of the target storage server
 * @param error_msg Buffer to store error message if any
 * @param error_msg_size Size of the error message buffer
 * @return true if the response indicates success, false otherwise
 */
bool wait_for_ss_response(StorageServerInfo* ss, char* error_msg, size_t error_msg_size);

/**
 * This function handles file operation commands (READ, WRITE, DELETE, INFO)
 * It looks up the file in the registry and returns the storage server info to the client
 * @param client_fd Client socket file descriptor
 * @param file_path Path of the file to operate on
 * @param command The command type (READ, WRITE, DELETE, INFO)
 */
void handle_file_operation_command(int client_fd, const char* file_path, const char* command);


/**
 * This function handles the WRITE command from a client
 * It looks up the file in the registry and returns the storage server info to the client
 * @param client_fd Client socket file descriptor
 * @param file_path Path of the file to write to
 */
void handle_write_command(int client_fd, const char* file_path);

/**
 * Handle INFO command - returns detailed file information
 * @param client_fd Client socket file descriptor
 * @param file_path Path of the file to get info about
 */
void handle_info_command(int client_fd, const char* file_path);

/**
 * Handle LIST command - returns list of all users/owners
 * @param client_fd Client socket file descriptor
 */
void handle_list_command(int client_fd);

/**
 * Handle ADDACCESS command
 * @param client_fd Client socket
 * @param file_path File path
 * @param username User to grant access
 * @param access_type 'R' for read, 'W' for write
 * @param requester Username of person making request
 */
void handle_addaccess_command(int client_fd, const char* file_path, 
                              const char* username, char access_type, 
                              const char* requester);

/**
 * Handle REMACCESS command
 * @param client_fd Client socket
 * @param file_path File path
 * @param username User to revoke access from
 * @param requester Username of person making request
 */
void handle_remaccess_command(int client_fd, const char* file_path, 
                              const char* username, const char* requester);

                              
/**
 * Handle VIEW command
 * @param client_fd Client socket
 * @param flags String containing flags (e.g., "-a", "-l", "-al")
 * @param username Username of requester
 */
void handle_view_command(int client_fd, const char* flags, const char* username);

/**
 * Handle EXEC command
 * @param client_fd Client socket
 * @param file_path Path of the file to execute
 * @param username Username of requester
 */
void handle_exec_command(int client_fd, const char* file_path, const char* username);

/**
 * Helper: NS connects to SS as a client and reads file content
 * Returns malloc'd string with file content (caller must free), or NULL on error
 */
char* ns_read_file_from_ss(FileInfo* file_info);


/**
 * Parse commands from a single line by detecting known command names
 * Input: "ls sleep 5 echo after_party"
 * Output: ["ls", "sleep 5", "echo after_party"]
 *
 * @param line Input line containing multiple commands
 * @param commands Output array of command strings (caller must free)
 * @return Number of commands parsed
 */
int parse_commands_from_line(const char* line, char*** commands);


/**
 * Handle DELETE command - soft delete file
 * @param client_fd Client socket file descriptor
 * @param file_path Path of the file to delete
 * @param username Username of requester
 */
void handle_delete_command(int client_fd, const char* file_path, const char* username);

/**
 * Handle RESTORE command - restore deleted file
 * @param client_fd Client socket file descriptor
 * @param file_path Path of the file to restore
 * @param username Username of requester
 */
void handle_restore_command(int client_fd, const char* file_path, const char* username);


/**
 * Handle CREATEFOLDER command - create a new folder
 * @param client_fd Client socket file descriptor
 * @param folderpath Path of the folder to create
 * @param username Username of requester
 */
void handle_createfolder_command(int client_fd, const char* folderpath, const char* username);

/**
 * Handle MOVE command - move file to folder
 * @param client_fd Client socket file descriptor
 * @param filename Name of the file to move
 * @param folderpath Destination folder path
 * @param username Username of requester
 */
void handle_move_command(int client_fd, const char* filename, const char* folderpath, const char* username);

/**
 * Handle VIEWFOLDER command - view folder contents
 * @param client_fd Client socket file descriptor
 * @param folderpath Path of the folder to view
 * @param username Username of requester
 */
void handle_viewfolder_command(int client_fd, const char* folderpath, const char* username);

/**
 * Handle REQUEST command - request access to file
 */
void handle_request_command(int client_fd, const char* file_path, 
                           const char* requester, const char* access_type);

/**
 * Handle VIEWREQUESTS command - view pending requests
 */
void handle_viewrequests_command(int client_fd, const char* owner);

/**
 * Handle APPROVE command - approve access request
 */
void handle_approve_command(int client_fd, const char* file_path,
                           const char* username, const char* access_type,
                           const char* owner);

/**
 * Handle REJECT command - reject access request
 */
void handle_reject_command(int client_fd, const char* file_path,
                          const char* username, const char* access_type,
                          const char* owner);
                          
#endif // CLIENT_COMMANDS_H