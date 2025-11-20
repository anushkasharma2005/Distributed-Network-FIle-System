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

    // // Create snapshot before writing (for UNDO)
    // if (fs_create_snapshot(fs, request->username) != 0) {
    //     fprintf(stderr, "[SS] Warning: Failed to create snapshot for %s\n", 
    //             request->filename);
    // }

    pthread_rwlock_wrlock(&fs->file_lock);

    // **NEW: Validate sentence boundaries based on delimiters**
    if (request->sentence_num > 0) {
        // For sentence N (N > 0), check if sentence N-1 exists AND has a delimiter
        SentenceNode *prev_sentence = find_sentence(fs, request->sentence_num - 1);
        
        if (!prev_sentence) {
            // Previous sentence doesn't exist - definitely out of range
            pthread_rwlock_unlock(&fs->file_lock);
            response.status = -5;
            snprintf(response.error_msg, sizeof(response.error_msg), 
                    "ERROR: Cannot write to sentence %d. Sentence %d does not exist. "
                    "File has %d sentence(s), valid range is 0-%d",
                    request->sentence_num, request->sentence_num - 1,
                    fs->sentence_count, fs->sentence_count);
            ss_send_to_client(client_fd, &response);
            return -1;
        }
        
        // Check if previous sentence has a delimiter
        pthread_rwlock_rdlock(&prev_sentence->lock);
        int has_delimiter = (prev_sentence->delimiters[0] != '\0');
        pthread_rwlock_unlock(&prev_sentence->lock);
        
        if (!has_delimiter) {
            // Previous sentence has no delimiter - cannot start new sentence
            pthread_rwlock_unlock(&fs->file_lock);
            response.status = -5;
            snprintf(response.error_msg, sizeof(response.error_msg), 
                    "ERROR: Cannot write to sentence %d. Sentence %d has no delimiter (., !, or ?). "
                    "A sentence must end with a delimiter before starting a new sentence. "
                    "Use WRITE %d to continue the current sentence.",
                    request->sentence_num, request->sentence_num - 1, request->sentence_num - 1);
            ss_send_to_client(client_fd, &response);
            return -1;
        }
    }

    // **Valid range check after delimiter validation**
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
    // snprintf(main_path, sizeof(main_path), "%s/%s", manager->base_path, fs->filename);
    // snprintf(temp_path, sizeof(temp_path), "%s/%s", manager->base_path, fs->temp_filename);
    
    if (strlen(fs->folder_path) == 0 || strcmp(fs->folder_path, "") == 0) {
        // Shouldn't happen after fs_create fix, but handle it
        snprintf(main_path, sizeof(main_path), "%s/root/%s", 
                 manager->base_path, fs->filename);
        snprintf(temp_path, sizeof(temp_path), "%s/root/%s", 
                 manager->base_path, fs->temp_filename);
    } else if (strcmp(fs->folder_path, "root") == 0) {
        snprintf(main_path, sizeof(main_path), "%s/root/%s", 
                 manager->base_path, fs->filename);
        snprintf(temp_path, sizeof(temp_path), "%s/root/%s", 
                 manager->base_path, fs->temp_filename);
    } else {
        // File in subfolder
        snprintf(main_path, sizeof(main_path), "%s/%s/%s", 
                 manager->base_path, fs->folder_path, fs->filename);
        snprintf(temp_path, sizeof(temp_path), "%s/%s/%s", 
                 manager->base_path, fs->folder_path, fs->temp_filename);
    }

    printf("[SS-WRITE] Main file path: %s\n", main_path);
    printf("[SS-WRITE] Temp file path: %s\n", temp_path);

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
    strcpy(temp_fs->folder_path, fs->folder_path);
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

    // Store temp_fs pointer in session
    session->temp_fs = temp_fs;

    response.status = 0;
    snprintf(response.error_msg, 512, "Write session started");
    ss_send_to_client(client_fd, &response);

    printf("[SS] WRITE_BEGIN: Sentence %d locked for %s in file %s (temp mode)\n", 
           request->sentence_num, request->username, request->filename);
    return 0;
}

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

//     WriteSession *session = ss_find_write_session(manager, client_fd);
//     if (!session || !session->temp_fs) {
//         response.status = -1;
//         snprintf(response.error_msg, 512, "No active write session");
//         ss_send_to_client(client_fd, &response);
//         return -1;
//     }

//     // Get main file structure
//     FileStructure *fs = fm_get_file(manager->file_manager, session->filename);
//     if (!fs) {
//         response.status = -1;
//         snprintf(response.error_msg, 512, "File not found");
//         ss_send_to_client(client_fd, &response);
//         ss_destroy_write_session(manager, client_fd);
//         return -1;
//     }

