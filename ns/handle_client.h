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



#endif // HANDLE_CLIENT_H