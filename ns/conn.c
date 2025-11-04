#include "conn.h"

// Global flag for graceful shutdown
volatile sig_atomic_t running = 1;

// Define global configuration variables
int NS_CLIENT_PORT=0;
int NS_CLIENT_BACKLOG=0;
int NS_CLIENT_BUFFER_SIZE=0;



int setup_server(){

    // Load configuration from .env file
    NS_CLIENT_PORT = load_env_int("NS_CLIENT_PORT", 9090);
    NS_CLIENT_BACKLOG = load_env_int("NS_CLIENT_BACKLOG", 10);
    NS_CLIENT_BUFFER_SIZE = load_env_int("NS_CLIENT_BUFFER_SIZE", 4096);
    
    starting_msg(NS_CLIENT_PORT, NS_CLIENT_BACKLOG, NS_CLIENT_BUFFER_SIZE); // Display starting message
    setup_signal_handlers();  // for graceful shutdown

    // Initialize naming server
    printf("[NS] Initializing server on port %d...\n", NS_CLIENT_PORT);
    int server_fd = init_server(NS_CLIENT_PORT, NS_CLIENT_BACKLOG);

    if (server_fd < 0) {
        fprintf(stderr, "[NS ERROR] Failed to initialize server: %s\n", get_socket_error());
        return -1;
    }

    printf("[NS] Server initialized successfully!\n");
    printf("[NS] Waiting for client connections...\n");
    printf("═══════════════════════════════════════════\n\n");

    // Set server socket to non-blocking mode for graceful shutdown
    set_socket_nonblocking(server_fd);

    return server_fd;
}


void starting_msg(int ns_client_port, int ns_client_backlog, int ns_client_buffer_size) {
    printf("╔════════════════════════════════════════╗\n");
    printf("║    NAMING SERVER (NS) STARTED          ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    printf("[NS] Configuration loaded:\n");
    printf("     - NS_CLIENT_PORT: %d\n", ns_client_port);
    printf("     - NS_CLIENT_BACKLOG: %d\n", ns_client_backlog);
    printf("     - NS_CLIENT_BUFFER_SIZE: %d\n\n", ns_client_buffer_size);
}



void shutdown_server(int server_fd) {
    printf("\n[NS] Shutting down server...\n");
    close_server(server_fd);
    printf("[NS] Server closed successfully.\n");
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║    NAMING SERVER (NS) STOPPED            ║\n");
    printf("╚═════════════════════════════════════════╝\n");
}


void setup_signal_handlers() {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}


void signal_handler(int signum) {
    (void)signum;  // Suppress unused parameter warning
    printf("\n[NS] Received shutdown signal. Cleaning up...\n");
    running = 0;
}

// Simple function to load int from .env file
int load_env_int(const char *key, int default_value) {
    FILE *file = fopen("../.env", "r");
    if (!file) return default_value;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;
        
        // Find key
        char *equals = strchr(line, '=');
        if (!equals) continue;
        
        *equals = '\0';
        char *value = equals + 1;
        
        // Trim whitespace from key
        char *k = line;
        while (isspace(*k)) k++;
        char *k_end = equals - 1;
        while (k_end > k && isspace(*k_end)) k_end--;
        *(k_end + 1) = '\0';
        
        // Check if this is our key
        if (strcmp(k, key) == 0) {
            // Trim whitespace and comments from value
            while (isspace(*value)) value++;
            char *v_end = value;
            while (*v_end && *v_end != '#' && *v_end != '\n') v_end++;
            *v_end = '\0';
            v_end--;
            while (v_end > value && isspace(*v_end)) *v_end-- = '\0';
            
            fclose(file);
            return atoi(value);
        }
    }
    
    fclose(file);
    return default_value;
}

