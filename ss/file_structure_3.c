// ==================== Checkpoint Operations ====================

#include "file_structure.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>

/**
 * Initialize checkpoint list
 */
int checkpoint_init(CheckpointList *list) {
    if (!list) {
        return -1;
    }
    
    list->head = NULL;
    list->count = 0;
    
    if (pthread_mutex_init(&list->lock, NULL) != 0) {
        return -1;
    }
    
    return 0;
}

/**
 * Cleanup all checkpoints
 */
void checkpoint_cleanup(CheckpointList *list) {
    if (!list) {
        return;
    }
    
    pthread_mutex_lock(&list->lock);
    
    CheckpointNode *current = list->head;
    while (current) {
        CheckpointNode *next = current->next;
        
        // Free all sentences in this checkpoint
        SentenceNode *sentence = current->sentences;
        while (sentence) {
            SentenceNode *next_sentence = sentence->next;
            sentence_destroy(sentence);
            sentence = next_sentence;
        }
        
        free(current);
        current = next;
    }
    
    list->head = NULL;
    list->count = 0;
    
    pthread_mutex_unlock(&list->lock);
    pthread_mutex_destroy(&list->lock);
}

/**
 * Find a checkpoint by tag
 */
CheckpointNode* checkpoint_find(CheckpointList *list, const char *tag) {
    if (!list || !tag) {
        return NULL;
    }
    
    CheckpointNode *current = list->head;
    while (current) {
        if (strcmp(current->tag, tag) == 0) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

/**
 * Create a checkpoint with the given tag
 */
int checkpoint_create(FileStructure *fs, const char *tag) {
    if (!fs || !tag) {
        return -1;
    }
    
    pthread_mutex_lock(&fs->checkpoints.lock);
    
    // Check if checkpoint with this tag already exists
    if (checkpoint_find(&fs->checkpoints, tag)) {
        pthread_mutex_unlock(&fs->checkpoints.lock);
        fprintf(stderr, "[CHECKPOINT] Tag '%s' already exists\n", tag);
        return -2; // Tag already exists
    }
    
    // Create new checkpoint node
    CheckpointNode *checkpoint = (CheckpointNode *)malloc(sizeof(CheckpointNode));
    if (!checkpoint) {
        pthread_mutex_unlock(&fs->checkpoints.lock);
        return -1;
    }
    
    strncpy(checkpoint->tag, tag, sizeof(checkpoint->tag) - 1);
    checkpoint->tag[sizeof(checkpoint->tag) - 1] = '\0';
    checkpoint->timestamp = time(NULL);
    checkpoint->next = NULL;
    
    // Deep copy sentences (need read lock on file)
    pthread_rwlock_rdlock(&fs->file_lock);
    checkpoint->sentences = fs_deep_copy_sentences(fs->sentences);
    pthread_rwlock_unlock(&fs->file_lock);
    
    if (!checkpoint->sentences && fs->sentences != NULL) {
        pthread_mutex_unlock(&fs->checkpoints.lock);
        free(checkpoint);
        return -1;
    }
    
    // Add to list
    checkpoint->next = fs->checkpoints.head;
    fs->checkpoints.head = checkpoint;
    fs->checkpoints.count++;
    
    pthread_mutex_unlock(&fs->checkpoints.lock);
    
    printf("[CHECKPOINT] Created checkpoint '%s' for file '%s'\n", tag, fs->filename);
    return 0;
}

/**
 * View the content of a checkpoint
 */
char* checkpoint_view(FileStructure *fs, const char *tag, char *buffer, size_t buffer_size) {
    if (!fs || !tag || !buffer || buffer_size == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&fs->checkpoints.lock);
    
    CheckpointNode *checkpoint = checkpoint_find(&fs->checkpoints, tag);
    if (!checkpoint) {
        pthread_mutex_unlock(&fs->checkpoints.lock);
        return NULL;
    }
    
    // Build content string from checkpoint
    buffer[0] = '\0';
    size_t offset = 0;
    char sentence_buffer[4096];
    
    SentenceNode *current = checkpoint->sentences;
    while (current && offset < buffer_size - 1) {
        sentence_to_string(current, sentence_buffer, sizeof(sentence_buffer));
        
        int written = snprintf(buffer + offset, buffer_size - offset, "%s", sentence_buffer);
        if (written < 0 || written >= (int)(buffer_size - offset)) {
            break;
        }
        offset += written;
        current = current->next;
    }
    
    pthread_mutex_unlock(&fs->checkpoints.lock);
    
    return buffer;
}

/**
 * Revert file to a checkpoint
 */
int checkpoint_revert(FileStructure *fs, const char *tag, const char *base_path) {
    if (!fs || !tag || !base_path) {
        return -1;
    }
    
    pthread_mutex_lock(&fs->checkpoints.lock);
    
    CheckpointNode *checkpoint = checkpoint_find(&fs->checkpoints, tag);
    if (!checkpoint) {
        pthread_mutex_unlock(&fs->checkpoints.lock);
        fprintf(stderr, "[CHECKPOINT] Tag '%s' not found\n", tag);
        return -2; // Checkpoint not found
    }
    
    // Deep copy checkpoint sentences
    SentenceNode *restored_sentences = fs_deep_copy_sentences(checkpoint->sentences);
    
    pthread_mutex_unlock(&fs->checkpoints.lock);
    
    if (!restored_sentences && checkpoint->sentences != NULL) {
        return -1;
    }
    
    // Write lock to replace file content
    pthread_rwlock_wrlock(&fs->file_lock);
    
    // Destroy current sentences
    SentenceNode *current = fs->sentences;
    while (current) {
        SentenceNode *next = current->next;
        sentence_destroy(current);
        current = next;
    }
    
    // Replace with checkpoint content
    fs->sentences = restored_sentences;
    
    // Recalculate sentence count
    fs->sentence_count = 0;
    current = fs->sentences;
    while (current) {
        fs->sentence_count++;
        current = current->next;
    }
    
    // Write to disk
    fs_write_to_disk(fs, base_path);
    fs->last_modified = time(NULL);
    
    pthread_rwlock_unlock(&fs->file_lock);
    
    printf("[CHECKPOINT] Reverted file '%s' to checkpoint '%s'\n", fs->filename, tag);
    return 0;
}

/**
 * List all checkpoints for a file
 */
char* checkpoint_list(FileStructure *fs, char *buffer, size_t buffer_size) {
    if (!fs || !buffer || buffer_size == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&fs->checkpoints.lock);
    
    if (fs->checkpoints.count == 0) {
        pthread_mutex_unlock(&fs->checkpoints.lock);
        snprintf(buffer, buffer_size, "No checkpoints available");
        return buffer;
    }
    
    buffer[0] = '\0';
    size_t offset = 0;
    
    offset += snprintf(buffer + offset, buffer_size - offset,
                      "Checkpoints for '%s' (%d total):\n", 
                      fs->filename, fs->checkpoints.count);
    
    CheckpointNode *current = fs->checkpoints.head;
    int index = 1;
    
    while (current && offset < buffer_size - 100) {
        char time_str[64];
        struct tm *tm_info = localtime(&current->timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        offset += snprintf(buffer + offset, buffer_size - offset,
                          "%d. Tag: %s | Created: %s\n",
                          index++, current->tag, time_str);
        
        current = current->next;
    }
    
    pthread_mutex_unlock(&fs->checkpoints.lock);
    
    return buffer;
}

/**
 * Save checkpoints to disk (metadata only, content is already deep-copied)
 */
int checkpoint_save_to_disk(FileStructure *fs, const char *base_path) {
    if (!fs || !base_path) {
        return -1;
    }
    
    char checkpoint_dir[MAX_FILENAME * 2];
    snprintf(checkpoint_dir, sizeof(checkpoint_dir), "%s/.checkpoints", base_path);
    
    // Create checkpoint directory if it doesn't exist
    mkdir(checkpoint_dir, 0755);
    
    char metadata_file[MAX_FILENAME * 3];
    snprintf(metadata_file, sizeof(metadata_file), 
             "%s/%s.checkpoint_meta", checkpoint_dir, fs->filename);
    
    pthread_mutex_lock(&fs->checkpoints.lock);
    
    FILE *fp = fopen(metadata_file, "w");
    if (!fp) {
        pthread_mutex_unlock(&fs->checkpoints.lock);
        fprintf(stderr, "[CHECKPOINT] Failed to save metadata: %s\n", strerror(errno));
        return -1;
    }
    
    fprintf(fp, "%d\n", fs->checkpoints.count);
    
    CheckpointNode *current = fs->checkpoints.head;
    while (current) {
        fprintf(fp, "%s|%ld\n", current->tag, (long)current->timestamp);
        
        // Save checkpoint content to separate file
        char checkpoint_file[MAX_FILENAME * 3];
        snprintf(checkpoint_file, sizeof(checkpoint_file),
                "%s/%s.%s", checkpoint_dir, fs->filename, current->tag);
        
        FILE *cp_fp = fopen(checkpoint_file, "w");
        if (cp_fp) {
            char sentence_buffer[4096];
            SentenceNode *sentence = current->sentences;
            while (sentence) {
                sentence_to_string(sentence, sentence_buffer, sizeof(sentence_buffer));
                fprintf(cp_fp, "%s", sentence_buffer);
                sentence = sentence->next;
            }
            fclose(cp_fp);
        }
        
        current = current->next;
    }
    
    fclose(fp);
    pthread_mutex_unlock(&fs->checkpoints.lock);
    
    printf("[CHECKPOINT] Saved %d checkpoints for '%s'\n", 
           fs->checkpoints.count, fs->filename);
    return 0;
}

/**
 * Load checkpoints from disk
 */
int checkpoint_load_from_disk(FileStructure *fs, const char *base_path) {
    if (!fs || !base_path) {
        return -1;
    }
    
    char checkpoint_dir[MAX_FILENAME * 2];
    snprintf(checkpoint_dir, sizeof(checkpoint_dir), "%s/.checkpoints", base_path);
    
    char metadata_file[MAX_FILENAME * 3];
    snprintf(metadata_file, sizeof(metadata_file), 
             "%s/%s.checkpoint_meta", checkpoint_dir, fs->filename);
    
    FILE *fp = fopen(metadata_file, "r");
    if (!fp) {
        // No checkpoints exist yet, that's okay
        return 0;
    }
    
    int count;
    if (fscanf(fp, "%d\n", &count) != 1) {
        fclose(fp);
        return -1;
    }
    
    pthread_mutex_lock(&fs->checkpoints.lock);
    
    for (int i = 0; i < count; i++) {
        char tag[64];
        long timestamp;
        
        if (fscanf(fp, "%63[^|]|%ld\n", tag, &timestamp) != 2) {
            break;
        }
        
        if (checkpoint_find(&fs->checkpoints, tag) != NULL) {
            printf("[CHECKPOINT] Skipping duplicate tag '%s' during load\n", tag);
            continue;
        }

        // Load checkpoint content
        char checkpoint_file[MAX_FILENAME * 3];
        snprintf(checkpoint_file, sizeof(checkpoint_file),
                "%s/%s.%s", checkpoint_dir, fs->filename, tag);
        
        FILE *cp_fp = fopen(checkpoint_file, "r");
        if (!cp_fp) {
            continue;
        }
        
        // Read content
        fseek(cp_fp, 0, SEEK_END);
        long file_size = ftell(cp_fp);
        fseek(cp_fp, 0, SEEK_SET);
        
        if (file_size > 0) {
            char *content = (char *)malloc(file_size + 1);
            if (content) {
                size_t read_size = fread(content, 1, file_size, cp_fp);
                content[read_size] = '\0';
                
                // Create checkpoint node
                CheckpointNode *checkpoint = (CheckpointNode *)malloc(sizeof(CheckpointNode));
                if (checkpoint) {
                    strncpy(checkpoint->tag, tag, sizeof(checkpoint->tag) - 1);
                    checkpoint->tag[sizeof(checkpoint->tag) - 1] = '\0';
                    checkpoint->timestamp = (time_t)timestamp;
                    checkpoint->sentences = NULL;
                    checkpoint->next = fs->checkpoints.head;
                    
                    // Parse content into sentences
                    FileStructure temp_fs;
                    memset(&temp_fs, 0, sizeof(FileStructure));
                    fs_parse_content(&temp_fs, content);
                    checkpoint->sentences = temp_fs.sentences;
                    
                    fs->checkpoints.head = checkpoint;
                    fs->checkpoints.count++;
                }
                
                free(content);
            }
        }
        
        fclose(cp_fp);
    }
    
    fclose(fp);
    pthread_mutex_unlock(&fs->checkpoints.lock);
    
    printf("[CHECKPOINT] Loaded %d checkpoints for '%s'\n", 
           fs->checkpoints.count, fs->filename);
    return 0;
}