// this file includes all the client operations which involves client-ss conn only (read, write, stream)

#include "client.h"

// STREAM command - Streams file content word-by-word
int cmd_stream(Client *client, const char *filename) {
    if (!client || !client->connected || !filename) {
        return ERR_INVALID_COMMAND;
    }

    // Request file location from Name Server
    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "STREAM %s", filename);

    if (send_to_nm(client, request, strlen(request)) != SUCCESS) {
        return ERR_CONNECTION;
    }

    // Receive SS info
    char response[MAX_BUFFER_SIZE];
    int bytes = recv_from_nm(client, response, sizeof(response) - 1);
    if (bytes <= 0) {
        return ERR_CONNECTION;
    }
    response[bytes] = '\0';

    if (strncmp(response, "ERROR", 5) == 0) {
        fprintf(stderr, "%s\n", response);
        return ERR_SERVER_ERROR;
    }

    // Parse SS IP and port
    char ss_ip[16];
    int ss_port;
    if (sscanf(response, "SS_INFO %s %d", ss_ip, &ss_port) != 2) {
        fprintf(stderr, "Invalid response from Name Server\n");
        return ERR_SERVER_ERROR;
    }

    // Connect to Storage Server
    if (client_connect_to_ss(client, ss_ip, ss_port) != SUCCESS) {
        return ERR_CONNECTION;
    }

    // Prepare and send stream request using ClientRequest structure
    ClientRequest req;
    memset(&req, 0, sizeof(ClientRequest));
    req.op_type = OP_STREAM;
    strncpy(req.filename, filename, MAX_FILENAME - 1);
    strncpy(req.username, client->username, 63);

    if (send_request_to_ss(client, &req) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    // Receive and display words with delay
    printf("\n--- Streaming: %s ---\n", filename);
    
    while (1) {
        ClientRequest resp;
        if (recv_response_from_ss(client, &resp) != SUCCESS) {
            fprintf(stderr, "\nError: Storage Server disconnected during streaming\n");
            client_disconnect_from_ss(client);
            return ERR_CONNECTION;
        }
        
        // Check for STOP signal
        if (resp.op_type == OP_STOP) {
            break;
        }
        
        // Check for errors
        if (resp.op_type == OP_ERROR || resp.status != 0) {
            fprintf(stderr, "\n%s\n", resp.error_msg);
            client_disconnect_from_ss(client);
            return ERR_SERVER_ERROR;
        }
        
        // Display word
        printf("%s ", resp.content);
        fflush(stdout);
        
        // Delay for 0.1 seconds
        usleep(STREAM_DELAY_MS);
    }
    
    printf("\n--- End of stream ---\n\n");
    
    client_disconnect_from_ss(client);
    return SUCCESS;
}

