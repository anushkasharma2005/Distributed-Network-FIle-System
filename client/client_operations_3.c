#include "client.h"

// CHECKPOINT command - Create a checkpoint
int cmd_checkpoint(Client *client, const char *filename, const char *tag) {
    if (!client || !client->connected || !filename || !tag) {
        return ERR_INVALID_COMMAND;
    }

    // Request SS info from NS
    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "WRITE_LOCK %s", filename);

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

    // Send CHECKPOINT request
    ClientRequest req;
    memset(&req, 0, sizeof(ClientRequest));
    req.op_type = OP_CHECKPOINT;
    strncpy(req.filename, filename, MAX_FILENAME - 1);
    strncpy(req.content, tag, sizeof(req.content) - 1);
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

    if (resp.status != 0) {
        fprintf(stderr, "%s\n", resp.error_msg);
        client_disconnect_from_ss(client);
        return ERR_SERVER_ERROR;
    }

    printf("%s\n", resp.error_msg);
    client_disconnect_from_ss(client);
    return SUCCESS;
}

// VIEWCHECKPOINT command
int cmd_viewcheckpoint(Client *client, const char *filename, const char *tag) {
    if (!client || !client->connected || !filename || !tag) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "READ %s", filename);

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

    char ss_ip[16];
    int ss_port;
    if (sscanf(response, "SS_INFO %s %d", ss_ip, &ss_port) != 2) {
        fprintf(stderr, "Invalid response from Name Server\n");
        return ERR_SERVER_ERROR;
    }

    if (client_connect_to_ss(client, ss_ip, ss_port) != SUCCESS) {
        return ERR_CONNECTION;
    }

    ClientRequest req;
    memset(&req, 0, sizeof(ClientRequest));
    req.op_type = OP_VIEWCHECKPOINT;
    strncpy(req.filename, filename, MAX_FILENAME - 1);
    strncpy(req.content, tag, sizeof(req.content) - 1);

    if (send_request_to_ss(client, &req) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    ClientRequest resp;
    if (recv_response_from_ss(client, &resp) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    if (resp.status != 0) {
        fprintf(stderr, "%s\n", resp.error_msg);
        client_disconnect_from_ss(client);
        return ERR_SERVER_ERROR;
    }

    printf("%s\n", resp.content);
    client_disconnect_from_ss(client);
    return SUCCESS;
}

// REVERT command
int cmd_revert(Client *client, const char *filename, const char *tag) {
    if (!client || !client->connected || !filename || !tag) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "WRITE_LOCK %s", filename);

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

    char ss_ip[16];
    int ss_port;
    if (sscanf(response, "SS_INFO %s %d", ss_ip, &ss_port) != 2) {
        fprintf(stderr, "Invalid response from Name Server\n");
        return ERR_SERVER_ERROR;
    }

    if (client_connect_to_ss(client, ss_ip, ss_port) != SUCCESS) {
        return ERR_CONNECTION;
    }

    ClientRequest req;
    memset(&req, 0, sizeof(ClientRequest));
    req.op_type = OP_REVERT;
    strncpy(req.filename, filename, MAX_FILENAME - 1);
    strncpy(req.content, tag, sizeof(req.content) - 1);
    strncpy(req.username, client->username, 63);

    if (send_request_to_ss(client, &req) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    ClientRequest resp;
    if (recv_response_from_ss(client, &resp) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    if (resp.status != 0) {
        fprintf(stderr, "%s\n", resp.error_msg);
        client_disconnect_from_ss(client);
        return ERR_SERVER_ERROR;
    }

    printf("%s\n", resp.error_msg);
    client_disconnect_from_ss(client);
    return SUCCESS;
}

// LISTCHECKPOINTS command
int cmd_listcheckpoints(Client *client, const char *filename) {
    if (!client || !client->connected || !filename) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "READ %s", filename);

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

    char ss_ip[16];
    int ss_port;
    if (sscanf(response, "SS_INFO %s %d", ss_ip, &ss_port) != 2) {
        fprintf(stderr, "Invalid response from Name Server\n");
        return ERR_SERVER_ERROR;
    }

    if (client_connect_to_ss(client, ss_ip, ss_port) != SUCCESS) {
        return ERR_CONNECTION;
    }

    ClientRequest req;
    memset(&req, 0, sizeof(ClientRequest));
    req.op_type = OP_LISTCHECKPOINTS;
    strncpy(req.filename, filename, MAX_FILENAME - 1);

    if (send_request_to_ss(client, &req) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    ClientRequest resp;
    if (recv_response_from_ss(client, &resp) != SUCCESS) {
        client_disconnect_from_ss(client);
        return ERR_CONNECTION;
    }

    if (resp.status != 0) {
        fprintf(stderr, "%s\n", resp.error_msg);
        client_disconnect_from_ss(client);
        return ERR_SERVER_ERROR;
    }

    printf("%s\n", resp.content);
    client_disconnect_from_ss(client);
    return SUCCESS;
}