//     // CRITICAL FIX: Extract ALL modified sentences from temp file (not just one)
    
//     pthread_rwlock_rdlock(&session->temp_fs->file_lock);
    
//     // Count how many sentences exist in temp file starting from locked sentence
//     int temp_sentence_count = 0;
//     SentenceNode *temp_current = find_sentence(session->temp_fs, session->sentence_num);
//     while (temp_current) {
//         temp_sentence_count++;
//         temp_current = temp_current->next;
//     }
    
//     // Deep copy ALL sentences from locked sentence onwards
//     SentenceNode **sentence_copies = (SentenceNode **)calloc(temp_sentence_count, sizeof(SentenceNode *));
//     if (!sentence_copies) {
//         pthread_rwlock_unlock(&session->temp_fs->file_lock);
//         response.status = -1;
//         snprintf(response.error_msg, 512, "Memory allocation failed");
//         ss_send_to_client(client_fd, &response);
//         ss_destroy_write_session(manager, client_fd);
//         return -1;
//     }
    
//     temp_current = find_sentence(session->temp_fs, session->sentence_num);
//     for (int i = 0; i < temp_sentence_count && temp_current; i++) {
//         pthread_rwlock_rdlock(&temp_current->lock);
        
//         SentenceNode *sentence_copy = sentence_create('\0');
        
//         // Copy delimiters
//         strncpy(sentence_copy->delimiters, temp_current->delimiters, MAX_WHITESPACE - 1);
//         sentence_copy->delimiters[MAX_WHITESPACE - 1] = '\0';
//         strncpy(sentence_copy->whitespace_after_delimiters, 
//                temp_current->whitespace_after_delimiters, MAX_WHITESPACE - 1);
//         sentence_copy->whitespace_after_delimiters[MAX_WHITESPACE - 1] = '\0';

//         // Copy all words
//         WordNode *word = temp_current->words;
//         while (word) {
//             WordNode *new_word = word_create(word->word);
//             if (new_word) {
//                 strncpy(new_word->whitespace_after, word->whitespace_after, MAX_WHITESPACE - 1);
//                 new_word->whitespace_after[MAX_WHITESPACE - 1] = '\0';
                
//                 // Append to sentence
//                 if (!sentence_copy->words) {
//                     sentence_copy->words = new_word;
//                 } else {
//                     WordNode *tail = sentence_copy->words;
//                     while (tail->next) tail = tail->next;
//                     tail->next = new_word;
//                 }
//                 sentence_copy->word_count++;
//             }
//             word = word->next;
//         }
        
//         sentence_copies[i] = sentence_copy;
        
//         pthread_rwlock_unlock(&temp_current->lock);
//         temp_current = temp_current->next;
//     }
    
//     pthread_rwlock_unlock(&session->temp_fs->file_lock);

//     // Reload main file from disk (in case it was modified by another client)
//     pthread_rwlock_wrlock(&fs->file_lock);
    
//     // Destroy old in-memory structure
//     SentenceNode *old_sentence = fs->sentences;
//     while (old_sentence) {
//         SentenceNode *next = old_sentence->next;
//         sentence_destroy(old_sentence);
//         old_sentence = next;
//     }
//     fs->sentences = NULL;
//     fs->sentence_count = 0;
    
//     // Reload from disk
//     pthread_rwlock_unlock(&fs->file_lock);
//     fs_load_from_disk(fs, manager->base_path);
//     pthread_rwlock_wrlock(&fs->file_lock);

//     // Find insertion point in main file
//     SentenceNode *target_sentence = find_sentence(fs, session->sentence_num);
//     SentenceNode *prev_sentence = NULL;
    
//     if (session->sentence_num > 0) {
//         prev_sentence = find_sentence(fs, session->sentence_num - 1);
//     }
    
//     // Remove old sentences starting from locked sentence (they will be replaced)
//     if (target_sentence) {
//         // Find how many sentences to remove (same count as we're adding)
//         SentenceNode *to_remove = target_sentence;
//         for (int i = 0; i < temp_sentence_count && to_remove; i++) {
//             SentenceNode *next = to_remove->next;
            
//             // Unlink and destroy
//             if (prev_sentence) {
//                 prev_sentence->next = next;
//             } else {
//                 fs->sentences = next;
//             }
            
//             sentence_destroy(to_remove);
//             fs->sentence_count--;
            
//             to_remove = next;
//         }
//     }
    