// READ command - Reads and displays file content
int cmd_read(Client *client, const char *filename) {
    if (!client || !client->connected || !filename) {
        return ERR_INVALID_COMMAND;
    }

    // Request file location from Name Server
    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "READ %s", filename);

    if (send_to_nm(client, request, strlen(request)) != SUCCESS) {
        return ERR_CONNECTION;
    }

    // Receive SS info (IP and port)
    char response[MAX_BUFFER_SIZE];
    int bytes = recv_from_nm(client, response, sizeof(response) - 1);
    // printf(" ============= received from nm: %d\n", bytes);
    if (bytes <= 0) {
        return ERR_CONNECTION;
    }
    response[bytes] = '\0';



    // Check for errors
    if (strncmp(response, "ERROR", 5) == 0) {
        fprintf(stderr, "%s\n", response);
        return ERR_SERVER_ERROR;
    }

    // Parse SS IP and port
    char ss_ip[16];
    int ss_port;
    if (sscanf(response, "SS_INFO %s %d", ss_ip, &ss_port) != 2) {
        fprintf(stderr, "Invalid response from Name Server\n");
        return ERR_SERVER_ERROR;
    }
    // printf("=============== resp from nm %s\n", response);

    // Connect to Storage Server
    if (client_connect_to_ss(client, ss_ip, ss_port) != SUCCESS) {
        return ERR_CONNECTION;
    }

    // Prepare and send read request using ClientRequest structure
    ClientRequest req;
    memset(&req, 0, sizeof(ClientRequest));
    req.op_type = OP_READ;
    strncpy(req.filename, filename, MAX_FILENAME - 1);
    strncpy(req.username, client->username, 63);

    if (send_request_to_ss(client, &req) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    // Receive file content
    ClientRequest resp;
    if (recv_response_from_ss(client, &resp) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    // Check for errors
    if (resp.status != 0) {
        fprintf(stderr, "Error: %s\n", resp.error_msg);
        client_disconnect_from_ss(client);
        return ERR_SERVER_ERROR;
    }

    // Display content
    printf("%s\n", resp.content);

    client_disconnect_from_ss(client);
    return SUCCESS;
}

// WRITE command - Interactive write mode
int cmd_write(Client *client, const char *filename, int sentence_num) {
    if (!client || !client->connected || !filename) {
        return ERR_INVALID_COMMAND;
    }

    // Request write lock from Name Server
    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "WRITE_LOCK %s %d", filename, sentence_num);

    if (send_to_nm(client, request, strlen(request)) != SUCCESS) {
        return ERR_CONNECTION;
    }

    // Receive SS info
    char response[MAX_BUFFER_SIZE];
    int bytes = recv_from_nm(client, response, sizeof(response) - 1);
    if (bytes <= 0) {
        return ERR_CONNECTION;
    }
    response[bytes] = '\0';

    if (strncmp(response, "ERROR", 5) == 0) {
        fprintf(stderr, "%s\n", response);
        return ERR_SERVER_ERROR;
    }

    // Parse SS info
    char ss_ip[16];
    int ss_port;
    if (sscanf(response, "SS_INFO %s %d", ss_ip, &ss_port) != 2) {
        fprintf(stderr, "Invalid response from Name Server\n");
        return ERR_SERVER_ERROR;
    }

    // Connect to Storage Server
    if (client_connect_to_ss(client, ss_ip, ss_port) != SUCCESS) {
        return ERR_CONNECTION;
    }

    // Send WRITE_BEGIN request
    ClientRequest req;
    memset(&req, 0, sizeof(ClientRequest));
    req.op_type = OP_WRITE_BEGIN;
    strncpy(req.filename, filename, MAX_FILENAME - 1);
    strncpy(req.username, client->username, 63);
    req.sentence_num = sentence_num;

    if (send_request_to_ss(client, &req) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    // Receive acknowledgment
    ClientRequest resp;
    if (recv_response_from_ss(client, &resp) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    if (resp.status != 0) {
        if (resp.status == -4) {
            // Word index out of range - special handling
            fprintf(stderr, "%s\n", resp.error_msg);
            fprintf(stderr, "Hint: Word indices start at 0. Use valid range shown above.\n");
        } else if (resp.status == -5) {
            // Sentence index out of range
            fprintf(stderr, "%s\n", resp.error_msg);
            fprintf(stderr, "Hint: Use READ to view the file and see the current sentence count.\n");
        } else {
            fprintf(stderr, "%s\n", resp.error_msg);
        }
        client_disconnect_from_ss(client);
        return ERR_SERVER_ERROR;
    }
    
    printf("Write session started: %s\n", resp.error_msg);

    // Enter interactive write mode
    printf("Write mode activated. Format: <word_index> <content>\n");
    printf("Type 'ETIRW' to finish and save.\n");

    char line[MAX_BUFFER_SIZE];

    while (1) {
        printf("Client: ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        // Remove newline
        line[strcspn(line, "\n")] = 0;

        // Check for ETIRW (end write)
        if (strcmp(line, "ETIRW") == 0) {
            // Send WRITE_END request
            memset(&req, 0, sizeof(ClientRequest));
            req.op_type = OP_WRITE_END;
            strncpy(req.filename, filename, MAX_FILENAME - 1);
            strncpy(req.username, client->username, 63);
            req.sentence_num = sentence_num;

            if (send_request_to_ss(client, &req) != SUCCESS) {
                client_disconnect_from_ss(client);
                return ERR_CONNECTION;
            }

            // Receive acknowledgment
            if (recv_response_from_ss(client, &resp) != SUCCESS) {
                client_disconnect_from_ss(client);
                return ERR_CONNECTION;
            }
            printf("%s\n", resp.error_msg);
            break;
        }

        // Parse word_index and content - PRESERVE SPACES
        // Format: <word_index><spaces><content>
        // We need to manually parse to preserve the exact spacing
        
        int word_idx;
        char *ptr = line;
        
        // Skip leading whitespace before word_index (if any)
        while (*ptr == ' ' || *ptr == '\t') {
            ptr++;
        }
        
        // Parse word_index
        char *endptr;
        word_idx = strtol(ptr, &endptr, 10);
        
        // Check if parsing was successful
        if (endptr == ptr) {
            fprintf(stderr, "Invalid format. Use: <word_index> <content>\n");
            continue;
        }
        
        // Everything after the word_index number is the content (including leading spaces)
        const char *content = endptr;
        
        // Validate that we have content (even if it's just spaces)
        if (*content == '\0') {
            fprintf(stderr, "Invalid format. Use: <word_index> <content>\n");
            continue;
        }

        // Send WRITE_UPDATE request
        memset(&req, 0, sizeof(ClientRequest));
        req.op_type = OP_WRITE_UPDATE;
        strncpy(req.filename, filename, MAX_FILENAME - 1);
        strncpy(req.username, client->username, 63);
        req.sentence_num = sentence_num;
        req.word_index = word_idx;
        strncpy(req.content, content, MAX_BUFFER_SIZE - 1);

        if (send_request_to_ss(client, &req) != SUCCESS) {
            client_disconnect_from_ss(client);
            return ERR_CONNECTION;
        }

        // Receive acknowledgment
        if (recv_response_from_ss(client, &resp) != SUCCESS) {
            client_disconnect_from_ss(client);
            return ERR_CONNECTION;
        }

        if (resp.status != 0) {
            if (resp.status == -5) {
                // Sentence index out of range - special handling
                fprintf(stderr, "%s\n", resp.error_msg);
                fprintf(stderr, "Hint: Use READ to view the file and see the current sentence count.\n");
            } else {
                fprintf(stderr, "Error: %s\n", resp.error_msg);
            }
            client_disconnect_from_ss(client);
            return ERR_SERVER_ERROR;
        }
    }

    client_disconnect_from_ss(client);
    return SUCCESS;
}