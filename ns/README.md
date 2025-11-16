# For the sake my sanity, i neeed to keep track of which file has what. 


:::warning
 This file is just for NS functions and its handling of the client and ss requests
:::


## Files 

1. [main.c](#main)  : Main function for NS, initializes servers, handles shutdown sequence. 

2. [conn.c/.h](#conn) : Connection management functions for NS. 

3. [handle_client_server.c/.h](#handle_client_server) : Functions to handle client connections and commands. 

4. [handle_ss.c/.h](#handle_ss) : Functions to handle storage server connections and commands. 

5. [ss_registry.c/.h](#ss_registry) : Registry management for storage servers. 

6. [file_registry.c/.h](#file_registry) : Registry management for files. 

7. [ss_selector.c/.h](#ss_selector) : Logic to select appropriate storage servers for file operations.


## Main

:::note
 This is only a c file and not a header file. therefore there are no functions defined here. This just calls functions from other files.
:::


### Flow of main.c
1. sets up the Naming server by initializing data structures and starting server sockets. 
2. Then creates threads to accept client and storage server connections. These now function independently.
3. Waits for shutdown signal. If signal received, running flag will be set to 0 and acceptance threads will exit their loops  and we'll proceed to cleanup
4. Cleanup registries and shutdown servers by closing their sockets.
5. Exit program.

### Functions called from other files
- setup_servers() : from conn.c
- create_acceptance_threads() : from conn.c
- shutdown_main_server() : from conn.c
- cleanup_ss_registry() : from ss_registry.c
- cleanup_file_registry() : from file_registry.c

## Conn

Has both .c and .h files 

::: note
 This file manages the connections for the Naming Server (NS). It includes functions to set up server sockets for clients and storage servers, create threads to accept incoming connections, handle graceful shutdowns, and manage server file descriptors.

 THATS IT. IT DOESNT WORK ON HANDLING ANY OF THE INTRICASIES OF CLIENT OR SS REQUESTS. THAT IS DONE IN OTHER FILES.
:::

``` C
/**
 * Structure to hold server file descriptors
 */
typedef struct {
    int client_server_fd;
    int ss_server_fd;
} ServerFDs;

// Global flag for graceful shutdown of the main server and its threads
extern volatile sig_atomic_t running;

```

### Functions in conn.c/.h
- **ServerFDs setup_server();** : Sets up server sockets for clients and storage servers.
- **int create_acceptance_threads(ServerFDs \*fds);** : Creates threads to accept connections from clients and storage servers.
- **void setup_signal_handlers();** : Sets up signal handlers for graceful shutdown.
- **void shutdown_main_server(ServerFDs fds);** : Shuts down the main server and its threads. (calls the shutdown_server() function for each server fd)
- **void shutdown_server(int server_fd);** : Shuts down a specific server(for given fd).


### functions called from other files

- setup_server():
    - setup_client_server(): from handle_client_server.c
    - setup_ss_server(): from handle_ss.c
    - init_ss_registry(): from ss_registry.c
    - init_file_registry(): from file_registry.c
    
- create_acceptance_threads():
    - accept_storage_servers(): from handle_ss.c
    - accept_clients(): from handle_client_server.c

- shutdown_main_server(): *none*
- shutdown_server():  *none*
- setup_signal_handlers(): *none*

## Handle_client_server


:::note
This file have functions that just handle client connections related things , like setting up client_socket in the naming server (I'm calling this the client server of naming server, confusing? ik . LOLL) accepting connections, and then recieving requestes from clients and passing them on to the process_client_commands() function from client_commands.c.
:::

:::danger[PLEASE NOTE]

THIS FILE DOES NOT PARSE THE RECIEVED REQUESTS OR HANDLE THEM. THAT IS DONE IN client_commands.c
:::


### structures/variables in handle_client_server.c/.h

``` C
static int first_time = 1;

typedef struct {
    int client_fd;           // Client socket file descriptor
    Connection client_conn;  // Client connection details (IP, port)
} ClientThreadData;

```

### Functions in handle_client_server.c/.h

- **void \*accept_clients(void \*arg);** : Thread function to accept client connections.
- **int setup_client_server();** : Sets up the client server socket.
- **void \*handle_client(void \*arg);** : Thread function to handle client requests.
- **void print_protocol_message_layout();** : Prints the layout of protocol messages for debugging.

### functions called from other files
- accept_clients():
    - accept_connection(): from api_ns_c/naming_server.c
    - get_socket_error(): from api_ns_c/networking.c
- setup_client_server():
    - init_server(): from api_ns_c/naming_server.c
    - get_socket_error(): from api_ns_c/networking.c
    - set_socket_nonblocking(): from api_ns_c/networking.c 
- handle_client():
    - recv_message(): from api_ns_c/networking.c
    - process_client_commands(): from client_commands.c


## Client_commands


### Functions in client_commands.c/.h
- **int process_client_commands(int client_fd, const char\* buffer, Connection client_conn);** : Checks what the commands is and calls the appropriate handler function.
- **int handle_create_command(int client_fd, const char \* file_path);** : Handles the CREATE FILE command from a client.
- **int handle_file_operation_command(int client_fd, Connection client_conn, char \*filename, size_t filename_len);** : Handles the DELETE FILE command from a client.
- **int send_create_request_to_ss(int client_fd, uint8_t status_code);** : Sends an acknowledgment for file operations to the client.
- **bool wait_for_ss_response(StorageServerInfo\* ss, char\* error_msg, size_t error_msg_size);** : Waits for a response from a storage server.



### Functions called from other files
- process_client_commands():
    - send_message(): from api_ns_c/networking.c

- handle_create_command():
    - find_file(): from file_registry.c
    - select_storage_servers(): from ss_selector.c
    - send_create_request_to_ss(): from ss_registry.c
    - wait_for_ss_responses(): from ss_registry.c
    - send_message(): from api_ns_c/networking.c
    - register_file(): from file_registry.c


## Handle_ss

### Structures/variables in client_commands.c/.h
### Functions in client_commands.c/.h
### Functions called from other files


## Ss_registry

### Structures/variables in client_commands.c/.h
### Functions in client_commands.c/.h
### Functions called from other files




## File_registry

### Structures/variables in client_commands.c/.h
### Functions in client_commands.c/.h
### Functions called from other files


## Ss_selector  

### Structures/variables in client_commands.c/.h
### Functions in client_commands.c/.h
### Functions called from other files