//     // Insert all copied sentences at the locked position
//     for (int i = 0; i < temp_sentence_count; i++) {
//         if (!prev_sentence) {
//             // Insert at beginning
//             sentence_copies[i]->next = fs->sentences;
//             fs->sentences = sentence_copies[i];
//             prev_sentence = sentence_copies[i];
//         } else {
//             // Insert after prev_sentence
//             sentence_copies[i]->next = prev_sentence->next;
//             prev_sentence->next = sentence_copies[i];
//             prev_sentence = sentence_copies[i];
//         }
//         fs->sentence_count++;
//     }
    
//     free(sentence_copies);
    
//     // Write merged content back to disk
//     int write_result = fs_write_to_disk(fs, manager->base_path);
    
//     if (write_result != 0) {
//         pthread_rwlock_unlock(&fs->file_lock);
//         response.status = -1;
//         snprintf(response.error_msg, 512, "Failed to write merged content to disk");
//         ss_send_to_client(client_fd, &response);
//         ss_destroy_write_session(manager, client_fd);
//         return -1;
//     }

//     // Clear write lock
//     fs->has_active_write = 0;
//     fs->write_user[0] = '\0';
//     fs->temp_filename[0] = '\0';
//     fs->last_modified = time(NULL);
//     pthread_rwlock_unlock(&fs->file_lock);

//     response.status = 0;
//     snprintf(response.error_msg, 512, "Write completed successfully");
//     ss_send_to_client(client_fd, &response);
    
//     ss_destroy_write_session(manager, client_fd);

