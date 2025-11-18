#include "client_ss_connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include "file_structure.h"

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

// void ss_destroy_write_session(ClientManager *manager, int client_fd) {
//     if (!manager) {
//         return;
//     }

//     pthread_mutex_lock(&manager->session_lock);

//     WriteSession *current = manager->active_sessions;
//     WriteSession *prev = NULL;

//     while (current) {
//         if (current->client_fd == client_fd) {

//             // Before destroying the session, unlock the sentence
//             FileStructure *fs = fm_get_file(manager->file_manager, current->filename);
//             if (fs) {
//                 fs_unlock_sentence(fs, current->sentence_num);
//                 printf("[SS] Sentence %d unlocked during session cleanup for %s\n", 
//                        current->sentence_num, current->filename);
//             }

//             if (prev) {
//                 prev->next = current->next;
//             } else {
//                 manager->active_sessions = current->next;
//             }
            
//             printf("[SS] Write session destroyed for %s by %s\n", 
//                    current->filename, current->username);
//             free(current);
//             pthread_mutex_unlock(&manager->session_lock);
//             return;
//         }
//         prev = current;
//         current = current->next;
//     }

//     pthread_mutex_unlock(&manager->session_lock);
// }

void ss_destroy_write_session(ClientManager *manager, int client_fd) {
    if (!manager) {
        return;
    }

    pthread_mutex_lock(&manager->session_lock);

    WriteSession *current = manager->active_sessions;
    WriteSession *prev = NULL;

    while (current) {
        if (current->client_fd == client_fd) {
            // Get main file structure to clean up
            FileStructure *fs = fm_get_file(manager->file_manager, current->filename);
            
            if (current->temp_fs) {
                // Delete temp disk file
                char temp_path[MAX_FILENAME * 2];
                snprintf(temp_path, sizeof(temp_path), "%s/%s", 
                        manager->base_path, current->temp_fs->filename);
                unlink(temp_path);
                
                // Destroy temp file structure
                fs_destroy(current->temp_fs);
                current->temp_fs = NULL;
            }
            
            // Clear write lock on main structure
            if (fs) {
                pthread_rwlock_wrlock(&fs->file_lock);
                fs->has_active_write = 0;
                fs->write_user[0] = '\0';
                fs->temp_filename[0] = '\0';
                pthread_rwlock_unlock(&fs->file_lock);
            }
            
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
// int ss_handle_write_begin(int client_fd, ClientRequest *request, ClientManager *manager) {
//     ClientRequest response;
    
//     memset(&response, 0, sizeof(ClientRequest));
//     response.op_type = OP_ACK;
//     strncpy(response.filename, request->filename, MAX_FILENAME - 1);

//     // Get or create file structure
//     FileStructure *fs = fm_get_or_create_file(manager->file_manager, 
//                                                request->filename, 
//                                                request->username);
//     if (!fs) {
//         response.status = -1;
//         snprintf(response.error_msg, 512, "Failed to load file");
//         ss_send_to_client(client_fd, &response);
//         return -1;
//     }

//     // Create snapshot before writing
//     if (fs_create_snapshot(fs, request->username) != 0) {
//         fprintf(stderr, "[SS] Warning: Failed to create snapshot for %s\n", 
//                 request->filename);
//     }

    
//     // Lock the sentence
//     int lock_result = fs_lock_sentence(fs, request->sentence_num, request->username);
//     if (lock_result == -5) {
//         // Sentence index out of range
//         // ClientRequest resp;
//         // memset(&resp, 0, sizeof(ClientRequest));
//         response.op_type = OP_ERROR;
//         response.status = -5;
//         snprintf(response.error_msg, sizeof(response.error_msg), 
//                 "ERROR: Sentence index %d out of range (file has %d sentences, valid range is 0-%d)",
//                 request->sentence_num, fs->sentence_count, fs->sentence_count);
        
//         ss_send_to_client(client_fd, &response);
//         return -1;
//     } else if (lock_result != 0) {
//         // Other locking errors (already locked, etc.)
//         // ClientRequest resp;
//         // memset(&resp, 0, sizeof(ClientRequest));
//         response.op_type = OP_ERROR;
//         response.status = lock_result;
//         snprintf(response.error_msg, sizeof(response.error_msg), 
//                 "ERROR: Failed to lock sentence %d", request->sentence_num);
        
//         ss_send_to_client(client_fd, &response);
//         return -1;
//     }

//     // Create write session
//     WriteSession *session = ss_create_write_session(manager, client_fd, 
//                                                      request->filename, 
//                                                      request->username, 
//                                                      request->sentence_num);
//     if (!session) {
//         fs_unlock_sentence(fs, request->sentence_num);
//         response.status = -1;
//         snprintf(response.error_msg, 512, "Failed to create write session");
//         ss_send_to_client(client_fd, &response);
//         return -1;
//     }

//     response.status = 0;
//     snprintf(response.error_msg, 512, "Write session started");
//     ss_send_to_client(client_fd, &response);

//     printf("[SS] WRITE_BEGIN: Sentence %d locked for %s in file %s\n", 
//            request->sentence_num, request->username, request->filename);
//     return 0;
// }

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

    // Create snapshot before writing (for UNDO)
    if (fs_create_snapshot(fs, request->username) != 0) {
        fprintf(stderr, "[SS] Warning: Failed to create snapshot for %s\n", 
                request->filename);
    }

    pthread_rwlock_wrlock(&fs->file_lock);

    // Check if sentence_num is valid
    if (request->sentence_num > fs->sentence_count) {
        pthread_rwlock_unlock(&fs->file_lock);
        response.status = -5;
        snprintf(response.error_msg, sizeof(response.error_msg), 
                "ERROR: Sentence index %d out of range (file has %d sentences, valid range is 0-%d)",
                request->sentence_num, fs->sentence_count, fs->sentence_count);
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    // Check if anyone is already writing THIS sentence
    if (fs->has_active_write && fs->write_sentence == request->sentence_num) {
        pthread_rwlock_unlock(&fs->file_lock);
        response.status = -2;
        snprintf(response.error_msg, sizeof(response.error_msg), 
                "ERROR: Sentence %d is already locked by %s",
                request->sentence_num, fs->write_user);
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    // Create temp file: original_file.tmp_username_pid
    snprintf(fs->temp_filename, MAX_FILENAME, "%s.tmp_%s_%d", 
             fs->filename, request->username, getpid());

    // Copy main file to temp file
    char main_path[MAX_FILENAME * 2];
    char temp_path[MAX_FILENAME * 2];
    snprintf(main_path, sizeof(main_path), "%s/%s", manager->base_path, fs->filename);
    snprintf(temp_path, sizeof(temp_path), "%s/%s", manager->base_path, fs->temp_filename);

    // Copy file (or create empty if doesn't exist)
    FILE *src = fopen(main_path, "r");
    FILE *dst = fopen(temp_path, "w");
    
    if (!dst) {
        if (src) fclose(src);
        pthread_rwlock_unlock(&fs->file_lock);
        response.status = -1;
        snprintf(response.error_msg, 512, "Failed to create temp file");
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    if (src) {
        char buffer[4096];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, n, dst);
        }
        fclose(src);
    }
    fclose(dst);

    // Mark write session as active
    fs->has_active_write = 1;
    strncpy(fs->write_user, request->username, 63);
    fs->write_user[63] = '\0';
    fs->write_sentence = request->sentence_num;

    pthread_rwlock_unlock(&fs->file_lock);

    // Now load the TEMP file into a separate structure for modification
    FileStructure *temp_fs = fs_create(fs->temp_filename, request->username);
    if (!temp_fs) {
        response.status = -1;
        snprintf(response.error_msg, 512, "Failed to create temp structure");
        ss_send_to_client(client_fd, &response);
        unlink(temp_path);
        return -1;
    }

    fs_load_from_disk(temp_fs, manager->base_path);

    // Lock the sentence in temp structure
    int lock_result = fs_lock_sentence(temp_fs, request->sentence_num, request->username);
    if (lock_result != 0) {
        fs_destroy(temp_fs);
        unlink(temp_path);
        
        response.status = lock_result;
        snprintf(response.error_msg, sizeof(response.error_msg), 
                "ERROR: Failed to lock sentence %d", request->sentence_num);
        ss_send_to_client(client_fd, &response);
        
        pthread_rwlock_wrlock(&fs->file_lock);
        fs->has_active_write = 0;
        pthread_rwlock_unlock(&fs->file_lock);
        
        return -1;
    }

    // Store temp_fs in the write session
    WriteSession *session = ss_create_write_session(manager, client_fd, 
                                                     request->filename, 
                                                     request->username, 
                                                     request->sentence_num);
    if (!session) {
        fs_destroy(temp_fs);
        unlink(temp_path);
        response.status = -1;
        snprintf(response.error_msg, 512, "Failed to create write session");
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    // Store temp_fs pointer in session (you'll need to add this field)
    session->temp_fs = temp_fs;

    response.status = 0;
    snprintf(response.error_msg, 512, "Write session started");
    ss_send_to_client(client_fd, &response);

    printf("[SS] WRITE_BEGIN: Sentence %d locked for %s in file %s (temp mode)\n", 
           request->sentence_num, request->username, request->filename);
    return 0;
}

// int ss_handle_write_update(int client_fd, ClientRequest *request, ClientManager *manager) {
//     ClientRequest response;
    
//     memset(&response, 0, sizeof(ClientRequest));
//     response.op_type = OP_ACK;
//     strncpy(response.filename, request->filename, MAX_FILENAME - 1);

//     // Find active write session
//     WriteSession *session = ss_find_write_session(manager, client_fd);
//     if (!session) {
//         response.status = -1;
//         snprintf(response.error_msg, 512, "No active write session");
//         ss_send_to_client(client_fd, &response);
//         return -1;
//     }

//     // Get file structure
//     FileStructure *fs = fm_get_file(manager->file_manager, session->filename);
//     if (!fs) {
//         response.status = -1;
//         snprintf(response.error_msg, 512, "File not found");
//         ss_send_to_client(client_fd, &response);
//         return -1;
//     }

//     // Perform write operation
//     int result = fs_write_word(fs, session->sentence_num, request->word_index, 
//                                request->content, session->username);
    
//     if (result == -4) {
//         // Word index out of range
//         response.status = -4;
        
//         // Get current word count for better error message
//         pthread_rwlock_rdlock(&fs->file_lock);
//         SentenceNode *sentence = fs->sentences;
//         int count = 0;
//         while (sentence && count < session->sentence_num) {
//             sentence = sentence->next;
//             count++;
//         }
//         int word_count = sentence ? sentence->word_count : 0;
//         pthread_rwlock_unlock(&fs->file_lock);
        
//         snprintf(response.error_msg, sizeof(response.error_msg), 
//                 "ERROR: Word index %d out of range (sentence %d has %d words, valid range is 0-%d)",
//                 request->word_index, session->sentence_num, word_count, word_count);
//     } else if (result == -5) {
//         // Sentence index out of range (shouldn't happen in UPDATE, but handle it)
//         response.status = -5;
//         snprintf(response.error_msg, sizeof(response.error_msg), 
//                 "ERROR: Sentence index out of range");
//     } else if (result != 0) {
//         // Other errors
//         response.status = -1;
//         snprintf(response.error_msg, 512, "Write operation failed (error code: %d)", result);
//     } else {
//         response.status = 0;
//         snprintf(response.error_msg, 512, "Write successful");
//     }

//     ss_send_to_client(client_fd, &response);
    
//     if (result == 0) {
//         printf("[SS] WRITE_UPDATE: Updated sentence %d in file %s\n", 
//                session->sentence_num, session->filename);
//     }
    
//     return result;
// }

// int ss_handle_write_update(int client_fd, ClientRequest *request, ClientManager *manager) {
//     ClientRequest response;
    
//     memset(&response, 0, sizeof(ClientRequest));
//     response.op_type = OP_ACK;
//     strncpy(response.filename, request->filename, MAX_FILENAME - 1);

//     // Find active write session
//     WriteSession *session = ss_find_write_session(manager, client_fd);
//     if (!session) {
//         response.status = -1;
//         snprintf(response.error_msg, 512, "No active write session");
//         ss_send_to_client(client_fd, &response);
//         return -1;
//     }

//     // Get file structure
//     FileStructure *fs = fm_get_file(manager->file_manager, session->filename);
//     if (!fs) {
//         response.status = -1;
//         snprintf(response.error_msg, 512, "File not found");
//         ss_send_to_client(client_fd, &response);
//         ss_destroy_write_session(manager, client_fd);  // Clean up on error
//         return -1;
//     }

//     // Perform write operation
//     int result = fs_write_word(fs, session->sentence_num, request->word_index, 
//                                request->content, session->username);
    
//     if (result == -4) {
//         // Word index out of range
//         response.status = -4;
        
//         // Get current word count for better error message
//         pthread_rwlock_rdlock(&fs->file_lock);
//         SentenceNode *sentence = fs->sentences;
//         int count = 0;
//         while (sentence && count < session->sentence_num) {
//             sentence = sentence->next;
//             count++;
//         }
//         int word_count = sentence ? sentence->word_count : 0;
//         pthread_rwlock_unlock(&fs->file_lock);
        
//         snprintf(response.error_msg, sizeof(response.error_msg), 
//                 "ERROR: Word index %d out of range (sentence %d has %d words, valid range is 0-%d)",
//                 request->word_index, session->sentence_num, word_count, word_count);
        
//         ss_send_to_client(client_fd, &response);
        
//         // Clean up the session and unlock sentence on error
//         ss_destroy_write_session(manager, client_fd);
//         return -1;
        
//     } else if (result == -5) {
//         // Sentence index out of range (shouldn't happen in UPDATE, but handle it)
//         response.status = -5;
//         snprintf(response.error_msg, sizeof(response.error_msg), 
//                 "ERROR: Sentence index out of range");
        
//         ss_send_to_client(client_fd, &response);
        
//         // Clean up the session and unlock sentence on error
//         ss_destroy_write_session(manager, client_fd);
//         return -1;
        
//     } else if (result != 0) {
//         // Other errors
//         response.status = -1;
//         snprintf(response.error_msg, 512, "Write operation failed (error code: %d)", result);
        
//         ss_send_to_client(client_fd, &response);
        
//         // Clean up the session and unlock sentence on error
//         ss_destroy_write_session(manager, client_fd);
//         return -1;
//     }

//     // Success case
//     response.status = 0;
//     snprintf(response.error_msg, 512, "Write successful");
//     ss_send_to_client(client_fd, &response);
    
//     printf("[SS] WRITE_UPDATE: Updated sentence %d in file %s\n", 
//            session->sentence_num, session->filename);
    
//     return 0;
// }

int ss_handle_write_update(int client_fd, ClientRequest *request, ClientManager *manager) {
    ClientRequest response;
    
    memset(&response, 0, sizeof(ClientRequest));
    response.op_type = OP_ACK;
    strncpy(response.filename, request->filename, MAX_FILENAME - 1);

    WriteSession *session = ss_find_write_session(manager, client_fd);
    if (!session || !session->temp_fs) {
        response.status = -1;
        snprintf(response.error_msg, 512, "No active write session");
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    // Perform write on TEMP file structure
    int result = fs_write_word(session->temp_fs, session->sentence_num, 
                               request->word_index, request->content, 
                               session->username);
    
    if (result == -4) {
        response.status = -4;
        
        pthread_rwlock_rdlock(&session->temp_fs->file_lock);
        SentenceNode *sentence = find_sentence(session->temp_fs, session->sentence_num);
        int word_count = sentence ? sentence->word_count : 0;
        pthread_rwlock_unlock(&session->temp_fs->file_lock);
        
        snprintf(response.error_msg, sizeof(response.error_msg), 
                "ERROR: Word index %d out of range (sentence %d has %d words, valid range is 0-%d)",
                request->word_index, session->sentence_num, word_count, word_count);
        
        ss_send_to_client(client_fd, &response);
        ss_destroy_write_session(manager, client_fd);
        return -1;
    } else if (result != 0) {
        response.status = -1;
        snprintf(response.error_msg, 512, "Write operation failed (error code: %d)", result);
        ss_send_to_client(client_fd, &response);
        ss_destroy_write_session(manager, client_fd);
        return -1;
    }

    // Write temp changes to temp disk file after each update
    fs_write_to_disk(session->temp_fs, manager->base_path);

    response.status = 0;
    snprintf(response.error_msg, 512, "Write successful");
    ss_send_to_client(client_fd, &response);
    
    printf("[SS] WRITE_UPDATE: Updated sentence %d in temp file\n", session->sentence_num);
    return 0;
}

// int ss_handle_write_end(int client_fd, ClientRequest *request, ClientManager *manager) {
//     ClientRequest response;
    
//     memset(&response, 0, sizeof(ClientRequest));
//     response.op_type = OP_ACK;
//     strncpy(response.filename, request->filename, MAX_FILENAME - 1);

//     // Find active write session
//     WriteSession *session = ss_find_write_session(manager, client_fd);
//     if (!session) {
//         response.status = -1;
//         snprintf(response.error_msg, 512, "No active write session");
//         ss_send_to_client(client_fd, &response);
//         return -1;
//     }

//     // Get file structure
//     FileStructure *fs = fm_get_file(manager->file_manager, session->filename);
//     if (!fs) {
//         response.status = -1;
//         snprintf(response.error_msg, 512, "File not found");
//         ss_send_to_client(client_fd, &response);
//         ss_destroy_write_session(manager, client_fd);
//         return -1;
//     }

//     // Unlock sentence
//     fs_unlock_sentence(fs, session->sentence_num);

//     // Commit changes to disk
//     int result = fs_commit_write(fs, manager->base_path);
    
//     if (result != 0) {
//         response.status = -1;
//         snprintf(response.error_msg, 512, "Failed to write to disk");
//     } else {
//         response.status = 0;
//         snprintf(response.error_msg, 512, "Write completed successfully");
//     }

//     ss_send_to_client(client_fd, &response);
//     ss_destroy_write_session(manager, client_fd);

//     printf("[SS] WRITE_END: Completed write for file %s\n", session->filename);
//     return 0;
// }

int ss_handle_write_end(int client_fd, ClientRequest *request, ClientManager *manager) {
    ClientRequest response;
    
    memset(&response, 0, sizeof(ClientRequest));
    response.op_type = OP_ACK;
    strncpy(response.filename, request->filename, MAX_FILENAME - 1);

    WriteSession *session = ss_find_write_session(manager, client_fd);
    if (!session || !session->temp_fs) {
        response.status = -1;
        snprintf(response.error_msg, 512, "No active write session");
        ss_send_to_client(client_fd, &response);
        return -1;
    }

    // Get main file structure
    FileStructure *fs = fm_get_file(manager->file_manager, session->filename);
    if (!fs) {
        response.status = -1;
        snprintf(response.error_msg, 512, "File not found");
        ss_send_to_client(client_fd, &response);
        ss_destroy_write_session(manager, client_fd);
        return -1;
    }

    // Final write of temp file
    fs_write_to_disk(session->temp_fs, manager->base_path);

    // ATOMIC RENAME: Move temp file to main file
    char main_path[MAX_FILENAME * 2];
    char temp_path[MAX_FILENAME * 2];
    snprintf(main_path, sizeof(main_path), "%s/%s", manager->base_path, fs->filename);
    snprintf(temp_path, sizeof(temp_path), "%s/%s", manager->base_path, fs->temp_filename);

    // Atomic rename (this is the magic - either succeeds completely or not at all)
    if (rename(temp_path, main_path) != 0) {
        response.status = -1;
        snprintf(response.error_msg, 512, "Failed to commit changes: %s", strerror(errno));
        ss_send_to_client(client_fd, &response);
        ss_destroy_write_session(manager, client_fd);
        return -1;
    }

    // Reload main file structure from disk
    pthread_rwlock_wrlock(&fs->file_lock);
    
    // Destroy old sentences
    SentenceNode *current = fs->sentences;
    while (current) {
        SentenceNode *next = current->next;
        sentence_destroy(current);
        current = next;
    }
    fs->sentences = NULL;
    fs->sentence_count = 0;
    
    // Reload from disk (now has new content)
    pthread_rwlock_unlock(&fs->file_lock);
    fs_load_from_disk(fs, manager->base_path);
    
    // Clear write lock
    pthread_rwlock_wrlock(&fs->file_lock);
    fs->has_active_write = 0;
    fs->write_user[0] = '\0';
    fs->temp_filename[0] = '\0';
    fs->last_modified = time(NULL);
    pthread_rwlock_unlock(&fs->file_lock);

    response.status = 0;
    snprintf(response.error_msg, 512, "Write completed successfully");
    ss_send_to_client(client_fd, &response);
    
    ss_destroy_write_session(manager, client_fd);

    printf("[SS] WRITE_END: Committed write for file %s\n", session->filename);
    return 0;
}
