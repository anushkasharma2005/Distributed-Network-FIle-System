#include "conn.h"
#include "handle_client_server.h"
#include "handle_ss.h"
#include "ss_registry.h"
#include "file_registry.h" 
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


    // Cleanup
    printf("\n[NS] Initiating shutdown sequence...\n");
    // Cleanup registry
    cleanup_ss_registry();
    cleanup_file_registry();

    // Shutdown servers by closing their sockets. 
    shutdown_main_server(fds);

    return EXIT_SUCCESS;
}