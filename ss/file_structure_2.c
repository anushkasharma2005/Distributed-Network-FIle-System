#include "file_structure.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

char* fs_read_sentence(FileStructure *fs, int sentence_num, char *buffer, size_t buffer_size) {
    if (!fs || !buffer || buffer_size == 0 || sentence_num < 0) {
        return NULL;
    }

    pthread_rwlock_rdlock(&fs->file_lock);

    SentenceNode *current = fs->sentences;
    int count = 0;

    while (current && count < sentence_num) {
        current = current->next;
        count++;
    }

    if (!current) {
        pthread_rwlock_unlock(&fs->file_lock);
        return NULL;
    }

    sentence_to_string(current, buffer, buffer_size);
    pthread_rwlock_unlock(&fs->file_lock);

    return buffer;
}

int fs_get_sentence_count(FileStructure *fs) {
    if (!fs) {
        return 0;
    }
    
    pthread_rwlock_rdlock(&fs->file_lock);
    int count = fs->sentence_count;
    pthread_rwlock_unlock(&fs->file_lock);
    
    return count;
}

// ==================== Write Operations ====================

int fs_lock_sentence(FileStructure *fs, int sentence_num, const char *username) {
    if (!fs || !username || sentence_num < 0) {
        return -1;
    }

    pthread_rwlock_rdlock(&fs->file_lock);

    SentenceNode *current = fs->sentences;
    int count = 0;

    while (current && count < sentence_num) {
        current = current->next;
        count++;
    }

    if (!current) {
        pthread_rwlock_unlock(&fs->file_lock);
        fprintf(stderr, "[FS] Sentence %d not found in file %s\n", 
                sentence_num, fs->filename);
        return -1;
    }

    // Try to acquire write lock on sentence
    pthread_rwlock_wrlock(&current->lock);

    if (current->locked_for_write) {
        pthread_rwlock_unlock(&current->lock);
        pthread_rwlock_unlock(&fs->file_lock);
        fprintf(stderr, "[FS] Sentence %d is already locked by %s\n", 
                sentence_num, current->locked_by);
        return -2; // Already locked
    }

    current->locked_for_write = 1;
    strncpy(current->locked_by, username, 63);
    current->locked_by[63] = '\0';

    pthread_rwlock_unlock(&current->lock);
    pthread_rwlock_unlock(&fs->file_lock);

    printf("[FS] Sentence %d locked by %s in file %s\n", 
           sentence_num, username, fs->filename);
    return 0;
}

int fs_unlock_sentence(FileStructure *fs, int sentence_num) {
    if (!fs || sentence_num < 0) {
        return -1;
    }

    pthread_rwlock_rdlock(&fs->file_lock);

    SentenceNode *current = fs->sentences;
    int count = 0;

    while (current && count < sentence_num) {
        current = current->next;
        count++;
    }

    if (!current) {
        pthread_rwlock_unlock(&fs->file_lock);
        return -1;
    }

    pthread_rwlock_wrlock(&current->lock);
    current->locked_for_write = 0;
    current->locked_by[0] = '\0';
    pthread_rwlock_unlock(&current->lock);

    pthread_rwlock_unlock(&fs->file_lock);

    printf("[FS] Sentence %d unlocked in file %s\n", sentence_num, fs->filename);
    return 0;
}

int fs_write_word(FileStructure *fs, int sentence_num, int word_index, 
                  const char *content, const char *username) {
    if (!fs || !content || !username || sentence_num < 0 || word_index < 0) {
        return -1;
    }

    pthread_rwlock_rdlock(&fs->file_lock);

    // Find the sentence
    SentenceNode *current = fs->sentences;
    int count = 0;

    while (current && count < sentence_num) {
        current = current->next;
        count++;
    }

    if (!current) {
        pthread_rwlock_unlock(&fs->file_lock);
        fprintf(stderr, "[FS] Sentence %d not found\n", sentence_num);
        return -1;
    }

    // Check if sentence is locked by this user
    pthread_rwlock_rdlock(&current->lock);
    
    if (!current->locked_for_write) {
        pthread_rwlock_unlock(&current->lock);
        pthread_rwlock_unlock(&fs->file_lock);
        fprintf(stderr, "[FS] Sentence %d is not locked\n", sentence_num);
        return -2;
    }

    if (strcmp(current->locked_by, username) != 0) {
        pthread_rwlock_unlock(&current->lock);
        pthread_rwlock_unlock(&fs->file_lock);
        fprintf(stderr, "[FS] Sentence %d is locked by %s, not %s\n", 
                sentence_num, current->locked_by, username);
        return -3;
    }

    pthread_rwlock_unlock(&current->lock);

    // Now we need write access to modify the sentence
    pthread_rwlock_wrlock(&current->lock);

    // Parse content and handle delimiters
    char word_buffer[MAX_WORD_LENGTH];
    int word_idx = 0;
    int insert_position = word_index;
    SentenceNode *new_sentence = NULL;
    int new_sentences_created = 0;

    for (int i = 0; content[i] != '\0'; i++) {
        char ch = content[i];

        if (ch == '.' || ch == '!' || ch == '?') {
            // Add current word if exists
            if (word_idx > 0) {
                word_buffer[word_idx] = '\0';
                sentence_add_word(current, word_buffer, insert_position++);
                word_idx = 0;
            }

            // Update delimiter and create new sentence if needed
            if (new_sentence == NULL) {
                // First delimiter updates current sentence
                current->delimiter = ch;
                
                // Create new sentence for subsequent content
                if (content[i + 1] != '\0') {
                    new_sentence = sentence_create('.');
                    new_sentence->next = current->next;
                    current->next = new_sentence;
                    fs->sentence_count++;
                    new_sentences_created++;
                    insert_position = 0;
                    current = new_sentence;
                }
            } else {
                // Subsequent delimiters create new sentences
                current->delimiter = ch;
                if (content[i + 1] != '\0') {
                    new_sentence = sentence_create('.');
                    new_sentence->next = current->next;
                    current->next = new_sentence;
                    fs->sentence_count++;
                    new_sentences_created++;
                    insert_position = 0;
                    current = new_sentence;
                }
            }
        } else if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            // Word separator
            if (word_idx > 0) {
                word_buffer[word_idx] = '\0';
                sentence_add_word(current, word_buffer, insert_position++);
                word_idx = 0;
            }
        } else {
            // Regular character
            if (word_idx < MAX_WORD_LENGTH - 1) {
                word_buffer[word_idx++] = ch;
            }
        }
    }

    // Add trailing word
    if (word_idx > 0) {
        word_buffer[word_idx] = '\0';
        sentence_add_word(current, word_buffer, insert_position++);
    }

    pthread_rwlock_unlock(&current->lock);
    pthread_rwlock_unlock(&fs->file_lock);

    printf("[FS] Write completed: %d words added to sentence %d (%d new sentences created)\n",
           insert_position - word_index, sentence_num, new_sentences_created);
    return 0;
}

