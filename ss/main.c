#include "api_ns_ss/ns_ss_connection.h"
#include "api_c_ss/client_ss_connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

void signal_handler(int sig) {
    (void)sig;
    printf("\n[SS] Received signal %d, shutting down...\n", sig);
    keep_running = 0;
}

// Thread for handling NS communication
void *ns_communication_thread(void *arg) {
    int ns_sock = *((int *)arg);
    char base_path[256];
    
    // Get base path from config or use default
    strcpy(base_path, "./storage");
    
    printf("[SS] Starting NS communication handler\n");
    
    // Handle NS commands (this blocks)
    ss_handle_ns_commands(ns_sock, base_path);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    // Configuration
    char ns_ip[16] = "127.0.0.1";
    int ns_port = 9091;
    int nm_port = 9002;      // Port for NM connection
    int client_port = 9003;  // Port for client connections
    char base_path[256] = "./storage";
    char ss_ip[16] = "127.0.0.1";
    
    // Parse command line arguments
    if (argc >= 5) {
        strncpy(ns_ip, argv[1], 15);
        ns_port = atoi(argv[2]);
        nm_port = atoi(argv[3]);
        client_port = atoi(argv[4]);
    }
    if (argc >= 6) {
        strncpy(base_path, argv[5], 255);
    }
    
    printf("==============================================\n");
    printf("       Storage Server Initialization\n");
    printf("==============================================\n");
    printf("NS Address: %s:%d\n", ns_ip, ns_port);
    printf("NM Port: %d\n", nm_port);
    printf("Client Port: %d\n", client_port);
    printf("Base Path: %s\n", base_path);
    printf("==============================================\n\n");

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Step 1: Connect to Name Server
    printf("[SS] Step 1: Connecting to Name Server...\n");
    int ns_sock = ss_connect_to_ns(ns_ip, ns_port);
    if (ns_sock < 0) {
        fprintf(stderr, "[SS] Failed to connect to Name Server\n");
        return EXIT_FAILURE;
    }

    // Step 2: Scan files and prepare registration data
    printf("\n[SS] Step 2: Scanning files in %s...\n", base_path);
    SSRegistrationData reg_data;
    memset(&reg_data, 0, sizeof(SSRegistrationData));
    
    strncpy(reg_data.ss_ip, ss_ip, 15);
    reg_data.nm_port = nm_port;
    reg_data.client_port = client_port;
    
    if (ss_scan_files(base_path, &reg_data) < 0) {
        fprintf(stderr, "[SS] Warning: Failed to scan files\n");
    }

    // Step 3: Register with Name Server
    printf("\n[SS] Step 3: Registering with Name Server...\n");
    if (ss_register_with_ns(ns_sock, &reg_data) < 0) {
        fprintf(stderr, "[SS] Failed to register with Name Server\n");
        close(ns_sock);
        return EXIT_FAILURE;
    }

    // Step 4: Start NS communication thread
    printf("\n[SS] Step 4: Starting NS communication thread...\n");
    pthread_t ns_thread;
    if (pthread_create(&ns_thread, NULL, ns_communication_thread, &ns_sock) != 0) {
        fprintf(stderr, "[SS] Failed to create NS communication thread\n");
        close(ns_sock);
        return EXIT_FAILURE;
    }
    pthread_detach(ns_thread);

    // Step 5: Initialize client listener
    printf("\n[SS] Step 5: Initializing client listener...\n");
    int client_listen_fd = ss_init_client_listener(client_port);
    if (client_listen_fd < 0) {
        fprintf(stderr, "[SS] Failed to initialize client listener\n");
        close(ns_sock);
        return EXIT_FAILURE;
    }

    // Step 6: Initialize client manager
    printf("\n[SS] Step 6: Initializing client manager...\n");
    ClientManager client_manager;
    if (ss_init_client_manager(&client_manager, base_path) < 0) {
        fprintf(stderr, "[SS] Failed to initialize client manager\n");
        close(client_listen_fd);
        close(ns_sock);
        return EXIT_FAILURE;
    }

    printf("\n==============================================\n");
    printf("  Storage Server is ready and running!\n");
    printf("==============================================\n\n");

    // Step 7: Accept and handle client connections
    // This is the main loop
    ss_accept_clients(client_listen_fd, &client_manager);

    // Cleanup (reached only on shutdown)
    printf("\n[SS] Cleaning up...\n");
    close(client_listen_fd);
    close(ns_sock);
    ss_cleanup_client_manager(&client_manager);

    printf("[SS] Storage Server shutdown complete\n");
    return EXIT_SUCCESS;
}