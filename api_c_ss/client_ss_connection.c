#include "client_ss_connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

volatile sig_atomic_t keep_running = 1;

int ss_init_client_listener(int port) {
    int listen_fd;
    struct sockaddr_in server_addr;
    int opt = 1;

    // Create socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Set socket options
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        close(listen_fd);
        return -1;
    }

    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_fd);
        return -1;
    }

    // Listen for connections
    if (listen(listen_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(listen_fd);
        return -1;
    }

    printf("[SS] Listening for client connections on port %d\n", port);
    return listen_fd;
}

int ss_init_client_manager(ClientManager *manager, const char *base_path) {
    if (!manager || !base_path) {
        return -1;
    }

    memset(manager, 0, sizeof(ClientManager));
    strncpy(manager->base_path, base_path, MAX_FILENAME - 1);
    manager->client_count = 0;
    
    if (pthread_mutex_init(&manager->lock, NULL) != 0) {
        perror("Mutex initialization failed");
        return -1;
    }

    printf("[SS] Client manager initialized with base path: %s\n", base_path);
    return 0;
}

int ss_send_to_client(int client_fd, ClientRequest *request) {
    if (!request) {
        return -1;
    }

    ssize_t sent = send(client_fd, request, sizeof(ClientRequest), 0);
    if (sent < 0) {
        perror("[SS] Failed to send to client");
        return -1;
    }

    return 0;
}

int ss_receive_from_client(int client_fd, ClientRequest *request) {
    if (!request) {
        return -1;
    }

    memset(request, 0, sizeof(ClientRequest));
    ssize_t received = recv(client_fd, request, sizeof(ClientRequest), 0);
    
    if (received < 0) {
        perror("[SS] Failed to receive from client");
        return -1;
    }
    
    if (received == 0) {
        // Client disconnected
        return -1;
    }

    return 0;
}

