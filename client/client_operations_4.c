// ========================== FOLDER OPERATIONS ============================

#include "client.h"

/**
 * CREATEFOLDER command - Create a new folder
 */
int cmd_createfolder(Client *client, const char *folderpath) {
    if (!client || !client->connected || !folderpath) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "CREATEFOLDER %s", folderpath);

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
 * MOVE command - Move file to a folder
 */
int cmd_move(Client *client, const char *filename, const char *folderpath) {
    if (!client || !client->connected || !filename || !folderpath) {
        return ERR_INVALID_COMMAND;
    }

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "MOVE %s %s", filename, folderpath);

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
 * VIEWFOLDER command - List folder contents
 */
int cmd_viewfolder(Client *client, const char *folderpath) {
    if (!client || !client->connected || !folderpath) {
        return ERR_INVALID_COMMAND;
    }

    // char request[MAX_BUFFER_SIZE];
    // snprintf(request, sizeof(request), "VIEWFOLDER %s", folderpath);

    // Build full path with root/ prefix if needed
    char full_path[512];
    if (strncmp(folderpath, "root/", 5) == 0 || strcmp(folderpath, "root") == 0) {
        // Already has root prefix
        strncpy(full_path, folderpath, sizeof(full_path) - 1);
    } else {
        // Add root prefix
        snprintf(full_path, sizeof(full_path), "root/%s", folderpath);
    }
    full_path[sizeof(full_path) - 1] = '\0';

    char request[MAX_BUFFER_SIZE];
    snprintf(request, sizeof(request), "VIEWFOLDER %s", full_path);

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