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
#include <signal.h>
#include <arpa/inet.h>

volatile sig_atomic_t ns_connection_lost = 0;
extern volatile sig_atomic_t keep_running;
// extern ClientManager *g_client_manager;

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

    printf("\n\n\n");

    printf("==================DEBUG=============================\n");
    printf("data: %s\n", msg->data);
    printf("mess: %s\n", msg->message);
    printf("data_len:%d\n", msg->data_len);
    printf("status:%d\n", msg->status);
    printf("type:%d\n", msg->type);
    printf("=====================================================\n");
    printf("\n\n\n");

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

int ss_handle_ns_commands(int sock_fd, const char *base_path, ClientManager *client_manager)
{
    ProtocolMessage msg, response;
    char file_path[MAX_PATH_LEN];

    while (keep_running)
    {
        if (ss_receive_message(sock_fd, &msg) < 0)
        {
            fprintf(stderr, "[SS] Connection to NS lost\n");
            ns_connection_lost = 1; // Set flag to signal shutdown
            keep_running = 0;       // Stop the loop
            return -1;
        }

        // DEBUG: Print what we received

        printf("[DEBUG SS] Received ProtocolMessage:\n");
        printf("  - sizeof(ProtocolMessage) = %zu bytes\n", sizeof(ProtocolMessage));
        printf("  - msg.type (raw) = %d (hex: 0x%08x)\n", msg.type, msg.type);
        printf("  - msg.status (raw) = %d (hex: 0x%08x)\n", msg.status, msg.status);
        printf("  - msg.data = '%s' (length: %zu)\n", msg.data, strlen(msg.data));
        printf("  - msg.message = '%s'\n", msg.message);
        printf("  - First 32 bytes of data field (hex): ");
        for (int i = 0; i < 32 && i < (int)sizeof(msg.data); i++)
        {
            printf("%02x ", (unsigned char)msg.data[i]);
        }
        printf("\n");

        // CONVERT FROM NETWORK BYTE ORDER
        int command_type = ntohl(msg.type);

        printf("[DEBUG SS] After ntohl:\n");
        printf("  - command_type = %d\n", command_type);
        printf("  - Expected MSG_CREATE_FILE = %d\n", MSG_CREATE_FILE);
        printf("  - Expected MSG_DELETE_FILE = %d\n", MSG_DELETE_FILE);

        printf("[SS] Received command type: %d, data: %s\n",
               command_type, msg.data);
        memset(&response, 0, sizeof(ProtocolMessage));
        response.type = htonl(MSG_FILE_OP_ACK);

        switch (command_type)
        {
        case MSG_CREATE_FILE:
        {
            // snprintf(file_path, MAX_PATH_LEN, "%s/%s", base_path, msg.data);
            char full_path[MAX_PATH_LEN];
            snprintf(full_path, MAX_PATH_LEN, "root/%s", msg.data);
            snprintf(file_path, MAX_PATH_LEN, "%s/%s", base_path, full_path);

            // Ensure root directory exists
            char root_dir[MAX_PATH_LEN];
            snprintf(root_dir, sizeof(root_dir), "%s/root", base_path);
            mkdir(root_dir, 0755);

            FILE *fp = fopen(file_path, "w");
            if (fp)
            {
                fclose(fp);
                response.status = htonl(0);
                snprintf(response.message, MAX_BUFFER_SIZE,
                         "SUCCESS: File created: %s", msg.data);
                printf("[SS] ✓ Created file: %s\n", file_path);
            }
            else
            {
                response.status = htonl(-1);
                snprintf(response.message, MAX_BUFFER_SIZE,
                         "ERROR: Failed to create file: %s", strerror(errno));
                fprintf(stderr, "[SS] ✗ Failed to create: %s\n", file_path);
            }

            // Send response back to NS
            if (send(sock_fd, &response, sizeof(ProtocolMessage), 0) < 0)
            {
                fprintf(stderr, "[SS] Failed to send CREATE response: %s\n", strerror(errno));
                return -1;
            }
            printf("[SS] CREATE response sent (status: %d)\n", ntohl(response.status));
            break;
        }

        case MSG_DELETE_FILE:
        {
            snprintf(file_path, MAX_PATH_LEN, "%s/%s", base_path, msg.data);

            if (unlink(file_path) == 0)
            {
                response.status = htonl(0);
                snprintf(response.message, MAX_BUFFER_SIZE,
                         "SUCCESS: File deleted: %s", msg.data);
                printf("[SS] ✓ Deleted: %s\n", file_path);
            }
            else
            {
                response.status = htonl(-1);
                snprintf(response.message, MAX_BUFFER_SIZE,
                         "ERROR: Failed to delete: %s", strerror(errno));
                fprintf(stderr, "[SS] ✗ Failed to delete: %s\n", file_path);
            }

            // Send response back to NS
            if (send(sock_fd, &response, sizeof(ProtocolMessage), 0) < 0)
            {
                fprintf(stderr, "[SS] Failed to send DELETE response: %s\n", strerror(errno));
                return -1;
            }
            printf("[SS] DELETE response sent (status: %d)\n", ntohl(response.status));
            break;
        }

        case MSG_HEARTBEAT:
            response.type = htonl(MSG_HEARTBEAT);
            response.status = htonl(0);
            strcpy(response.message, "Alive");

            // Send heartbeat response
            if (send(sock_fd, &response, sizeof(ProtocolMessage), 0) < 0)
            {
                fprintf(stderr, "[SS] Failed to send HEARTBEAT response: %s\n", strerror(errno));
                return -1;
            }
            break;

        case MSG_CREATE_FOLDER:
        {
            printf("[SS] Processing CREATEFOLDER command\n");

            if (!client_manager || !client_manager->file_manager)
            {
                memset(&response, 0, sizeof(ProtocolMessage));
                response.type = htonl(MSG_ERROR);
                response.status = htonl(-1);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: File manager not initialized");
                send(sock_fd, &response, sizeof(ProtocolMessage), 0);
                break;
            }

            // Parse data: "folderpath|username"
            char folderpath[512];
            char username[64];
            if (sscanf(msg.data, "%511[^|]|%63s", folderpath, username) != 2)
            {
                memset(&response, 0, sizeof(ProtocolMessage));
                response.type = htonl(MSG_ERROR);
                response.status = htonl(-1);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: Invalid CREATEFOLDER data format");
                send(sock_fd, &response, sizeof(ProtocolMessage), 0);
                break;
            }

            printf("[SS] Creating folder hierarchy: %s (owner: %s)\n", folderpath, username);

            // **FIX: Clear response buffer before processing**
            memset(&response, 0, sizeof(ProtocolMessage));

            // Use FileManager's folder hierarchy system
            int result = folder_create_hierarchy(client_manager->file_manager, folderpath, username);

            if (result == 0)
            {
                response.type = htonl(MSG_FILE_OP_ACK);
                response.status = htonl(0);
                snprintf(response.message, sizeof(response.message),
                         "SUCCESS: Folder '%s' created", folderpath);
                printf("[SS] Folder created successfully: %s\n", folderpath);
            }
            else
            {
                response.type = htonl(MSG_ERROR);
                response.status = htonl(-1);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: Failed to create folder '%s'", folderpath);
                fprintf(stderr, "[SS] Failed to create folder: %s\n", folderpath);
            }

            if (send(sock_fd, &response, sizeof(ProtocolMessage), 0) < 0)
            {
                perror("[SS] Failed to send folder create response");
                return -1;
            }

            printf("[SS] CREATEFOLDER response sent (status: %d, msg: %.50s)\n",
                   ntohl(response.status), response.message);
            break;
        }

        case MSG_MOVE_FILE:
        {
            printf("[SS] Processing MOVE command\n");

            if (!client_manager || !client_manager->file_manager)
            {
                memset(&response, 0, sizeof(ProtocolMessage));
                response.type = htonl(MSG_ERROR);
                response.status = htonl(-1);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: File manager not initialized");
                send(sock_fd, &response, sizeof(ProtocolMessage), 0);
                break;
            }

            // Parse data: "filename|folderpath|username"
            char filename[256];
            char folderpath[512];
            char username[64];
            if (sscanf(msg.data, "%255[^|]|%511[^|]|%63s",
                       filename, folderpath, username) != 3)
            {
                memset(&response, 0, sizeof(ProtocolMessage));
                response.type = htonl(MSG_ERROR);
                response.status = htonl(-1);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: Invalid MOVE data format");
                send(sock_fd, &response, sizeof(ProtocolMessage), 0);
                break;
            }

            printf("[SS] Moving file '%s' to folder '%s'\n", filename, folderpath);

            // **FIX: Clear response buffer before processing**
            memset(&response, 0, sizeof(ProtocolMessage));

            // Use FileManager's folder move function
            int result = folder_move_file(client_manager->file_manager,
                                          filename, folderpath, username);

            if (result == 0)
            {
                response.type = htonl(MSG_FILE_OP_ACK);
                response.status = htonl(0);
                snprintf(response.message, sizeof(response.message),
                         "SUCCESS: File '%s' moved to '%s'", filename, folderpath);
                printf("[SS] File moved successfully\n");
            }
            else if (result == -2)
            {
                response.type = htonl(MSG_ERROR);
                response.status = htonl(-2);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: Access denied - only owner can move files");
                fprintf(stderr, "[SS] Access denied for user '%s'\n", username);
            }
            else if (result == -3)
            {
                response.type = htonl(MSG_ERROR);
                response.status = htonl(-3);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: Destination folder '%s' not found", folderpath);
                fprintf(stderr, "[SS] Folder not found: %s\n", folderpath);
            }
            else
            {
                response.type = htonl(MSG_ERROR);
                response.status = htonl(-1);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: Failed to move file");
                fprintf(stderr, "[SS] Move operation failed\n");
            }

            if (send(sock_fd, &response, sizeof(ProtocolMessage), 0) < 0)
            {
                fprintf(stderr, "[SS] Failed to send MOVE response: %s\n", strerror(errno));
                return -1;
            }

            printf("[SS] MOVE response sent (status: %d, msg: %.50s)\n",
                   ntohl(response.status), response.message);
            break;
        }

        case MSG_VIEW_FOLDER:
        {
            printf("[SS] Processing VIEWFOLDER command\n");

            if (!client_manager || !client_manager->file_manager)
            {
                response.type = htonl(MSG_ERROR);
                response.status = htonl(-1);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: File manager not initialized");
                send(sock_fd, &response, sizeof(ProtocolMessage), 0);
                break;
            }

            // Parse data: "folderpath|username"
            char folderpath[512];
            char username[64];
            if (sscanf(msg.data, "%511[^|]|%63s", folderpath, username) != 2)
            {
                response.type = htonl(MSG_ERROR);
                response.status = htonl(-1);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: Invalid VIEWFOLDER data format");
                send(sock_fd, &response, sizeof(ProtocolMessage), 0);
                break;
            }

            printf("[SS] Viewing folder: %s\n", folderpath);

            // Use FileManager's folder list function
            char folder_contents[MAX_BUFFER_SIZE];
            memset(folder_contents, 0, sizeof(folder_contents)); // CLEAR BUFFER

            char *result = folder_list_contents(client_manager->file_manager,
                                                folderpath, folder_contents,
                                                sizeof(folder_contents), username);

            // CLEAR response structure completely
            memset(&response, 0, sizeof(ProtocolMessage));
            response.type = htonl(MSG_FILE_OP_ACK);

            if (result)
            {
                // response.type = htonl(MSG_FILE_OP_ACK);
                response.status = htonl(0);
                strncpy(response.message, folder_contents, sizeof(response.message) - 1);
                response.message[sizeof(response.message) - 1] = '\0';
                // printf("[SS] Folder contents retrieved successfully\n");
                printf("[SS] Folder contents retrieved successfully\n");
                printf("[SS] Message length: %zu\n", strlen(response.message));
                printf("[SS] First 100 chars: '%.100s'\n", response.message);
            }
            else
            {
                // response.type = htonl(MSG_ERROR);
                response.status = htonl(-1);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: Folder '%s' not found", folderpath);
                fprintf(stderr, "[SS] Folder not found: %s\n", folderpath);
            }

            if (send(sock_fd, &response, sizeof(ProtocolMessage), 0) < 0)
            {
                fprintf(stderr, "[SS] Failed to send VIEWFOLDER response: %s\n", strerror(errno));
                break;
            }

            printf("[SS] Folder contents sent (status: %d, msg_len: %zu)\n",
                   ntohl(response.status), strlen(response.message));
            break;
        }

        case MSG_GET_METADATA:
        {
            printf("[SS] Processing GET_METADATA command\n");

            if (!client_manager || !client_manager->file_manager)
            {
                memset(&response, 0, sizeof(ProtocolMessage));
                response.type = htonl(MSG_ERROR);
                response.status = htonl(-1);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: File manager not initialized");
                send(sock_fd, &response, sizeof(ProtocolMessage), 0);
                break;
            }

            // Parse filename from msg.data
            char filename[512];
            strncpy(filename, msg.data, sizeof(filename) - 1);
            filename[sizeof(filename) - 1] = '\0';

            printf("[SS] Getting metadata for: %s\n", filename);

            // Get file structure
            FileStructure *fs = fm_get_file(client_manager->file_manager, filename);

            memset(&response, 0, sizeof(ProtocolMessage));
            response.type = htonl(MSG_FILE_OP_ACK);

            if (!fs)
            {
                response.status = htonl(-1);
                snprintf(response.message, sizeof(response.message),
                         "ERROR: File not found");
                printf("[SS] File not found: %s\n", filename);
            }
            else
            {
                // Calculate metadata by traversing the file structure
                long file_size = 0;
                int word_count = 0;
                int char_count = 0;

                pthread_rwlock_rdlock(&fs->file_lock);

                SentenceNode *sentence = fs->sentences;
                while (sentence)
                {
                    pthread_rwlock_rdlock(&sentence->lock);

                    // Count words in this sentence
                    WordNode *word = sentence->words;
                    while (word)
                    {
                        word_count++;

                        // Count characters in word
                        int word_len = strlen(word->word);
                        char_count += word_len;
                        file_size += word_len;

                        // Count whitespace after word
                        int ws_len = strlen(word->whitespace_after);
                        char_count += ws_len;
                        file_size += ws_len;

                        word = word->next;
                    }

                    // Count delimiters (., !, ?, etc.)
                    int delim_len = strlen(sentence->delimiters);
                    char_count += delim_len;
                    file_size += delim_len;
                    // Count whitespace after delimiters
                    int ws_delim_len = strlen(sentence->whitespace_after_delimiters);
                    char_count += ws_delim_len;
                    file_size += ws_delim_len;

                    pthread_rwlock_unlock(&sentence->lock);
                    sentence = sentence->next;
                }

                pthread_rwlock_unlock(&fs->file_lock);

                response.status = htonl(0);
                snprintf(response.message, sizeof(response.message),
                         "%ld|%d|%d", file_size, word_count, char_count);

                printf("[SS] Metadata calculated: size=%ld, words=%d, chars=%d\n",
                       file_size, word_count, char_count);
            }

            if (send(sock_fd, &response, sizeof(ProtocolMessage), 0) < 0)
            {
                fprintf(stderr, "[SS] Failed to send METADATA response: %s\n", strerror(errno));
                return -1;
            }

            printf("[SS] METADATA response sent (status: %d)\n", ntohl(response.status));
            break;
        }

        default:
            memset(&response, 0, sizeof(ProtocolMessage));
            response.type = htonl(MSG_ERROR);
            response.status = htonl(-1);
            snprintf(response.message, MAX_BUFFER_SIZE,
                     "ERROR: Unknown command: %d", command_type);
            fprintf(stderr, "[SS] Unknown command: %d\n", command_type);

            // Send error response for unknown commands
            if (send(sock_fd, &response, sizeof(ProtocolMessage), 0) < 0)
            {
                fprintf(stderr, "[SS] Failed to send error response: %s\n", strerror(errno));
                return -1;
            }
            break;
        }

        // Note: All case blocks handle their own response sending
        // No need to send again here - it would cause duplicate sends!
    }

    return 0;
}