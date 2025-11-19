#ifndef CLIENT_SS_CONNECTION_H
#define CLIENT_SS_CONNECTION_H

#include <pthread.h>
#include <netinet/in.h>
#include <signal.h>
#include "../include/constants.h"
#include "../ss/file_structure.h"

// Maximum limits
#define MAX_CLIENTS 100
#define MAX_SENTENCE_LEN 2048
#define MAX_FILENAME 256

extern volatile sig_atomic_t keep_running;

// Client-SS operation types
typedef enum
{
    OP_READ=1,
    OP_WRITE=2,
    OP_WRITE_BEGIN = 3,
    OP_WRITE_UPDATE = 4,
    OP_WRITE_END = 5,
    OP_STREAM = 6,
    OP_UNDO = 7,
    OP_ACK = 100,
    OP_ERROR = 101,
    OP_STOP = 102
} OperationType;

// Structure for read/write operations
typedef struct {
    OperationType op_type;
    char filename[MAX_FILENAME];
    char username[64];
    int sentence_num;
    int word_index;
    char content[MAX_BUFFER_SIZE];
    int status;
    char error_msg[512];
} ClientRequest;

// Forward declarations
typedef struct ClientManager ClientManager;

// Structure to track active write sessions
typedef struct WriteSession {
    int client_fd;
    char filename[MAX_FILENAME];
    char username[64];
    int sentence_num;
    int is_active;
    time_t start_time;
    FileStructure *temp_fs;  // Points to temp file structure
    struct WriteSession *next;
} WriteSession;

// Structure for client thread data
typedef struct
{
    int client_fd;
    int thread_id;
    char client_ip[16];
    int client_port;
    pthread_t thread;
    int active;
    WriteSession *write_session;
    ClientManager *manager;  // Now ClientManager is forward-declared
} ClientThreadData;

// Structure for managing multiple client connections
struct ClientManager  // Changed from typedef to struct definition
{
    ClientThreadData clients[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t lock;
    char base_path[MAX_FILENAME];
    FileManager *file_manager;
    WriteSession *active_sessions;
    pthread_mutex_t session_lock;
};

// Function prototypes for Client-SS API

/**
 * Initialize Storage Server for client connections
 * @param port Port to listen on for client connections
 * @return Socket file descriptor, or -1 on error
 */
int ss_init_client_listener(int port);

/**
 * Initialize client manager
 * @param manager Client manager structure
 * @param base_path Base path for file storage
 * @return 0 on success, -1 on error
 */
int ss_init_client_manager(ClientManager *manager, const char *base_path);

/**
 * Accept and handle client connections (main loop)
 * @param listen_fd Listening socket file descriptor
 * @param manager Client manager structure
 * @return Does not return unless error occurs
 */
int ss_accept_clients(int listen_fd, ClientManager *manager);

/**
 * Thread function to handle individual client
 * @param arg Pointer to ClientThreadData
 * @return NULL
 */
void *ss_client_handler(void *arg);

/**
 * Send response to client
 * @param client_fd Client socket file descriptor
 * @param request Response request structure
 * @return 0 on success, -1 on error
 */
int ss_send_to_client(int client_fd, ClientRequest *request);

/**
 * Receive request from client
 * @param client_fd Client socket file descriptor
 * @param request Buffer to store request
 * @return 0 on success, -1 on error
 */
int ss_receive_from_client(int client_fd, ClientRequest *request);

/**
 * Cleanup client thread
 * @param manager Client manager
 * @param thread_id Thread ID to cleanup
 */
void ss_cleanup_client(ClientManager *manager, int thread_id);

int ss_handle_read(int client_fd, ClientRequest *request, ClientManager *manager);
int ss_handle_stream(int client_fd, ClientRequest *request, ClientManager *manager);

int ss_handle_write_begin(int client_fd, ClientRequest *request, ClientManager *manager);
int ss_handle_write_update(int client_fd, ClientRequest *request, ClientManager *manager);
int ss_handle_write_end(int client_fd, ClientRequest *request, ClientManager *manager);
int ss_handle_undo(int client_fd, ClientRequest *request, ClientManager *manager);

// Write Session Management
WriteSession* ss_create_write_session(ClientManager *manager, int client_fd, 
                                      const char *filename, const char *username, 
                                      int sentence_num);
WriteSession* ss_find_write_session(ClientManager *manager, int client_fd);
void ss_destroy_write_session(ClientManager *manager, int client_fd);
void ss_cleanup_client_manager(ClientManager *manager);

#endif // CLIENT_SS_CONNECTION_H