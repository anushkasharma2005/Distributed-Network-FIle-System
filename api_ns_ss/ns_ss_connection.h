#ifndef NS_SS_CONNECTION_H
#define NS_SS_CONNECTION_H

#include <netinet/in.h>
#include "../include/constants.h"
#include "../api_c_ns/networking.h"
#include "ns_ss_connection.h"
#include "../api_c_ss/client_ss_connection.h"  // ADD THIS - for ClientManager definition
#include "../ss/file_structure.h"              // ADD THIS - for folder functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <arpa/inet.h>


// Message types for NS-SS communication
typedef enum
{
    MSG_SS_REGISTER,
    MSG_SS_REGISTER_ACK,
    MSG_CREATE_FILE,
    MSG_DELETE_FILE,
    MSG_FILE_OP_ACK,
    MSG_HEARTBEAT,
    MSG_ERROR,
    MSG_CREATE_FOLDER = 17,
    MSG_MOVE_FILE = 18,
    MSG_VIEW_FOLDER = 19,
    MSG_READ_FILE,
    MSG_COPY_FILE,
    MSG_INFO_FILE,
} MessageType;

// Structure for file information
typedef struct
{
    char path[MAX_PATH_LEN];
} SSFileInfo;

// Structure for SS registration data
typedef struct
{
    char ss_ip[16];
    int nm_port;     // Port for NM connection
    int client_port; // Port for client connections
    int file_count;
    SSFileInfo files[MAX_FILES];
} SSRegistrationData;

// Structure for message protocol
typedef struct
{
    MessageType type;
    int status; // 0 = success, non-zero = error code
    char message[MAX_BUFFER_SIZE];
    int data_len;
    char data[MAX_BUFFER_SIZE];
} ProtocolMessage;


typedef struct ClientManager ClientManager;

// Function prototypes for NS-SS API

/**
 * Initialize connection from Storage Server to Name Server
 * @param ns_ip IP address of the Name Server
 * @param ns_port Port of the Name Server
 * @return Socket file descriptor, or -1 on error
 */
int ss_connect_to_ns(const char *ns_ip, int ns_port);

/**
 * Register Storage Server with Name Server
 * @param sock_fd Socket connection to NS
 * @param reg_data Registration data containing SS info and file list
 * @return 0 on success, -1 on error
 */
int ss_register_with_ns(int sock_fd, SSRegistrationData *reg_data);

/**
 * Send a protocol message to Name Server
 * @param sock_fd Socket file descriptor
 * @param msg Message to send
 * @return 0 on success, -1 on error
 */
int ss_send_message(int sock_fd, ProtocolMessage *msg);

/**
 * Receive a protocol message from Name Server
 * @param sock_fd Socket file descriptor
 * @param msg Buffer to store received message
 * @return 0 on success, -1 on error
 */
int ss_receive_message(int sock_fd, ProtocolMessage *msg);

/**
 * Handle incoming commands from Name Server
 * @param sock_fd Socket file descriptor
 * @param base_path Base storage path for files
 * @param client_manager Pointer to client manager for file operations
 * @return 0 on success, -1 on error
 */
int ss_handle_ns_commands(int sock_fd, const char *base_path, ClientManager *client_manager);

/**
 * Send heartbeat to Name Server
 * @param sock_fd Socket file descriptor
 * @return 0 on success, -1 on error
 */
int ss_send_heartbeat(int sock_fd);

/**
 * Scan directory and populate file list
 * @param base_path Base directory path
 * @param reg_data Registration data to populate
 * @return Number of files found, or -1 on error
 */
int ss_scan_files(const char *base_path, SSRegistrationData *reg_data);

/**
 * Send binary protocol message
 */
bool send_protocol_message(int fd, const ProtocolMessage* msg);

/**
 * Receive binary protocol message
 */
bool recv_protocol_message(int fd, ProtocolMessage* msg);

// extern volatile sig_atomic_t ns_connection_lost;

#endif // NS_SS_CONNECTION_H