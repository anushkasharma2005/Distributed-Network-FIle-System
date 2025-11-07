#include "client_ss_connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

// ==================== Write Session Management ====================

WriteSession* ss_create_write_session(ClientManager *manager, int client_fd, 
                                      const char *filename, const char *username, 
                                      int sentence_num) {
    if (!manager || !filename || !username) {
        return NULL;
    }

    pthread_mutex_lock(&manager->session_lock);

    WriteSession *session = (WriteSession *)malloc(sizeof(WriteSession));
    if (!session) {
        pthread_mutex_unlock(&manager->session_lock);
        return NULL;
    }

    session->client_fd = client_fd;
    strncpy(session->filename, filename, MAX_FILENAME - 1);
    strncpy(session->username, username, 63);
    session->sentence_num = sentence_num;
    session->is_active = 1;
    session->start_time = time(NULL);
    
    session->next = manager->active_sessions;
    manager->active_sessions = session;

    pthread_mutex_unlock(&manager->session_lock);

    printf("[SS] Write session created for %s (sentence %d) by %s\n", 
           filename, sentence_num, username);
    return session;
}

WriteSession* ss_find_write_session(ClientManager *manager, int client_fd) {
    if (!manager) {
        return NULL;
    }

    pthread_mutex_lock(&manager->session_lock);

    WriteSession *current = manager->active_sessions;
    while (current) {
        if (current->client_fd == client_fd && current->is_active) {
            pthread_mutex_unlock(&manager->session_lock);
            return current;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&manager->session_lock);
    return NULL;
}

void ss_destroy_write_session(ClientManager *manager, int client_fd) {
    if (!manager) {
        return;
    }

    pthread_mutex_lock(&manager->session_lock);

    WriteSession *current = manager->active_sessions;
    WriteSession *prev = NULL;

    while (current) {
        if (current->client_fd == client_fd) {
            if (prev) {
                prev->next = current->next;
            } else {
                manager->active_sessions = current->next;
            }
            
            printf("[SS] Write session destroyed for %s by %s\n", 
                   current->filename, current->username);
            free(current);
            pthread_mutex_unlock(&manager->session_lock);
            return;
        }
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&manager->session_lock);
}

// ==================== Write Operation Handlers ====================
int ss_handle_write_begin(int client_fd, ClientRequest *request, ClientManager *manager) {
    ClientRequest response;
    
    memset(&response, 0, sizeof(ClientRequest));
    response.op_type = OP_ACK;
    strncpy(response.filename, request->filename, MAX_FILENAME - 1);

    // Get or create file structure
    FileStructure *fs = fm_get_or_create_file(manager->file_manager, 
                                               request->filename, 
                                               request->username);
    if (!fs) {
        response.status = -1;
        snprintf(response.error_msg, 512, "Failed to load file");
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    // Create snapshot before writing
    if (fs_create_snapshot(fs, request->username) != 0) {
        fprintf(stderr, "[SS] Warning: Failed to create snapshot for %s\n", 
                request->filename);
    }

    // Try to lock the sentence
    int lock_result = fs_lock_sentence(fs, request->sentence_num, request->username);
    
    if (lock_result == -2) {
        response.status = -2;
        snprintf(response.error_msg, 512, 
                 "Sentence %d is locked by another user", request->sentence_num);
        ss_send_to_client(client_fd, &response);
        return -2;
    } else if (lock_result != 0) {
        response.status = -1;
        snprintf(response.error_msg, 512, "Sentence index out of range");
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    // Create write session
    WriteSession *session = ss_create_write_session(manager, client_fd, 
                                                     request->filename, 
                                                     request->username, 
                                                     request->sentence_num);
    if (!session) {
        fs_unlock_sentence(fs, request->sentence_num);
        response.status = -1;
        snprintf(response.error_msg, 512, "Failed to create write session");
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    response.status = 0;
    snprintf(response.error_msg, 512, "Write session started");
    ss_send_to_client(client_fd, &response);

    printf("[SS] WRITE_BEGIN: Sentence %d locked for %s in file %s\n", 
           request->sentence_num, request->username, request->filename);
    return 0;
}

int ss_handle_write_update(int client_fd, ClientRequest *request, ClientManager *manager) {
    ClientRequest response;
    
    memset(&response, 0, sizeof(ClientRequest));
    response.op_type = OP_ACK;
    strncpy(response.filename, request->filename, MAX_FILENAME - 1);

    // Find active write session
    WriteSession *session = ss_find_write_session(manager, client_fd);
    if (!session) {
        response.status = -1;
        snprintf(response.error_msg, 512, "No active write session");
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    // Get file structure
    FileStructure *fs = fm_get_file(manager->file_manager, session->filename);
    if (!fs) {
        response.status = -1;
        snprintf(response.error_msg, 512, "File not found");
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    // Perform write operation
    int result = fs_write_word(fs, session->sentence_num, request->word_index, 
                               request->content, session->username);
    
    if (result != 0) {
        response.status = -1;
        snprintf(response.error_msg, 512, "Write operation failed");
    } else {
        response.status = 0;
        snprintf(response.error_msg, 512, "Write successful");
    }

    ss_send_to_client(client_fd, &response);
    printf("[SS] WRITE_UPDATE: Updated sentence %d in file %s\n", 
           session->sentence_num, session->filename);
    return 0;
}

int ss_handle_write_end(int client_fd, ClientRequest *request, ClientManager *manager) {
    ClientRequest response;
    
    memset(&response, 0, sizeof(ClientRequest));
    response.op_type = OP_ACK;
    strncpy(response.filename, request->filename, MAX_FILENAME - 1);

    // Find active write session
    WriteSession *session = ss_find_write_session(manager, client_fd);
    if (!session) {
        response.status = -1;
        snprintf(response.error_msg, 512, "No active write session");
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    // Get file structure
    FileStructure *fs = fm_get_file(manager->file_manager, session->filename);
    if (!fs) {
        response.status = -1;
        snprintf(response.error_msg, 512, "File not found");
        ss_send_to_client(client_fd, &response);
        ss_destroy_write_session(manager, client_fd);
        return -1;
    }

    // Unlock sentence
    fs_unlock_sentence(fs, session->sentence_num);

    // Commit changes to disk
    int result = fs_commit_write(fs, manager->base_path);
    
    if (result != 0) {
        response.status = -1;
        snprintf(response.error_msg, 512, "Failed to write to disk");
    } else {
        response.status = 0;
        snprintf(response.error_msg, 512, "Write completed successfully");
    }

    ss_send_to_client(client_fd, &response);
    ss_destroy_write_session(manager, client_fd);

    printf("[SS] WRITE_END: Completed write for file %s\n", session->filename);
    return 0;
}