int ss_handle_read(int client_fd, ClientRequest *request, const char *base_path) {
    char file_path[MAX_FILENAME];
    ClientRequest response;
    FILE *fp;
    size_t file_size;

    snprintf(file_path, MAX_FILENAME, "%s/%s", base_path, request->filename);
    
    memset(&response, 0, sizeof(ClientRequest));
    response.op_type = OP_ACK;
    strncpy(response.filename, request->filename, MAX_FILENAME - 1);

    // Open file for reading
    fp = fopen(file_path, "r");
    if (!fp) {
        response.status = -1;
        snprintf(response.error_msg, 512, "Failed to open file: %s", strerror(errno));
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Read file content
    if (file_size > 0 && file_size < MAX_BUFFER_SIZE) {
        size_t read_size = fread(response.content, 1, file_size, fp);
        response.content[read_size] = '\0';
        response.status = 0;
    } else if (file_size >= MAX_BUFFER_SIZE) {
        response.status = -1;
        snprintf(response.error_msg, 512, "File too large");
    } else {
        response.content[0] = '\0';
        response.status = 0;
    }

    fclose(fp);

    // Send response
    if (ss_send_to_client(client_fd, &response) < 0) {
        return -1;
    }

    printf("[SS] READ completed for file: %s\n", request->filename);
    return 0;
}

int ss_handle_write(int client_fd, ClientRequest *request, const char *base_path) {
    char file_path[MAX_FILENAME];
    ClientRequest response;
    
    snprintf(file_path, MAX_FILENAME, "%s/%s", base_path, request->filename);
    
    memset(&response, 0, sizeof(ClientRequest));
    response.op_type = OP_ACK;
    strncpy(response.filename, request->filename, MAX_FILENAME - 1);

    // For this simplified version, we'll append to the file
    // In the full implementation, you'll need sentence-level editing
    FILE *fp = fopen(file_path, "a");
    if (!fp) {
        response.status = -1;
        snprintf(response.error_msg, 512, "Failed to open file: %s", strerror(errno));
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    fprintf(fp, "%s", request->content);
    fclose(fp);

    response.status = 0;
    snprintf(response.error_msg, 512, "Write successful");

    if (ss_send_to_client(client_fd, &response) < 0) {
        return -1;
    }

    printf("[SS] WRITE completed for file: %s\n", request->filename);
    return 0;
}

int ss_handle_stream(int client_fd, ClientRequest *request, const char *base_path) {
    char file_path[MAX_FILENAME];
    ClientRequest response;
    FILE *fp;
    char word[256];
    int ch, idx;

    snprintf(file_path, MAX_FILENAME, "%s/%s", base_path, request->filename);
    
    fp = fopen(file_path, "r");
    if (!fp) {
        memset(&response, 0, sizeof(ClientRequest));
        response.op_type = OP_ERROR;
        response.status = -1;
        snprintf(response.error_msg, 512, "Failed to open file: %s", strerror(errno));
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    printf("[SS] Starting STREAM for file: %s\n", request->filename);

    // Stream word by word
    idx = 0;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == ' ' || ch == '\n' || ch == '\t') {
            if (idx > 0) {
                word[idx] = '\0';
                
                // Send word
                memset(&response, 0, sizeof(ClientRequest));
                response.op_type = OP_ACK;
                response.status = 0;
                strncpy(response.content, word, MAX_BUFFER_SIZE - 1);
                
                if (ss_send_to_client(client_fd, &response) < 0) {
                    fclose(fp);
                    return -1;
                }
                
                // 0.1 second delay
                usleep(100000);
                idx = 0;
            }
        } else {
            if (idx < 255) {
                word[idx++] = ch;
            }
        }
    }

    // Send last word if exists
    if (idx > 0) {
        word[idx] = '\0';
        memset(&response, 0, sizeof(ClientRequest));
        response.op_type = OP_ACK;
        response.status = 0;
        strncpy(response.content, word, MAX_BUFFER_SIZE - 1);
        ss_send_to_client(client_fd, &response);
    }

    // Send STOP signal
    memset(&response, 0, sizeof(ClientRequest));
    response.op_type = OP_STOP;
    response.status = 0;
    ss_send_to_client(client_fd, &response);

    fclose(fp);
    printf("[SS] STREAM completed for file: %s\n", request->filename);
    return 0;
}

void *ss_client_handler(void *arg) {
    ClientThreadData *thread_data = (ClientThreadData *)arg;
    ClientRequest request;
    ClientManager *manager = (ClientManager *)thread_data; // Note: You'll need to pass manager properly
    
    printf("[SS] Thread %d: Handling client %s:%d\n", 
           thread_data->thread_id, thread_data->client_ip, thread_data->client_port);

    while (thread_data->active && keep_running) {
        // Receive request from client
        if (ss_receive_from_client(thread_data->client_fd, &request) < 0) {
            printf("[SS] Thread %d: Client disconnected\n", thread_data->thread_id);
            break;
        }

        printf("[SS] Thread %d: Received operation type %d for file %s\n",
               thread_data->thread_id, request.op_type, request.filename);

        // Handle request based on operation type
        switch (request.op_type) {
            case OP_READ:
                ss_handle_read(thread_data->client_fd, &request, 
                              ((ClientManager *)arg)->base_path);
                break;
            
            case OP_WRITE:
                ss_handle_write(thread_data->client_fd, &request,
                               ((ClientManager *)arg)->base_path);
                break;
            
            case OP_STREAM:
                ss_handle_stream(thread_data->client_fd, &request,
                                ((ClientManager *)arg)->base_path);
                break;
            
            default:
                printf("[SS] Thread %d: Unknown operation type %d\n",
                       thread_data->thread_id, request.op_type);
                break;
        }
    }

    // Cleanup
    close(thread_data->client_fd);
    thread_data->active = 0;
    
    printf("[SS] Thread %d: Exiting\n", thread_data->thread_id);
    return NULL;
}

int ss_accept_clients(int listen_fd, ClientManager *manager) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd;

    // Set socket to non-blocking mode
    int flags = fcntl(listen_fd, F_GETFL, 0);
    fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

    while (keep_running) {
        // Accept new client connection
        client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No pending connections, sleep briefly and check keep_running
                usleep(100000); // 100ms
                continue;
            }
            perror("[SS] Accept failed");
            continue;
        }

        // Lock manager for thread creation
        pthread_mutex_lock(&manager->lock);

        // Find free slot
        int thread_id = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!manager->clients[i].active) {
                thread_id = i;
                break;
            }
        }

        if (thread_id == -1) {
            printf("[SS] Maximum clients reached, rejecting connection\n");
            close(client_fd);
            pthread_mutex_unlock(&manager->lock);
            continue;
        }

        // Setup thread data
        manager->clients[thread_id].client_fd = client_fd;
        manager->clients[thread_id].thread_id = thread_id;
        manager->clients[thread_id].active = 1;
        manager->clients[thread_id].client_port = ntohs(client_addr.sin_port);
        inet_ntop(AF_INET, &client_addr.sin_addr, 
                  manager->clients[thread_id].client_ip, 16);

        // Create thread
        if (pthread_create(&manager->clients[thread_id].thread, NULL, 
                          ss_client_handler, manager) != 0) {
            perror("[SS] Thread creation failed");
            close(client_fd);
            manager->clients[thread_id].active = 0;
        } else {
            pthread_detach(manager->clients[thread_id].thread);
            manager->client_count++;
            printf("[SS] Accepted client %s:%d (Thread %d, Total: %d)\n",
                   manager->clients[thread_id].client_ip,
                   manager->clients[thread_id].client_port,
                   thread_id, manager->client_count);
        }

        printf("[SS] Exiting accept loop due to shutdown signal\n");
        pthread_mutex_unlock(&manager->lock);
    }

    return 0;
}

void ss_cleanup_client(ClientManager *manager, int thread_id) {
    if (thread_id < 0 || thread_id >= MAX_CLIENTS) {
        return;
    }

    pthread_mutex_lock(&manager->lock);
    
    if (manager->clients[thread_id].active) {
        manager->clients[thread_id].active = 0;
        manager->client_count--;
        printf("[SS] Cleaned up thread %d (Remaining: %d)\n", 
               thread_id, manager->client_count);
    }
    
    pthread_mutex_unlock(&manager->lock);
}