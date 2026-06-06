#include "client.h"

/**
 * REQUEST command - Request read/write access to a file
 */
int cmd_request_access(Client *client, const char *filename, const char *access_type) {
    if (!client || !client->connected || !filename || !access_type) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "REQUEST %s %s", filename, access_type);

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

/**
 * VIEWREQUESTS command - View pending access requests for your files
 */
int cmd_view_requests(Client *client) {
    if (!client || !client->connected) {
        return ERR_CONNECTION;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "VIEWREQUESTS");

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

/**
 * APPROVE command - Approve an access request
 */
int cmd_approve_request(Client *client, const char *filename, const char *username, const char *access_type) {
    if (!client || !client->connected || !filename || !username || !access_type) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "APPROVE %s %s %s", filename, username, access_type);

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

/**
 * REJECT command - Reject an access request
 */
int cmd_reject_request(Client *client, const char *filename, const char *username, const char *access_type) {
    if (!client || !client->connected || !filename || !username || !access_type) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "REJECT %s %s %s", filename, username, access_type);

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