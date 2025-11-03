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

    // Request streaming from SS
    snprintf(request, sizeof(request), "STREAM_FILE %s", filename);
    if (send_to_ss(client, request, strlen(request)) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    // Receive and display words with delay
    printf("\n--- Streaming: %s ---\n", filename);
    
    while (1) {
        char word[256];
        bytes = recv_from_ss(client, word, sizeof(word) - 1);
        
        if (bytes <= 0) {
            fprintf(stderr, "\nError: Storage Server disconnected during streaming\n");
            client_disconnect_from_ss(client);
            return ERR_CONNECTION;
        }
        
        word[bytes] = '\0';
        
        // Check for STOP signal
        if (strcmp(word, "STOP") == 0) {
            break;
        }
        
        // Check for errors
        if (strncmp(word, "ERROR", 5) == 0) {
            fprintf(stderr, "\n%s\n", word);
            client_disconnect_from_ss(client);
            return ERR_SERVER_ERROR;
        }
        
        // Display word
        printf("%s ", word);
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

    // Connect to Storage Server
    if (client_connect_to_ss(client, ss_ip, ss_port) != SUCCESS) {
        return ERR_CONNECTION;
    }

    // Request file from SS
    snprintf(request, sizeof(request), "READ_FILE %s", filename);
    if (send_to_ss(client, request, strlen(request)) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    // Receive file content
    char content[MAX_BUFFER_SIZE];
    bytes = recv_from_ss(client, content, sizeof(content) - 1);
    if (bytes <= 0) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }
    content[bytes] = '\0';

    // Check for errors
    if (strncmp(content, "ERROR", 5) == 0) {
        fprintf(stderr, "%s\n", content);
        client_disconnect_from_ss(client);
        return ERR_SERVER_ERROR;
    }

    // Display content
    printf("%s\n", content);

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

    // Enter interactive write mode
    printf("Write mode activated. Format: <word_index> <content>\n");
    printf("Type 'ETIRW' to finish and save.\n");

    char line[MAX_BUFFER_SIZE];
    bool first_write = true;

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
            // Send end signal to SS
            snprintf(request, sizeof(request), "WRITE_END %s %d", filename, sentence_num);
            send_to_ss(client, request, strlen(request));

            // Receive acknowledgment
            bytes = recv_from_ss(client, response, sizeof(response) - 1);
            if (bytes > 0) {
                response[bytes] = '\0';
                printf("%s\n", response);
            }
            break;
        }

        // Parse word_index and content
        int word_idx;
        char content[MAX_BUFFER_SIZE];
        if (sscanf(line, "%d %[^\n]", &word_idx, content) != 2) {
            fprintf(stderr, "Invalid format. Use: <word_index> <content>\n");
            continue;
        }

        // Send write command to SS
        if (first_write) {
            snprintf(request, sizeof(request), "WRITE_START %s %d %d %s", 
                     filename, sentence_num, word_idx, content);
            first_write = false;
        } else {
            snprintf(request, sizeof(request), "WRITE_CONTINUE %d %s", word_idx, content);
        }

        if (send_to_ss(client, request, strlen(request)) != SUCCESS) {
            client_disconnect_from_ss(client);
            return ERR_CONNECTION;
        }

        // Receive acknowledgment
        bytes = recv_from_ss(client, response, sizeof(response) - 1);
        if (bytes <= 0) {
            client_disconnect_from_ss(client);
            return ERR_CONNECTION;
        }
        response[bytes] = '\0';

        if (strncmp(response, "ERROR", 5) == 0) {
            fprintf(stderr, "%s\n", response);
            client_disconnect_from_ss(client);
            return ERR_SERVER_ERROR;
        }
    }

    client_disconnect_from_ss(client);
    return SUCCESS;
}