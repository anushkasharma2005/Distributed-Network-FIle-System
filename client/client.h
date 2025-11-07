#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdbool.h>

#include "../include/constants.h"
# include "../api_c_ss/client_ss_connection.h"

// Configuration
#define MAX_COMMAND_LENGTH 1024
#define MAX_FILENAME_LENGTH 256
#define MAX_USERNAME_LENGTH 64
#define STREAM_DELAY_MS 100000 // 0.1 seconds in microseconds

// Error codes
#define SUCCESS 0
#define ERR_INVALID_COMMAND -1
#define ERR_CONNECTION -2
#define ERR_FILE_NOT_FOUND -3
#define ERR_ACCESS_DENIED -4
#define ERR_SERVER_ERROR -5
#define ERR_INVALID_INDEX -6
#define ERR_FILE_EXISTS -7
#define ERR_SENTENCE_LOCKED -8

// Message types
#define MSG_VIEW 1
#define MSG_READ 2
#define MSG_CREATE 3
#define MSG_WRITE 4
#define MSG_WRITE_END 5
#define MSG_UNDO 6
#define MSG_INFO 7
#define MSG_DELETE 8
#define MSG_STREAM 9
#define MSG_LIST_USERS 10
#define MSG_ADD_ACCESS 11
#define MSG_REM_ACCESS 12
#define MSG_EXEC 13

// Client structure
typedef struct
{
    int nm_socket; // Socket for Name Server
    int ss_socket; // Socket for Storage Server (direct connections)
    char username[MAX_USERNAME_LENGTH];
    char nm_ip[16];
    int nm_port;
    int client_port;
    bool connected;
} Client;

// Command parsing structures
typedef struct
{
    int type;
    char filename[MAX_FILENAME_LENGTH];
    char username[MAX_USERNAME_LENGTH];
    char content[MAX_BUFFER_SIZE];
    int sentence_num;
    int word_index;
    bool flag_all;
    bool flag_list;
    bool read_access;
    bool write_access;
} Command;

// Function declarations

// Core client functions
int client_init_struct(Client *client, const char *nm_ip, int nm_port, const char *username);
void client_cleanup(Client *client);
int client_connect_to_nm(Client *client);
int client_connect_to_ss(Client *client, const char *ss_ip, int ss_port);
void client_disconnect_from_ss(Client *client);

// Command parsing
int parse_command(const char *input, Command *cmd);
void print_help();

// Client operations (these communicate with Name Server or Storage Server)
int cmd_view(Client *client, bool flag_all, bool flag_list);
int cmd_read(Client *client, const char *filename);
int cmd_create(Client *client, const char *filename);
int cmd_write(Client *client, const char *filename, int sentence_num);
int cmd_undo(Client *client, const char *filename);
int cmd_info(Client *client, const char *filename);
int cmd_delete(Client *client, const char *filename);
int cmd_stream(Client *client, const char *filename);
int cmd_list_users(Client *client);
int cmd_add_access(Client *client, const char *filename, const char *username, bool write_access);
int cmd_rem_access(Client *client, const char *filename, const char *username);
int cmd_exec(Client *client, const char *filename);

// Helper functions
int send_to_nm(Client *client, const void *data, size_t len);
int recv_from_nm(Client *client, void *buffer, size_t len);
int send_to_ss(Client *client, const void *data, size_t len);
int recv_from_ss(Client *client, void *buffer, size_t len);
int send_request_to_ss(Client *client, ClientRequest *req);
int recv_response_from_ss(Client *client, ClientRequest *resp);
void print_error(int error_code);

#endif // CLIENT_H