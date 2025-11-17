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


#endif // CLIENT_COMMANDS_H