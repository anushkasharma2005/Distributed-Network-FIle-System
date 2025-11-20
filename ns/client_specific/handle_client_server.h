#ifndef HANDLE_CLIENT_SERVER_H
#define HANDLE_CLIENT_SERVER_H


#include "../types.h"



/**
 * Structure to pass data to client handler threads
 * Contains client socket file descriptor and connection information
 */
typedef struct {
    int client_fd;           // Client socket file descriptor
    Connection client_conn;  // Client connection details (IP, port)
    char username[MAX_USERNAME_LENGTH]; // Client username
} ClientThreadData_NS;

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
 * Client handler function that runs in a separate thread
 * Handles communication with a single client, receives messages and sends acknowledgments
 * Thread automatically detaches and cleans up resources on completion
 * @param arg Pointer to ClientThreadData_NS structure containing client information
 * @return NULL on thread completion
 */
void* handle_client(void* arg);


/**
 * Helper functions for client command processing
 */
void print_protocol_message_layout(void);



#endif // HANDLE_CLIENT_SERVER_H