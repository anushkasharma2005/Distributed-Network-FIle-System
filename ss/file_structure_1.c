#include "file_structure.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

// Hash function for filename
static unsigned int hash_filename(const char *filename, int table_size) {
    unsigned int hash = 5381;
    int c;
    while ((c = *filename++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % table_size;
}

// ==================== File Manager Operations ====================

int fm_init(FileManager *manager, const char *base_path, int table_size) {
    if (!manager || !base_path || table_size <= 0) {
        return -1;
    }

    manager->table_size = table_size;
    manager->files = (FileStructure **)calloc(table_size, sizeof(FileStructure *));
    if (!manager->files) {
        return -1;
    }

    strncpy(manager->base_path, base_path, MAX_FILENAME - 1);
    
    if (pthread_mutex_init(&manager->manager_lock, NULL) != 0) {
        free(manager->files);
        return -1;
    }

    printf("[FM] File manager initialized with base path: %s\n", base_path);
    return 0;
}

void fm_cleanup(FileManager *manager) {
    if (!manager || !manager->files) {
        return;
    }

    pthread_mutex_lock(&manager->manager_lock);

    for (int i = 0; i < manager->table_size; i++) {
        FileStructure *current = manager->files[i];
        while (current) {
            FileStructure *next = current->next;
            fs_destroy(current);
            current = next;
        }
    }

    free(manager->files);
    pthread_mutex_unlock(&manager->manager_lock);
    pthread_mutex_destroy(&manager->manager_lock);
}

FileStructure* fm_get_or_create_file(FileManager *manager, const char *filename, const char *owner) {
    if (!manager || !filename) {
        return NULL;
    }

    unsigned int index = hash_filename(filename, manager->table_size);
    
    pthread_mutex_lock(&manager->manager_lock);

    // Search for existing file
    FileStructure *current = manager->files[index];
    while (current) {
        if (strcmp(current->filename, filename) == 0) {
            pthread_mutex_unlock(&manager->manager_lock);
            return current;
        }
        current = current->next;
    }

    // Create new file structure
    FileStructure *new_fs = fs_create(filename, owner);
    if (!new_fs) {
        pthread_mutex_unlock(&manager->manager_lock);
        return NULL;
    }

    // Try to load from disk
    fs_load_from_disk(new_fs, manager->base_path);

    // Add to hash table
    new_fs->next = manager->files[index];
    manager->files[index] = new_fs;

    pthread_mutex_unlock(&manager->manager_lock);
    
    printf("[FM] File structure created/loaded: %s\n", filename);
    return new_fs;
}

FileStructure* fm_get_file(FileManager *manager, const char *filename) {
    if (!manager || !filename) {
        return NULL;
    }

    unsigned int index = hash_filename(filename, manager->table_size);
    
    pthread_mutex_lock(&manager->manager_lock);

    FileStructure *current = manager->files[index];
    while (current) {
        if (strcmp(current->filename, filename) == 0) {
            pthread_mutex_unlock(&manager->manager_lock);
            return current;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&manager->manager_lock);
    return NULL;
}

// ==================== Word Operations ====================

WordNode* word_create(const char *word) {
    if (!word) {
        return NULL;
    }

    WordNode *node = (WordNode *)malloc(sizeof(WordNode));
    if (!node) {
        return NULL;
    }

    strncpy(node->word, word, MAX_WORD_LENGTH - 1);
    node->word[MAX_WORD_LENGTH - 1] = '\0';
    node->whitespace_after[0] = '\0';  // No whitespace by default
    node->next = NULL;

    return node;
}

void word_destroy(WordNode *word) {
    if (!word) {
        return;
    }
    free(word);
}

// ==================== Sentence Operations ====================

SentenceNode* sentence_create(char delimiter) {
    SentenceNode *sentence = (SentenceNode *)malloc(sizeof(SentenceNode));
    if (!sentence) {
        return NULL;
    }

    sentence->words = NULL;
    sentence->word_count = 0;
    if (delimiter) {
        sentence->delimiters[0] = delimiter;
        sentence->delimiters[1] = '\0';
    } else {
        sentence->delimiters[0] = '\0';
    }
    sentence->whitespace_after_delimiters[0] = '\0';
    sentence->locked_for_write = 0;
    sentence->locked_by[0] = '\0';
    sentence->next = NULL;

    if (pthread_rwlock_init(&sentence->lock, NULL) != 0) {
        free(sentence);
        return NULL;
    }

    return sentence;
}

void sentence_destroy(SentenceNode *sentence) {
    if (!sentence) {
        return;
    }

    WordNode *current = sentence->words;
    while (current) {
        WordNode *next = current->next;
        word_destroy(current);
        current = next;
    }

    pthread_rwlock_destroy(&sentence->lock);
    free(sentence);
}

int sentence_add_word(SentenceNode *sentence, const char *word, int position) {
    if (!sentence || !word) {
        return -1;
    }

    WordNode *new_word = word_create(word);
    if (!new_word) {
        return -1;
    }

    // Insert at beginning
    if (position == 0 || sentence->words == NULL) {
        new_word->next = sentence->words;
        sentence->words = new_word;
        sentence->word_count++;
        return 0;
    }

    // Insert at position
    WordNode *current = sentence->words;
    int count = 0;

    while (current && count < position - 1) {
        current = current->next;
        count++;
    }

    if (!current) {
        // Position beyond end, append at end
        current = sentence->words;
        while (current->next) {
            current = current->next;
        }
        current->next = new_word;
    } else {
        new_word->next = current->next;
        current->next = new_word;
    }

    sentence->word_count++;
    return 0;
}

int sentence_get_word_count(SentenceNode *sentence) {
    if (!sentence) {
        return 0;
    }
    return sentence->word_count;
}

char* sentence_to_string(SentenceNode *sentence, char *buffer, size_t buffer_size) {
    if (!sentence || !buffer || buffer_size == 0) {
        return NULL;
    }

    buffer[0] = '\0';
    size_t offset = 0;

    WordNode *current = sentence->words;
    while (current && offset < buffer_size - 1) {
        // Add the word
        int word_len = strlen(current->word);
        if (offset + word_len < buffer_size) {
            strcpy(buffer + offset, current->word);
            offset += word_len;
        } else {
            break;
        }
        
        // Add whitespace after the word
        if (current->whitespace_after[0] != '\0') {
            int ws_len = strlen(current->whitespace_after);
            if (offset + ws_len < buffer_size) {
                strcpy(buffer + offset, current->whitespace_after);
                offset += ws_len;
            }
        }
        
        current = current->next;
    }

    // Add delimiters (can be multiple like "!.?")
    if (sentence->delimiters[0] != '\0') {
        int delim_len = strlen(sentence->delimiters);
        if (offset + delim_len < buffer_size) {
            strcpy(buffer + offset, sentence->delimiters);
            offset += delim_len;
        }
    }
    
    // Add whitespace after delimiters
    if (sentence->whitespace_after_delimiters[0] != '\0') {
        int ws_len = strlen(sentence->whitespace_after_delimiters);
        if (offset + ws_len < buffer_size) {
            strcpy(buffer + offset, sentence->whitespace_after_delimiters);
            offset += ws_len;
        }
    }

    return buffer;
}

// ==================== File Structure Operations ====================

FileStructure* fs_create(const char *filename, const char *owner) {
    if (!filename) {
        return NULL;
    }

    FileStructure *fs = (FileStructure *)malloc(sizeof(FileStructure));
    if (!fs) {
        return NULL;
    }

    strncpy(fs->filename, filename, MAX_FILENAME - 1);
    fs->filename[MAX_FILENAME - 1] = '\0';
    
    if (owner) {
        strncpy(fs->owner, owner, 63);
        fs->owner[63] = '\0';
    } else {
        fs->owner[0] = '\0';
    }

    fs->sentences = NULL;
    fs->sentence_count = 0;
    fs->last_snapshot = NULL;
    fs->last_modified = time(NULL);
    fs->next = NULL;

    if (pthread_rwlock_init(&fs->file_lock, NULL) != 0) {
        free(fs);
        return NULL;
    }

    return fs;
}

void fs_destroy(FileStructure *fs) {
    if (!fs) {
        return;
    }

    // Destroy all sentences
    SentenceNode *current = fs->sentences;
    while (current) {
        SentenceNode *next = current->next;
        sentence_destroy(current);
        current = next;
    }

    // Destroy snapshot
    if (fs->last_snapshot) {
        SentenceNode *snap_current = fs->last_snapshot->sentences;
        while (snap_current) {
            SentenceNode *next = snap_current->next;
            sentence_destroy(snap_current);
            snap_current = next;
        }
        free(fs->last_snapshot);
    }

    pthread_rwlock_destroy(&fs->file_lock);
    free(fs);
}

int fs_load_from_disk(FileStructure *fs, const char *base_path) {
    if (!fs || !base_path) {
        return -1;
    }

    char file_path[MAX_FILENAME * 2];
    snprintf(file_path, sizeof(file_path), "%s/%s", base_path, fs->filename);

    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        // File doesn't exist yet, that's okay
        return 0;
    }

    // Read entire file content
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(fp);
        return 0;
    }

    char *content = (char *)malloc(file_size + 1);
    if (!content) {
        fclose(fp);
        return -1;
    }

    size_t read_size = fread(content, 1, file_size, fp);
    content[read_size] = '\0';
    fclose(fp);

    // Parse content into structure
    fs_parse_content(fs, content);
    free(content);

    printf("[FS] Loaded file from disk: %s (%d sentences)\n", 
           fs->filename, fs->sentence_count);
    return 0;
}

void fs_parse_content(FileStructure *fs, const char *content) {
    if (!fs || !content) {
        return;
    }

    SentenceNode *current_sentence = NULL;
    SentenceNode *last_sentence_in_list = NULL;
    char word_buffer[MAX_WORD_LENGTH];
    int word_idx = 0;
    char whitespace_buffer[MAX_WHITESPACE];
    int ws_idx = 0;
    char delimiter_buffer[MAX_WHITESPACE];
    int delim_idx = 0;
    WordNode *last_word = NULL;
    int after_delimiter = 0;  // Flag to track if we're collecting whitespace after delimiter(s)

    for (int i = 0; content[i] != '\0'; i++) {
        char ch = content[i];

        // Check for sentence delimiter
        if (ch == '.' || ch == '!' || ch == '?') {
            // If we have accumulated whitespace, it goes before the delimiter
            if (ws_idx > 0 && last_word) {
                whitespace_buffer[ws_idx] = '\0';
                strncpy(last_word->whitespace_after, whitespace_buffer, MAX_WHITESPACE - 1);
                last_word->whitespace_after[MAX_WHITESPACE - 1] = '\0';
                ws_idx = 0;
                last_word = NULL;
            }
            
            // Add current word if exists
            if (word_idx > 0) {
                word_buffer[word_idx] = '\0';
                
                if (!current_sentence) {
                    current_sentence = sentence_create('\0');
                    if (!fs->sentences) {
                        fs->sentences = current_sentence;
                        last_sentence_in_list = current_sentence;
                    } else {
                        last_sentence_in_list->next = current_sentence;
                        last_sentence_in_list = current_sentence;
                    }
                    fs->sentence_count++;
                }
                
                sentence_add_word(current_sentence, word_buffer, current_sentence->word_count);
                
                // Get the last word (but don't set it for whitespace accumulation)
                WordNode *w = current_sentence->words;
                while (w && w->next) w = w->next;
                if (w) {
                    w->whitespace_after[0] = '\0';  // No whitespace before delimiter
                }
                
                word_idx = 0;
            }

            // Start or continue accumulating delimiters
            if (!current_sentence) {
                // Create sentence if needed
                current_sentence = sentence_create('\0');
                if (!fs->sentences) {
                    fs->sentences = current_sentence;
                    last_sentence_in_list = current_sentence;
                } else {
                    last_sentence_in_list->next = current_sentence;
                    last_sentence_in_list = current_sentence;
                }
                fs->sentence_count++;
            }
            
            // Add delimiter to buffer
            if (delim_idx < MAX_WHITESPACE - 1) {
                delimiter_buffer[delim_idx++] = ch;
            }
            
            after_delimiter = 1;
        } else if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            // Whitespace character
            if (word_idx > 0) {
                // We just finished a word, save it
                word_buffer[word_idx] = '\0';
                
                if (!current_sentence) {
                    current_sentence = sentence_create('\0');
                    if (!fs->sentences) {
                        fs->sentences = current_sentence;
                        last_sentence_in_list = current_sentence;
                    } else {
                        last_sentence_in_list->next = current_sentence;
                        last_sentence_in_list = current_sentence;
                    }
                    fs->sentence_count++;
                }
                
                sentence_add_word(current_sentence, word_buffer, current_sentence->word_count);
                
                // Get the last word to accumulate whitespace
                WordNode *w = current_sentence->words;
                while (w && w->next) w = w->next;
                last_word = w;
                
                word_idx = 0;
                ws_idx = 0;
                after_delimiter = 0;
                delim_idx = 0;
                // Start accumulating whitespace
                if (ws_idx < MAX_WHITESPACE - 1) {
                    whitespace_buffer[ws_idx++] = ch;
                }
            } else if (after_delimiter && ws_idx < MAX_WHITESPACE - 1) {
                // Accumulate whitespace after delimiter(s)
                whitespace_buffer[ws_idx++] = ch;
            } else if (last_word && ws_idx < MAX_WHITESPACE - 1) {
                // Continue accumulating whitespace after a word
                whitespace_buffer[ws_idx++] = ch;
            }
        } else {
            // Regular character - start of new word
            
            // If we have accumulated delimiters, finalize the sentence
            if (delim_idx > 0 && current_sentence) {
                delimiter_buffer[delim_idx] = '\0';
                strncpy(current_sentence->delimiters, delimiter_buffer, MAX_WHITESPACE - 1);
                current_sentence->delimiters[MAX_WHITESPACE - 1] = '\0';
                delim_idx = 0;
                
                // Save accumulated whitespace after delimiters
                if (ws_idx > 0) {
                    whitespace_buffer[ws_idx] = '\0';
                    strncpy(current_sentence->whitespace_after_delimiters, whitespace_buffer, MAX_WHITESPACE - 1);
                    current_sentence->whitespace_after_delimiters[MAX_WHITESPACE - 1] = '\0';
                    ws_idx = 0;
                }
                
                // Start new sentence
                current_sentence = NULL;
                after_delimiter = 0;
            } else if (ws_idx > 0 && last_word) {
                // Save accumulated whitespace to previous word
                whitespace_buffer[ws_idx] = '\0';
                strncpy(last_word->whitespace_after, whitespace_buffer, MAX_WHITESPACE - 1);
                last_word->whitespace_after[MAX_WHITESPACE - 1] = '\0';
                ws_idx = 0;
                last_word = NULL;
            }
            
            // Start new word
            if (word_idx < MAX_WORD_LENGTH - 1) {
                word_buffer[word_idx++] = ch;
            }
        }
    }

    // Handle trailing word without delimiter
    if (word_idx > 0) {
        word_buffer[word_idx] = '\0';
        
        if (!current_sentence) {
            current_sentence = sentence_create('\0');
            if (!fs->sentences) {
                fs->sentences = current_sentence;
                last_sentence_in_list = current_sentence;
            } else {
                last_sentence_in_list->next = current_sentence;
                last_sentence_in_list = current_sentence;
            }
            fs->sentence_count++;
        }
        
        sentence_add_word(current_sentence, word_buffer, current_sentence->word_count);
        
        // Last word in file has no whitespace after
        WordNode *w = current_sentence->words;
        while (w && w->next) w = w->next;
        if (w) {
            w->whitespace_after[0] = '\0';
        }
    }
    
    // Handle trailing delimiters and whitespace
    if (delim_idx > 0 && current_sentence) {
        delimiter_buffer[delim_idx] = '\0';
        strncpy(current_sentence->delimiters, delimiter_buffer, MAX_WHITESPACE - 1);
        current_sentence->delimiters[MAX_WHITESPACE - 1] = '\0';
    }
    
    if (ws_idx > 0 && after_delimiter && current_sentence) {
        whitespace_buffer[ws_idx] = '\0';
        strncpy(current_sentence->whitespace_after_delimiters, whitespace_buffer, MAX_WHITESPACE - 1);
        current_sentence->whitespace_after_delimiters[MAX_WHITESPACE - 1] = '\0';
    }
}