int fs_commit_write(FileStructure *fs, const char *base_path) {
    if (!fs || !base_path) {
        return -1;
    }

    // Write lock the entire file during commit
    pthread_rwlock_wrlock(&fs->file_lock);

    // Write to disk
    int result = fs_write_to_disk(fs, base_path);
    
    if (result == 0) {
        fs->last_modified = time(NULL);
    }

    pthread_rwlock_unlock(&fs->file_lock);

    return result;
}

// ==================== Undo Operations ====================

SentenceNode* fs_deep_copy_sentences(SentenceNode *original) {
    if (!original) {
        return NULL;
    }

    SentenceNode *new_head = NULL;
    SentenceNode *new_tail = NULL;

    SentenceNode *current = original;
    while (current) {
        SentenceNode *new_sentence = sentence_create(current->delimiter);
        if (!new_sentence) {
            // Cleanup on failure
            while (new_head) {
                SentenceNode *next = new_head->next;
                sentence_destroy(new_head);
                new_head = next;
            }
            return NULL;
        }

        // Copy words
        WordNode *word = current->words;
        while (word) {
            sentence_add_word(new_sentence, word->word, new_sentence->word_count);
            word = word->next;
        }

        // Link into new list
        if (!new_head) {
            new_head = new_sentence;
            new_tail = new_sentence;
        } else {
            new_tail->next = new_sentence;
            new_tail = new_sentence;
        }

        current = current->next;
    }

    return new_head;
}

int fs_create_snapshot(FileStructure *fs, const char *username) {
    if (!fs || !username) {
        return -1;
    }

    pthread_rwlock_wrlock(&fs->file_lock);

    // Delete old snapshot if exists
    if (fs->last_snapshot) {
        SentenceNode *current = fs->last_snapshot->sentences;
        while (current) {
            SentenceNode *next = current->next;
            sentence_destroy(current);
            current = next;
        }
        free(fs->last_snapshot);
        fs->last_snapshot = NULL;
    }

    // Create new snapshot
    FileSnapshot *snapshot = (FileSnapshot *)malloc(sizeof(FileSnapshot));
    if (!snapshot) {
        pthread_rwlock_unlock(&fs->file_lock);
        return -1;
    }

    snapshot->sentences = fs_deep_copy_sentences(fs->sentences);
    snapshot->timestamp = time(NULL);
    strncpy(snapshot->modified_by, username, 63);
    snapshot->modified_by[63] = '\0';
    snapshot->next = NULL;

    fs->last_snapshot = snapshot;

    pthread_rwlock_unlock(&fs->file_lock);

    printf("[FS] Snapshot created for file %s by %s\n", fs->filename, username);
    return 0;
}

int fs_undo(FileStructure *fs, const char *base_path) {
    if (!fs || !base_path) {
        return -1;
    }

    pthread_rwlock_wrlock(&fs->file_lock);

    if (!fs->last_snapshot) {
        pthread_rwlock_unlock(&fs->file_lock);
        fprintf(stderr, "[FS] No snapshot available for undo\n");
        return -1;
    }

    // Destroy current sentences
    SentenceNode *current = fs->sentences;
    while (current) {
        SentenceNode *next = current->next;
        sentence_destroy(current);
        current = next;
    }

    // Restore from snapshot
    fs->sentences = fs->last_snapshot->sentences;
    
    // Count sentences
    fs->sentence_count = 0;
    current = fs->sentences;
    while (current) {
        fs->sentence_count++;
        current = current->next;
    }

    // Clear snapshot (we only support one level of undo)
    free(fs->last_snapshot);
    fs->last_snapshot = NULL;

    // Write to disk
    int result = fs_write_to_disk(fs, base_path);
    fs->last_modified = time(NULL);

    pthread_rwlock_unlock(&fs->file_lock);

    printf("[FS] Undo completed for file %s\n", fs->filename);
    return result;
}