#include "api_c_ss/client_ss_connection.h"
#include "file_structure.h"
#include <stdio.h>
#include <string.h>

/**
 * Handle CHECKPOINT command - create a checkpoint
 */
int ss_handle_checkpoint(int client_fd, ClientRequest *request, ClientManager *manager) {
    printf("[SS-Checkpoint] Creating checkpoint '%s' for file '%s'\n",
           request->content, request->filename);
    
    // Get file structure
    FileStructure *fs = fm_get_file(manager->file_manager, request->filename);
    if (!fs) {
        ClientRequest response;
        memset(&response, 0, sizeof(ClientRequest));
        response.op_type = OP_ERROR;
        response.status = -1;
        snprintf(response.error_msg, sizeof(response.error_msg),
                "ERROR: File not found");
        return ss_send_to_client(client_fd, &response);
    }
    
    // Create checkpoint
    int result = checkpoint_create(fs, request->content);
    
    ClientRequest response;
    memset(&response, 0, sizeof(ClientRequest));
    
        if (result == 0) {
        // ONLY save to disk on success - CHANGED THIS
        if (checkpoint_save_to_disk(fs, manager->base_path) == 0) {
            response.op_type = OP_ACK;
            response.status = 0;
            snprintf(response.error_msg, sizeof(response.error_msg),
                    "SUCCESS: Checkpoint '%s' created", request->content);
        } else {
            // Failed to save - rollback the in-memory checkpoint
            pthread_mutex_lock(&fs->checkpoints.lock);
            CheckpointNode *head = fs->checkpoints.head;
            if (head && strcmp(head->tag, request->content) == 0) {
                fs->checkpoints.head = head->next;
                fs->checkpoints.count--;
                
                // Free the checkpoint
                SentenceNode *sentence = head->sentences;
                while (sentence) {
                    SentenceNode *next = sentence->next;
                    sentence_destroy(sentence);
                    sentence = next;
                }
                free(head);
            }
            pthread_mutex_unlock(&fs->checkpoints.lock);
            
            response.op_type = OP_ERROR;
            response.status = -1;
            snprintf(response.error_msg, sizeof(response.error_msg),
                    "ERROR: Failed to save checkpoint to disk");
        }
    } else if (result == -2) {
        response.op_type = OP_ERROR;
        response.status = -2;
        snprintf(response.error_msg, sizeof(response.error_msg),
                "ERROR: Checkpoint tag '%s' already exists", request->content);
    } else {
        response.op_type = OP_ERROR;
        response.status = -1;
        snprintf(response.error_msg, sizeof(response.error_msg),
                "ERROR: Failed to create checkpoint");
    }
    
    return ss_send_to_client(client_fd, &response);
}

/**
 * Handle VIEWCHECKPOINT command - view checkpoint content
 */
int ss_handle_viewcheckpoint(int client_fd, ClientRequest *request, ClientManager *manager) {
    printf("[SS-Checkpoint] Viewing checkpoint '%s' for file '%s'\n",
           request->content, request->filename);
    
    FileStructure *fs = fm_get_file(manager->file_manager, request->filename);
    if (!fs) {
        ClientRequest response;
        memset(&response, 0, sizeof(ClientRequest));
        response.op_type = OP_ERROR;
        response.status = -1;
        snprintf(response.error_msg, sizeof(response.error_msg),
                "ERROR: File not found");
        return ss_send_to_client(client_fd, &response);
    }
    
    char content_buffer[MAX_BUFFER_SIZE];
    char *content = checkpoint_view(fs, request->content, content_buffer, sizeof(content_buffer));
    
    ClientRequest response;
    memset(&response, 0, sizeof(ClientRequest));
    
    if (content) {
        response.op_type = OP_ACK;
        response.status = 0;
        strncpy(response.content, content, sizeof(response.content) - 1);
        response.content[sizeof(response.content) - 1] = '\0';
    } else {
        response.op_type = OP_ERROR;
        response.status = -1;
        snprintf(response.error_msg, sizeof(response.error_msg),
                "ERROR: Checkpoint '%s' not found", request->content);
    }
    
    return ss_send_to_client(client_fd, &response);
}

/**
 * Handle REVERT command - revert to checkpoint
 */
int ss_handle_revert(int client_fd, ClientRequest *request, ClientManager *manager) {
    printf("[SS-Checkpoint] Reverting file '%s' to checkpoint '%s'\n",
           request->filename, request->content);
    
    FileStructure *fs = fm_get_file(manager->file_manager, request->filename);
    if (!fs) {
        ClientRequest response;
        memset(&response, 0, sizeof(ClientRequest));
        response.op_type = OP_ERROR;
        response.status = -1;
        snprintf(response.error_msg, sizeof(response.error_msg),
                "ERROR: File not found");
        return ss_send_to_client(client_fd, &response);
    }
    
    int result = checkpoint_revert(fs, request->content, manager->base_path);
    
    ClientRequest response;
    memset(&response, 0, sizeof(ClientRequest));
    
    if (result == 0) {
        response.op_type = OP_ACK;
        response.status = 0;
        snprintf(response.error_msg, sizeof(response.error_msg),
                "SUCCESS: File reverted to checkpoint '%s'", request->content);
    } else if (result == -2) {
        response.op_type = OP_ERROR;
        response.status = -2;
        snprintf(response.error_msg, sizeof(response.error_msg),
                "ERROR: Checkpoint '%s' not found", request->content);
    } else {
        response.op_type = OP_ERROR;
        response.status = -1;
        snprintf(response.error_msg, sizeof(response.error_msg),
                "ERROR: Failed to revert to checkpoint");
    }
    
    return ss_send_to_client(client_fd, &response);
}

/**
 * Handle LISTCHECKPOINTS command - list all checkpoints
 */
int ss_handle_listcheckpoints(int client_fd, ClientRequest *request, ClientManager *manager) {
    printf("[SS-Checkpoint] Listing checkpoints for file '%s'\n", request->filename);
    
    FileStructure *fs = fm_get_file(manager->file_manager, request->filename);
    if (!fs) {
        ClientRequest response;
        memset(&response, 0, sizeof(ClientRequest));
        response.op_type = OP_ERROR;
        response.status = -1;
        snprintf(response.error_msg, sizeof(response.error_msg),
                "ERROR: File not found");
        return ss_send_to_client(client_fd, &response);
    }
    
    char list_buffer[MAX_BUFFER_SIZE];
    checkpoint_list(fs, list_buffer, sizeof(list_buffer));
    
    ClientRequest response;
    memset(&response, 0, sizeof(ClientRequest));
    response.op_type = OP_ACK;
    response.status = 0;
    strncpy(response.content, list_buffer, sizeof(response.content) - 1);
    response.content[sizeof(response.content) - 1] = '\0';
    
    return ss_send_to_client(client_fd, &response);
}