int fs_write_to_disk(FileStructure *fs, const char *base_path) {
    if (!fs || !base_path) {
        return -1;
    }

    char file_path[MAX_FILENAME * 2];
    snprintf(file_path, sizeof(file_path), "%s/%s", base_path, fs->filename);

    FILE *fp = fopen(file_path, "w");
    if (!fp) {
        fprintf(stderr, "[FS] Failed to open file for writing: %s (%s)\n", 
                file_path, strerror(errno));
        return -1;
    }

    char sentence_buffer[4096];
    SentenceNode *current = fs->sentences;

    // NOTE: Caller must hold at least read lock on fs->file_lock
    while (current) {
        // Read lock each sentence individually
        pthread_rwlock_rdlock(&current->lock);
        sentence_to_string(current, sentence_buffer, sizeof(sentence_buffer));
        pthread_rwlock_unlock(&current->lock);
        
        fprintf(fp, "%s", sentence_buffer);
        current = current->next;
    }

    fclose(fp);
    printf("[FS] Written file to disk: %s\n", fs->filename);
    return 0;
}

// char* fs_read_all(FileStructure *fs, char *buffer, size_t buffer_size) {
//     if (!fs || !buffer || buffer_size == 0) {
//         return NULL;
//     }

//     // Read lock on file - allows concurrent reads
//     pthread_rwlock_rdlock(&fs->file_lock);

