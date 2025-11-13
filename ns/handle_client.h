#ifndef HANDLE_CLIENT_H
#define HANDLE_CLIENT_H

#include "conn.h"

/**
 * Structure to pass data to client handler threads
 * Contains client socket file descriptor and connection information
 */
typedef struct {
    int client_fd;           // Client socket file descriptor
    Connection client_conn;  // Client connection details (IP, port)
} ClientThreadData;

/**
 * Client handler function that runs in a separate thread
 * Handles communication with a single client, receives messages and sends acknowledgments
 * Thread automatically detaches and cleans up resources on completion
 * @param arg Pointer to ClientThreadData structure containing client information
 * @return NULL on thread completion
 */
void* handle_client(void* arg);


/**
 * Thread function to accept incoming client connections
 * Creates a new thread for each client using handle_client()
 * @param arg Pointer to client server file descriptor (int*)
 * @return NULL
 */
void* accept_clients(void* arg);

/**
 * Setup and initialize the client server for handling client connections
 * @return Server socket file descriptor on success, -1 on failure
 */
int setup_client_server();

/**
 * Helper functions for client command processing
 */
void print_protocol_message_layout(void);

/**
 * This function handles the CREATE command from a client
 * It checks if the file already exists, selects a storage server,
 * @param client_fd Client socket file descriptor
 * @param file_path Path of the file to create
 */
void handle_create_command(int client_fd, const char* file_path);

/**
 * This function handles file operation commands (READ, WRITE, DELETE, INFO)
 * It looks up the file in the registry and returns the storage server info to the client
 * @param client_fd Client socket file descriptor
 * @param file_path Path of the file to operate on
 * @param command The command type (READ, WRITE, DELETE, INFO)
 */
void handle_file_operation_command(int client_fd, const char* file_path, const char* command);

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
 * Processes a command received from the client
 * @param client_fd Client socket file descriptor
 * @param buffer Buffer containing the received command
 * @param client_conn Connection structure with client details
 */
void process_client_command(int client_fd, const char* buffer, Connection client_conn);


#endif // HANDLE_CLIENT_H