//     printf("[SS] WRITE_END: Committed write for file %s (%d sentences merged starting from sentence %d)\n", 
//            session->filename, temp_sentence_count, session->sentence_num);
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

    // **NEW: Create snapshot BEFORE merging changes**
    // This captures the state BEFORE this write operation
    
    // First, reload main file from disk to get current state
    pthread_rwlock_wrlock(&fs->file_lock);
    
    // Destroy old in-memory structure
    SentenceNode *old_sentence = fs->sentences;
    while (old_sentence) {
        SentenceNode *next = old_sentence->next;
        sentence_destroy(old_sentence);
        old_sentence = next;
    }
    fs->sentences = NULL;
    fs->sentence_count = 0;
    
    // Reload from disk
    pthread_rwlock_unlock(&fs->file_lock);
    fs_load_from_disk(fs, manager->base_path);
    
    // **CREATE SNAPSHOT NOW (before merge)**
    printf("[SS] Creating snapshot before applying write by %s\n", session->username);
    if (fs_create_snapshot(fs, session->username) != 0) {
        fprintf(stderr, "[SS] Warning: Failed to create snapshot for %s\n", 
                session->filename);
        // Don't fail the write, but warn
    }
    
    // Extract ALL modified sentences from temp file
    pthread_rwlock_rdlock(&session->temp_fs->file_lock);
    
    // Count how many sentences exist in temp file starting from locked sentence
    int temp_sentence_count = 0;
    SentenceNode *temp_current = find_sentence(session->temp_fs, session->sentence_num);
    while (temp_current) {
        temp_sentence_count++;
        temp_current = temp_current->next;
    }
    
    // Deep copy ALL sentences from locked sentence onwards
    SentenceNode **sentence_copies = (SentenceNode **)calloc(temp_sentence_count, sizeof(SentenceNode *));
    if (!sentence_copies) {
        pthread_rwlock_unlock(&session->temp_fs->file_lock);
        response.status = -1;
        snprintf(response.error_msg, 512, "Memory allocation failed");
        ss_send_to_client(client_fd, &response);
        ss_destroy_write_session(manager, client_fd);
        return -1;
    }
    
    temp_current = find_sentence(session->temp_fs, session->sentence_num);
    for (int i = 0; i < temp_sentence_count && temp_current; i++) {
        pthread_rwlock_rdlock(&temp_current->lock);
        
        SentenceNode *sentence_copy = sentence_create('\0');
        
        // Copy delimiters
        strncpy(sentence_copy->delimiters, temp_current->delimiters, MAX_WHITESPACE - 1);
        sentence_copy->delimiters[MAX_WHITESPACE - 1] = '\0';
        strncpy(sentence_copy->whitespace_after_delimiters, 
               temp_current->whitespace_after_delimiters, MAX_WHITESPACE - 1);
        sentence_copy->whitespace_after_delimiters[MAX_WHITESPACE - 1] = '\0';

        // Copy all words
        WordNode *word = temp_current->words;
        while (word) {
            WordNode *new_word = word_create(word->word);
            if (new_word) {
                strncpy(new_word->whitespace_after, word->whitespace_after, MAX_WHITESPACE - 1);
                new_word->whitespace_after[MAX_WHITESPACE - 1] = '\0';
                
                // Append to sentence
                if (!sentence_copy->words) {
                    sentence_copy->words = new_word;
                } else {
                    WordNode *tail = sentence_copy->words;
                    while (tail->next) tail = tail->next;
                    tail->next = new_word;
                }
                sentence_copy->word_count++;
            }
            word = word->next;
        }
        
        sentence_copies[i] = sentence_copy;
        
        pthread_rwlock_unlock(&temp_current->lock);
        temp_current = temp_current->next;
    }
    
    pthread_rwlock_unlock(&session->temp_fs->file_lock);

    // Now merge changes into main file (which was already reloaded above)
    pthread_rwlock_wrlock(&fs->file_lock);

    // Find insertion point in main file
    SentenceNode *target_sentence = find_sentence(fs, session->sentence_num);
    SentenceNode *prev_sentence = NULL;
    
    if (session->sentence_num > 0) {
        prev_sentence = find_sentence(fs, session->sentence_num - 1);
    }
    
    // Save sentences AFTER the locked sentence (we'll only remove what we're replacing)
    SentenceNode *sentences_after_target = NULL;
    
    if (target_sentence) {
        sentences_after_target = target_sentence->next;
        
        // Unlink and destroy ONLY the locked sentence
        if (prev_sentence) {
            prev_sentence->next = NULL;
        } else {
            fs->sentences = NULL;
        }
        
        target_sentence->next = NULL;
        sentence_destroy(target_sentence);
        fs->sentence_count--;
    }
    
    // Now remove additional sentences if we're replacing multiple
    SentenceNode *to_remove = sentences_after_target;
    for (int i = 1; i < temp_sentence_count && to_remove; i++) {
        SentenceNode *next_after_removal = to_remove->next;
        sentence_destroy(to_remove);
        fs->sentence_count--;
        to_remove = next_after_removal;
    }
    sentences_after_target = to_remove; // Update to point to sentences we're keeping
    
    // Insert all copied sentences at the locked position
    SentenceNode *insert_point = prev_sentence;
    
    for (int i = 0; i < temp_sentence_count; i++) {
        if (!insert_point) {
            // Insert at beginning
            sentence_copies[i]->next = fs->sentences;
            fs->sentences = sentence_copies[i];
            insert_point = sentence_copies[i];
        } else {
            // Insert after insert_point
            sentence_copies[i]->next = insert_point->next;
            insert_point->next = sentence_copies[i];
            insert_point = sentence_copies[i];
        }
        fs->sentence_count++;
    }
    
    // Reconnect sentences after
    if (insert_point) {
        insert_point->next = sentences_after_target;
    } else if (fs->sentences) {
        SentenceNode *tail = fs->sentences;
        while (tail->next) tail = tail->next;
        tail->next = sentences_after_target;
    } else {
        fs->sentences = sentences_after_target;
    }
    
    free(sentence_copies);
    
    // Write merged content back to disk
    int write_result = fs_write_to_disk(fs, manager->base_path);
    
    if (write_result != 0) {
        pthread_rwlock_unlock(&fs->file_lock);
        response.status = -1;
        snprintf(response.error_msg, 512, "Failed to write merged content to disk");
        ss_send_to_client(client_fd, &response);
        ss_destroy_write_session(manager, client_fd);
        return -1;
    }

    char temp_path[MAX_FILENAME * 2];
    
    if (strlen(fs->folder_path) == 0 || strcmp(fs->folder_path, "") == 0) {
        snprintf(temp_path, sizeof(temp_path), "%s/root/%s", 
                 manager->base_path, fs->temp_filename);
    } else if (strcmp(fs->folder_path, "root") == 0) {
        snprintf(temp_path, sizeof(temp_path), "%s/root/%s", 
                 manager->base_path, fs->temp_filename);
    } else {
        snprintf(temp_path, sizeof(temp_path), "%s/%s/%s", 
                 manager->base_path, fs->folder_path, fs->temp_filename);
    }

    printf("[SS-WRITE] Deleting temp file: %s\n", temp_path);
    if (unlink(temp_path) != 0) {
        fprintf(stderr, "[SS-WRITE] Warning: Failed to delete temp file %s: %s\n", 
                temp_path, strerror(errno));
    }

    // Clear write lock
    fs->has_active_write = 0;
    fs->write_user[0] = '\0';
    fs->temp_filename[0] = '\0';
    fs->last_modified = time(NULL);
    pthread_rwlock_unlock(&fs->file_lock);

    response.status = 0;
    snprintf(response.error_msg, 512, "Write completed successfully");
    ss_send_to_client(client_fd, &response);
    
    ss_destroy_write_session(manager, client_fd);

    printf("[SS] WRITE_END: Committed write for file %s (%d sentences merged starting from sentence %d)\n", 
           session->filename, temp_sentence_count, session->sentence_num);
    return 0;
}