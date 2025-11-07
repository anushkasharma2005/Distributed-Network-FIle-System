#include "../api_c_ns/networking.h"
#include "ns_ss_connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

int ss_connect_to_ns(const char *ns_ip, int ns_port)
{
    int sock_fd;
    struct sockaddr_in ns_addr;

    // Create socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    // Setup NS address structure
    memset(&ns_addr, 0, sizeof(ns_addr));
    ns_addr.sin_family = AF_INET;
    ns_addr.sin_port = htons(ns_port);

    if (inet_pton(AF_INET, ns_ip, &ns_addr.sin_addr) <= 0)
    {
        perror("Invalid NS IP address");
        close(sock_fd);
        return -1;
    }

    // Connect to Name Server
    if (connect(sock_fd, (struct sockaddr *)&ns_addr, sizeof(ns_addr)) < 0)
    {
        perror("Connection to NS failed");
        close(sock_fd);
        return -1;
    }

    printf("[SS] Connected to Name Server at %s:%d\n", ns_ip, ns_port);
    return sock_fd;
}

int ss_send_message(int sock_fd, ProtocolMessage *msg)
{
    if (!msg)
    {
        fprintf(stderr, "[SS] Error: NULL message\n");
        return -1;
    }

    ssize_t sent = send(sock_fd, msg, sizeof(ProtocolMessage), 0);
    if (sent < 0)
    {
        perror("[SS] Failed to send message");
        return -1;
    }

    return 0;
}

int ss_receive_message(int sock_fd, ProtocolMessage *msg)
{
    if (!msg)
    {
        fprintf(stderr, "[SS] Error: NULL message buffer\n");
        return -1;
    }

    memset(msg, 0, sizeof(ProtocolMessage));
    ssize_t received = recv(sock_fd, msg, sizeof(ProtocolMessage), 0);

    if (received < 0)
    {
        perror("[SS] Failed to receive message");
        return -1;
    }

    if (received == 0)
    {
        fprintf(stderr, "[SS] Connection closed by Name Server\n");
        return -1;
    }

    return 0;
}

int ss_scan_files(const char *base_path, SSRegistrationData *reg_data)
{
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char full_path[MAX_PATH_LEN];
    int count = 0;

    dir = opendir(base_path);
    if (!dir)
    {
        perror("[SS] Failed to open directory");
        return -1;
    }

    reg_data->file_count = 0;

    while ((entry = readdir(dir)) != NULL && count < MAX_FILES)
    {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        snprintf(full_path, MAX_PATH_LEN, "%s/%s", base_path, entry->d_name);

        if (stat(full_path, &statbuf) == 0)
        {
            if (S_ISREG(statbuf.st_mode))
            {
                // Regular file
                strncpy(reg_data->files[count].path, entry->d_name, MAX_PATH_LEN - 1);
                reg_data->files[count].path[MAX_PATH_LEN - 1] = '\0';
                count++;
            }
        }
    }

    closedir(dir);
    reg_data->file_count = count;

    printf("[SS] Scanned %d files from %s\n", count, base_path);
    return count;
}

int ss_register_with_ns(int sock_fd, SSRegistrationData *reg_data)
{
    // Use the same string-based protocol as Name Server (send_message/recv_message)
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    snprintf(buffer, sizeof(buffer), "SS_REGISTER %s %d %d %d",
             reg_data->ss_ip, reg_data->nm_port, reg_data->client_port, reg_data->file_count);

    if (send_message(sock_fd, buffer) < 0)
    {
        fprintf(stderr, "[SS] Failed to send registration\n");
        return -1;
    }

    printf("[SS] Registration sent to NS\n");

    // Wait for string acknowledgment
    char ack[MAX_BUFFER_SIZE];
    int r = recv_message(sock_fd, ack, sizeof(ack));
    if (r <= 0)
    {
        fprintf(stderr, "[SS] Failed to receive registration acknowledgment\n");
        return -1;
    }

    // Simple check: expect ACK substring
    if (strstr(ack, "ACK") == NULL)
    {
        fprintf(stderr, "[SS] Registration failed: %s\n", ack);
        return -1;
    }

    printf("[SS] Registration successful: %s\n", ack);
    return 0;
}

int ss_send_heartbeat(int sock_fd)
{
    ProtocolMessage msg;
    memset(&msg, 0, sizeof(ProtocolMessage));
    msg.type = MSG_HEARTBEAT;
    msg.status = 0;
    strcpy(msg.message, "Heartbeat");

    return ss_send_message(sock_fd, &msg);
}

int ss_handle_ns_commands(int sock_fd, const char *base_path)
{
    ProtocolMessage msg, response;
    char file_path[MAX_PATH_LEN];

    while (1)
    {
        // Receive command from NS
        if (ss_receive_message(sock_fd, &msg) < 0)
        {
            fprintf(stderr, "[SS] Connection to NS lost\n");
            return -1;
        }

        // Prepare response
        memset(&response, 0, sizeof(ProtocolMessage));
        response.type = MSG_FILE_OP_ACK;

        switch (msg.type)
        {
        case MSG_CREATE_FILE:
        {
            // Extract filename from message data
            snprintf(file_path, MAX_PATH_LEN, "%s/%s", base_path, msg.data);

            FILE *fp = fopen(file_path, "w");
            if (fp)
            {
                fclose(fp);
                response.status = 0;
                snprintf(response.message, MAX_BUFFER_SIZE,
                         "File created: %s", msg.data);
                printf("[SS] Created file: %s\n", file_path);
            }
            else
            {
                response.status = -1;
                snprintf(response.message, MAX_BUFFER_SIZE,
                         "Failed to create file: %s", strerror(errno));
                fprintf(stderr, "[SS] Failed to create file: %s\n", file_path);
            }
            break;
        }

        case MSG_DELETE_FILE:
        {
            // Extract filename from message data
            snprintf(file_path, MAX_PATH_LEN, "%s/%s", base_path, msg.data);

            if (unlink(file_path) == 0)
            {
                response.status = 0;
                snprintf(response.message, MAX_BUFFER_SIZE,
                         "File deleted: %s", msg.data);
                printf("[SS] Deleted file: %s\n", file_path);
            }
            else
            {
                response.status = -1;
                snprintf(response.message, MAX_BUFFER_SIZE,
                         "Failed to delete file: %s", strerror(errno));
                fprintf(stderr, "[SS] Failed to delete file: %s\n", file_path);
            }
            break;
        }

        case MSG_HEARTBEAT:
            // Respond to heartbeat
            response.type = MSG_HEARTBEAT;
            response.status = 0;
            strcpy(response.message, "Alive");
            break;

        default:
            response.status = -1;
            snprintf(response.message, MAX_BUFFER_SIZE,
                     "Unknown command type: %d", msg.type);
            fprintf(stderr, "[SS] Unknown command type: %d\n", msg.type);
        }

        // Send response back to NS
        if (ss_send_message(sock_fd, &response) < 0)
        {
            fprintf(stderr, "[SS] Failed to send response to NS\n");
            return -1;
        }
    }

    return 0;
}