//     buffer[0] = '\0';
//     size_t offset = 0;
//     char sentence_buffer[4096];

//     SentenceNode *current = fs->sentences;
//     while (current && offset < buffer_size - 1) {
//         // Read lock on each sentence individually
//         pthread_rwlock_rdlock(&current->lock);
        
//         sentence_to_string(current, sentence_buffer, sizeof(sentence_buffer));
        
//         pthread_rwlock_unlock(&current->lock); // Release sentence lock immediately
        
//         sentence_to_string(current, sentence_buffer, sizeof(sentence_buffer));
        
//         int written = snprintf(buffer + offset, buffer_size - offset, "%s", sentence_buffer);
//         if (written < 0 || written >= (int)(buffer_size - offset)) {
//             break;
//         }
//         offset += written;
//         current = current->next;
//     }

//     pthread_rwlock_unlock(&fs->file_lock);
//     return buffer;
// }

char* fs_read_all(FileStructure *fs, char *buffer, size_t buffer_size) {
    if (!fs || !buffer || buffer_size == 0) {
        return NULL;
    }

    // Read lock on file - allows concurrent reads
    pthread_rwlock_rdlock(&fs->file_lock);

    buffer[0] = '\0';
    size_t offset = 0;
    char sentence_buffer[4096];

    SentenceNode *current = fs->sentences;
    while (current && offset < buffer_size - 1) {
        // Read lock on each sentence individually
        pthread_rwlock_rdlock(&current->lock);
        
        sentence_to_string(current, sentence_buffer, sizeof(sentence_buffer));
        
        pthread_rwlock_unlock(&current->lock); // Release sentence lock immediately
        
        // REMOVED: Don't call sentence_to_string again!
        // sentence_to_string(current, sentence_buffer, sizeof(sentence_buffer));
        
        int written = snprintf(buffer + offset, buffer_size - offset, "%s", sentence_buffer);
        if (written < 0 || written >= (int)(buffer_size - offset)) {
            break;
        }
        offset += written;
        current = current->next;
    }

    pthread_rwlock_unlock(&fs->file_lock);
    return buffer;
}