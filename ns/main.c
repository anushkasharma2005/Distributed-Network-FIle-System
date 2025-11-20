#include "conn.h"
#include "client_specific/handle_client_server.h"
#include "ss_specific/handle_ss.h"
#include "registry/ss_registry.h"
#include "registry/file_registry.h" 
#include "registry/user_registry.h"
#include "registry/cache.h" 
#include "registry/access_request_registry.h"
#include "../include/constants.h"
#include <stdio.h>

int main() {

    // First setup the servers. this function returns the struct with both fds (for clients and ss)
    ServerFDs fds = setup_server();

    // Then create the acceptance threads for both clients and ss. These threrads basically run infinite loops accepting connections from clients and ss
    if(create_acceptance_threads(&fds) == EXIT_FAILURE) {
        fprintf(stderr, "[NS] Failed to create acceptance threads\n");
        shutdown_main_server(fds);
        return EXIT_FAILURE;
    }


    // Wait for shutdown signal. If signal received, running flag will be set to 0 and acceptance threads will exit their loops  and we'll proceed to cleanup


    // Print cache statistics BEFORE cleanup
    print_cache_stats();

    // Cleanup
    printf("\n[NS] Initiating shutdown sequence...\n");
    // Cleanup registry
    cleanup_ss_registry();
    cleanup_file_registry();
    cleanup_user_registry();
    cleanup_cache();
    cleanup_access_request_registry();
    
    // Shutdown servers by closing their sockets. 
    shutdown_main_server(fds);

    return EXIT_SUCCESS;
}