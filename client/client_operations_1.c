// this file includes all the client operations which involves client-ns conn only

#include "client.h"

// VIEW command - Lists files
int cmd_view(Client *client, bool flag_all, bool flag_list) {
    if (!client || !client->connected) {
        return ERR_CONNECTION;
    }

    // Prepare request
    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "VIEW %d %d", flag_all ? 1 : 0, flag_list ? 1 : 0);

    // Send to Name Server
    if (send_to_nm(client, request, strlen(request)) != SUCCESS) {
        return ERR_CONNECTION;
    }

    // Receive response
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

    // Display the response
    printf("%s\n", response);
    return SUCCESS;
}

// CREATE command - Creates a new file
int cmd_create(Client *client, const char *filename) {
    if (!client || !client->connected || !filename) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "CREATE %s", filename);

    if (send_to_nm(client, request, strlen(request)) != SUCCESS) {
        return ERR_CONNECTION;
    }

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

    printf("%s\n", response);
    return SUCCESS;
}

// UNDO command - Reverts last change
int cmd_undo(Client *client, const char *filename) {
    if (!client || !client->connected || !filename) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "UNDO %s", filename);

    if (send_to_nm(client, request, strlen(request)) != SUCCESS) {
        return ERR_CONNECTION;
    }

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

    // printf("%s\n", response);
    // return SUCCESS;

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

    // **NEW: Send UNDO request using ClientRequest structure**
    ClientRequest req;
    memset(&req, 0, sizeof(ClientRequest));
    req.op_type = OP_UNDO;
    strncpy(req.filename, filename, MAX_FILENAME - 1);
    strncpy(req.username, client->username, 63);

    if (send_request_to_ss(client, &req) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    // Receive response
    ClientRequest resp;
    if (recv_response_from_ss(client, &resp) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    // Check for errors
    if (resp.status != 0) {
        fprintf(stderr, "%s\n", resp.error_msg);
        client_disconnect_from_ss(client);
        return ERR_SERVER_ERROR;
    }

    // Display success message
    printf("%s\n", resp.error_msg);

    client_disconnect_from_ss(client);
    return SUCCESS;
}

// INFO command - Displays file information
int cmd_info(Client *client, const char *filename) {
    if (!client || !client->connected || !filename) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "INFO %s", filename);

    if (send_to_nm(client, request, strlen(request)) != SUCCESS) {
        return ERR_CONNECTION;
    }

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

    printf("%s\n", response);
    return SUCCESS;
}

// DELETE command - Deletes a file
int cmd_delete(Client *client, const char *filename) {
    if (!client || !client->connected || !filename) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "DELETE %s", filename);

    if (send_to_nm(client, request, strlen(request)) != SUCCESS) {
        return ERR_CONNECTION;
    }

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

    printf("%s\n", response);
    return SUCCESS;
}

// EXEC command - Executes file content as shell commands
int cmd_exec(Client *client, const char *filename) {
    if (!client || !client->connected || !filename) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "EXEC %s", filename);

    if (send_to_nm(client, request, strlen(request)) != SUCCESS) {
        return ERR_CONNECTION;
    }

    // The Name Server will execute the commands and return output
    // We may receive multiple packets for large outputs
    char response[MAX_BUFFER_SIZE];
    
    while (1) {
        int bytes = recv_from_nm(client, response, sizeof(response) - 1);
        if (bytes <= 0) {
            return ERR_CONNECTION;
        }
        response[bytes] = '\0';

        // Check for end signal
        if (strcmp(response, "EXEC_END") == 0) {
            break;
        }

        // Check for errors
        if (strncmp(response, "ERROR", 5) == 0) {
            fprintf(stderr, "%s\n", response);
            return ERR_SERVER_ERROR;
        }

        // Display output
        printf("%s", response);
        fflush(stdout);
    }

    return SUCCESS;
}

// LIST command - Lists all users
int cmd_list_users(Client *client) {
    if (!client || !client->connected) {
        return ERR_CONNECTION;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "LIST_USERS");

    if (send_to_nm(client, request, strlen(request)) != SUCCESS) {
        return ERR_CONNECTION;
    }

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

    printf("%s\n", response);
    return SUCCESS;
}

// ADDACCESS command - Grants access to a user
int cmd_add_access(Client *client, const char *filename, const char *username, bool write_access) {
    if (!client || !client->connected || !filename || !username) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "ADDACCESS %s %s %c", 
             filename, username, write_access ? 'W' : 'R');

    if (send_to_nm(client, request, strlen(request)) != SUCCESS) {
        return ERR_CONNECTION;
    }

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

    printf("%s\n", response);
    return SUCCESS;
}

// REMACCESS command - Removes access from a user
int cmd_rem_access(Client *client, const char *filename, const char *username) {
    if (!client || !client->connected || !filename || !username) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "REMACCESS %s %s", filename, username);

    if (send_to_nm(client, request, strlen(request)) != SUCCESS) {
        return ERR_CONNECTION;
    }

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

    printf("%s\n", response);
    return SUCCESS;
}
