#include "conn.h"
#include "handle_client.h"
#include "handle_ss.h"
#include "ss_registry.h"
#include "file_registry.h" 
#include "../include/constants.h"


int main() {

    ServerFDs fds = setup_server();

    if(create_acceptance_threads(&fds) == EXIT_FAILURE) {
        fprintf(stderr, "[NS] Failed to create acceptance threads\n");
        shutdown_main_server(fds);
        return EXIT_FAILURE;
    }

    // Cleanup
    printf("\n[NS] Initiating shutdown sequence...\n");
    
    // Cleanup registry
    cleanup_ss_registry();
    cleanup_file_registry();

    shutdown_main_server(fds);

    return EXIT_SUCCESS;
}