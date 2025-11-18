#include "file_structure.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

// ==================== Helper Functions ====================

/**
 * Ensures a sentence exists at the given position.
 * Creates sentences as needed if they don't exist.
 * Returns pointer to the sentence at sentence_num, or NULL on error.
 * NOTE: Caller must hold fs->file_lock (at least read lock, write lock if creating)
 */
static SentenceNode* ensure_sentence_exists(FileStructure *fs, int sentence_num) {
    if (!fs || sentence_num < 0) {
        return NULL;
    }
    
    // Navigate to the target sentence or the end of the list
    SentenceNode *current = fs->sentences;
    SentenceNode *prev = NULL;
    int count = 0;
    
    while (current && count < sentence_num) {
        prev = current;
        current = current->next;
        count++;
    }
    
    // If sentence exists, return it
    if (current && count == sentence_num) {
        return current;
    }
    
    // Create missing sentences
    while (count <= sentence_num) {
        SentenceNode *new_sentence = sentence_create('\0');
        if (!new_sentence) {
            return NULL;
        }
        
        if (!fs->sentences) {
            // First sentence in the file
            fs->sentences = new_sentence;
            current = new_sentence;
        } else if (!prev) {
            // Should not happen, but handle it
            fs->sentences = new_sentence;
            current = new_sentence;
        } else {
            // Append to the list
            prev->next = new_sentence;
            current = new_sentence;
        }
        
        prev = current;
        fs->sentence_count++;
        count++;
    }
    
    return current;
}

/**
 * Find sentence at a specific position
 * NOTE: Caller must hold fs->file_lock (at least read lock)
 */
SentenceNode* find_sentence(FileStructure *fs, int sentence_num) {
    if (!fs || sentence_num < 0) {
        return NULL;
    }
    
    SentenceNode *current = fs->sentences;
    int count = 0;
    
    while (current && count < sentence_num) {
        current = current->next;
        count++;
    }
    
    return current;
}

/**
 * Find the word at a specific position in a sentence
 * NOTE: Caller must hold sentence->lock (at least read lock)
 */
WordNode* find_word_at_position(SentenceNode *sentence, int position) {
    if (!sentence || position < 0) {
        return NULL;
    }
    
    WordNode *current = sentence->words;
    int count = 0;
    
    while (current && count < position) {
        current = current->next;
        count++;
    }
    
    return current;
}

/**
 * Insert a word at a specific position in a sentence
 * Handles all linking and whitespace properly
 * NOTE: Caller must hold sentence->lock (write lock)
 */
static int insert_word_at_position(SentenceNode *sentence, const char *word, 
                                   int position, const char *whitespace_before) {
    if (!sentence || !word) {
        return -1;
    }
    
    WordNode *new_word = word_create(word);
    if (!new_word) {
        return -1;
    }
    
    // Initialize whitespace
    new_word->whitespace_after[0] = '\0';
    
    if (position == 0 || !sentence->words) {
        // Insert at beginning
        new_word->next = sentence->words;
        sentence->words = new_word;
        sentence->word_count++;
        return 0;
    }
    
    // Find insertion point
    WordNode *prev = find_word_at_position(sentence, position - 1);
    if (!prev) {
        // Position is beyond end, append at end
        prev = sentence->words;
        while (prev->next) {
            prev = prev->next;
        }
    }
    
    // Insert after prev
    new_word->next = prev->next;
    prev->next = new_word;
    
    // Set whitespace before the new word (on previous word)
    if (whitespace_before) {
        strncpy(prev->whitespace_after, whitespace_before, MAX_WHITESPACE - 1);
        prev->whitespace_after[MAX_WHITESPACE - 1] = '\0';
    }
    
    sentence->word_count++;
    return 0;
}

/**
 * Parse content and return structured data about words, delimiters, and whitespace
 * NEW: Now handles multiple sentences separated by delimiters
 */
typedef struct {
    char word[MAX_WORD_LENGTH];
    char whitespace_after[MAX_WHITESPACE];
    int has_word;
} ParsedWord;

typedef struct ParsedSentence {
    ParsedWord *words;
    int word_count;
    int word_capacity;
    char delimiters[MAX_WHITESPACE];
    char whitespace_after_delimiters[MAX_WHITESPACE];
    char leading_whitespace[MAX_WHITESPACE];
    struct ParsedSentence *next;
} ParsedSentence;

typedef struct {
    ParsedSentence *sentences;
    int sentence_count;
} ParsedContent;

static ParsedSentence* create_parsed_sentence() {
    ParsedSentence *sentence = (ParsedSentence *)calloc(1, sizeof(ParsedSentence));
    if (!sentence) {
        return NULL;
    }
    
    sentence->word_capacity = 16;
    sentence->words = (ParsedWord *)calloc(sentence->word_capacity, sizeof(ParsedWord));
    if (!sentence->words) {
        free(sentence);
        return NULL;
    }
    
    return sentence;
}

static ParsedContent* parse_write_content(const char *content) {
    if (!content) {
        return NULL;
    }
    
    ParsedContent *parsed = (ParsedContent *)calloc(1, sizeof(ParsedContent));
    if (!parsed) {
        return NULL;
    }
    
    // Create first sentence
    ParsedSentence *current_sentence = create_parsed_sentence();
    if (!current_sentence) {
        free(parsed);
        return NULL;
    }
    
    parsed->sentences = current_sentence;
    parsed->sentence_count = 1;
    ParsedSentence *last_sentence = current_sentence;
    
    char word_buffer[MAX_WORD_LENGTH];
    int word_idx = 0;
    char whitespace_buffer[MAX_WHITESPACE];
    int ws_idx = 0;
    int in_whitespace = 0;
    int found_first_word = 0;  // Track if we've seen the first word in current sentence
    
    for (int i = 0; content[i] != '\0'; i++) {
        char ch = content[i];
        
        if (ch == '.' || ch == '!' || ch == '?') {
            // Save current word if exists
            if (word_idx > 0) {
                // Expand array if needed
                if (current_sentence->word_count >= current_sentence->word_capacity) {
                    current_sentence->word_capacity *= 2;
                    ParsedWord *new_words = (ParsedWord *)realloc(
                        current_sentence->words, 
                        current_sentence->word_capacity * sizeof(ParsedWord)
                    );
                    if (!new_words) {
                        // Cleanup on error
                        while (parsed->sentences) {
                            ParsedSentence *next = parsed->sentences->next;
                            free(parsed->sentences->words);
                            free(parsed->sentences);
                            parsed->sentences = next;
                        }
                        free(parsed);
                        return NULL;
                    }
                    current_sentence->words = new_words;
                }
                
                word_buffer[word_idx] = '\0';
                strncpy(current_sentence->words[current_sentence->word_count].word, 
                       word_buffer, MAX_WORD_LENGTH - 1);
                current_sentence->words[current_sentence->word_count].word[MAX_WORD_LENGTH - 1] = '\0';
                current_sentence->words[current_sentence->word_count].whitespace_after[0] = '\0';
                current_sentence->words[current_sentence->word_count].has_word = 1;
                current_sentence->word_count++;
                
                word_idx = 0;
                ws_idx = 0;
                in_whitespace = 0;
                found_first_word = 1;
            }
            
            // Add delimiter to current sentence
            int delim_len = strlen(current_sentence->delimiters);
            if (delim_len < MAX_WHITESPACE - 1) {
                current_sentence->delimiters[delim_len] = ch;
                current_sentence->delimiters[delim_len + 1] = '\0';
            }
            
            // Check if there's more content after this delimiter
            // If so, we need to start a new sentence
            int has_more_content = 0;
            for (int j = i + 1; content[j] != '\0'; j++) {
                if (content[j] != ' ' && content[j] != '\t' && 
                    content[j] != '\n' && content[j] != '\r') {
                    has_more_content = 1;
                    break;
                }
            }
            
            if (has_more_content) {
                // Create new sentence for content after delimiter
                ParsedSentence *new_sentence = create_parsed_sentence();
                if (!new_sentence) {
                    // Cleanup on error
                    while (parsed->sentences) {
                        ParsedSentence *next = parsed->sentences->next;
                        free(parsed->sentences->words);
                        free(parsed->sentences);
                        parsed->sentences = next;
                    }
                    free(parsed);
                    return NULL;
                }
                
                last_sentence->next = new_sentence;
                last_sentence = new_sentence;
                current_sentence = new_sentence;
                parsed->sentence_count++;
                
                found_first_word = 0;
                ws_idx = 0;
                in_whitespace = 0;
            }
            
        } else if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            // Whitespace
            if (word_idx > 0) {
                // End of word, save it
                if (current_sentence->word_count >= current_sentence->word_capacity) {
                    current_sentence->word_capacity *= 2;
                    ParsedWord *new_words = (ParsedWord *)realloc(
                        current_sentence->words, 
                        current_sentence->word_capacity * sizeof(ParsedWord)
                    );
                    if (!new_words) {
                        while (parsed->sentences) {
                            ParsedSentence *next = parsed->sentences->next;
                            free(parsed->sentences->words);
                            free(parsed->sentences);
                            parsed->sentences = next;
                        }
                        free(parsed);
                        return NULL;
                    }
                    current_sentence->words = new_words;
                }
                
                word_buffer[word_idx] = '\0';
                strncpy(current_sentence->words[current_sentence->word_count].word, 
                       word_buffer, MAX_WORD_LENGTH - 1);
                current_sentence->words[current_sentence->word_count].word[MAX_WORD_LENGTH - 1] = '\0';
                current_sentence->words[current_sentence->word_count].has_word = 1;
                
                // Start collecting whitespace
                ws_idx = 0;
                if (ws_idx < MAX_WHITESPACE - 1) {
                    whitespace_buffer[ws_idx++] = ch;
                }
                
                current_sentence->word_count++;
                word_idx = 0;
                in_whitespace = 1;
                found_first_word = 1;
                
            } else if (in_whitespace) {
                // Continue collecting whitespace
                if (ws_idx < MAX_WHITESPACE - 1) {
                    whitespace_buffer[ws_idx++] = ch;
                }
            } else if (!found_first_word) {
                // Leading whitespace before first word in sentence
                int leading_len = strlen(current_sentence->leading_whitespace);
                if (leading_len < MAX_WHITESPACE - 1) {
                    current_sentence->leading_whitespace[leading_len] = ch;
                    current_sentence->leading_whitespace[leading_len + 1] = '\0';
                }
            } else if (current_sentence->delimiters[0] != '\0') {
                // Whitespace after delimiter (but we already moved to new sentence)
                // This whitespace becomes leading whitespace of current sentence
                int leading_len = strlen(current_sentence->leading_whitespace);
                if (leading_len < MAX_WHITESPACE - 1) {
                    current_sentence->leading_whitespace[leading_len] = ch;
                    current_sentence->leading_whitespace[leading_len + 1] = '\0';
                }
            }
            
        } else {
            // Regular character
            if (in_whitespace && current_sentence->word_count > 0) {
                // Save accumulated whitespace to previous word
                whitespace_buffer[ws_idx] = '\0';
                strncpy(current_sentence->words[current_sentence->word_count - 1].whitespace_after, 
                       whitespace_buffer, MAX_WHITESPACE - 1);
                current_sentence->words[current_sentence->word_count - 1].whitespace_after[MAX_WHITESPACE - 1] = '\0';
                ws_idx = 0;
                in_whitespace = 0;
            }
            
            // Add to current word
            if (word_idx < MAX_WORD_LENGTH - 1) {
                word_buffer[word_idx++] = ch;
            }
        }
    }
    
    // Handle trailing word
    if (word_idx > 0) {
        if (current_sentence->word_count >= current_sentence->word_capacity) {
            current_sentence->word_capacity *= 2;
            ParsedWord *new_words = (ParsedWord *)realloc(
                current_sentence->words, 
                current_sentence->word_capacity * sizeof(ParsedWord)
            );
            if (!new_words) {
                while (parsed->sentences) {
                    ParsedSentence *next = parsed->sentences->next;
                    free(parsed->sentences->words);
                    free(parsed->sentences);
                    parsed->sentences = next;
                }
                free(parsed);
                return NULL;
            }
            current_sentence->words = new_words;
        }
        
        word_buffer[word_idx] = '\0';
        strncpy(current_sentence->words[current_sentence->word_count].word, 
               word_buffer, MAX_WORD_LENGTH - 1);
        current_sentence->words[current_sentence->word_count].word[MAX_WORD_LENGTH - 1] = '\0';
        current_sentence->words[current_sentence->word_count].whitespace_after[0] = '\0';
        current_sentence->words[current_sentence->word_count].has_word = 1;
        current_sentence->word_count++;
    }
    
    // Handle trailing whitespace after last word (if no delimiter)
    if (in_whitespace && current_sentence->word_count > 0 && 
        current_sentence->delimiters[0] == '\0') {
        whitespace_buffer[ws_idx] = '\0';
        strncpy(current_sentence->words[current_sentence->word_count - 1].whitespace_after, 
               whitespace_buffer, MAX_WHITESPACE - 1);
        current_sentence->words[current_sentence->word_count - 1].whitespace_after[MAX_WHITESPACE - 1] = '\0';
    }
    
    return parsed;
}

static void free_parsed_content(ParsedContent *parsed) {
    if (!parsed) {
        return;
    }
    
    ParsedSentence *current = parsed->sentences;
    while (current) {
        ParsedSentence *next = current->next;
        if (current->words) {
            free(current->words);
        }
        free(current);
        current = next;
    }
    
    free(parsed);
}

// ==================== Read Operations ====================

char* fs_read_sentence(FileStructure *fs, int sentence_num, char *buffer, size_t buffer_size) {
    if (!fs || !buffer || buffer_size == 0 || sentence_num < 0) {
        return NULL;
    }

    // Read lock on file to traverse sentence list
    pthread_rwlock_rdlock(&fs->file_lock);

    SentenceNode *sentence = find_sentence(fs, sentence_num);
    if (!sentence) {
        pthread_rwlock_unlock(&fs->file_lock);
        return NULL;
    }

    // Read lock on sentence to read its content
    pthread_rwlock_rdlock(&sentence->lock);
    pthread_rwlock_unlock(&fs->file_lock); // Release file lock early

    sentence_to_string(sentence, buffer, buffer_size);
    
    pthread_rwlock_unlock(&sentence->lock);

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

    // Read lock on file to check sentence count
    pthread_rwlock_rdlock(&fs->file_lock);

    // Check if sentence_num is within valid range
    if (sentence_num > fs->sentence_count) {
        pthread_rwlock_unlock(&fs->file_lock);
        fprintf(stderr, "[FS] ERROR: Sentence index %d out of range (file has %d sentences, valid range is 0-%d)\n", 
                sentence_num, fs->sentence_count, 
                fs->sentence_count);
        return -5;  // Sentence index out of range
    }

    // Find or create the sentence
    SentenceNode *sentence = NULL;

    if (sentence_num == fs->sentence_count) {
        // Need to create a new sentence at the end
        pthread_rwlock_unlock(&fs->file_lock);
        pthread_rwlock_wrlock(&fs->file_lock);  // Upgrade to write lock
        
        sentence = ensure_sentence_exists(fs, sentence_num);
        if (!sentence) {
            pthread_rwlock_unlock(&fs->file_lock);
            fprintf(stderr, "[FS] ERROR: Failed to create sentence %d in file %s\n", 
                    sentence_num, fs->filename);
            return -1;
        }
    } else {
        // Sentence should exist, just find it
        sentence = find_sentence(fs, sentence_num);
        if (!sentence) {
            pthread_rwlock_unlock(&fs->file_lock);
            fprintf(stderr, "[FS] ERROR: Failed to access sentence %d in file %s\n", 
                    sentence_num, fs->filename);
            return -1;
        }
    }

    // Write lock on sentence to check and modify lock state
    pthread_rwlock_wrlock(&sentence->lock);
    pthread_rwlock_unlock(&fs->file_lock); // Release file lock early

    // Check if already locked
    if (sentence->locked_for_write) {
        pthread_rwlock_unlock(&sentence->lock);
        fprintf(stderr, "[FS] Sentence %d is already locked by %s\n", 
                sentence_num, sentence->locked_by);
        return -2; // Already locked
    }

    // Lock the sentence
    sentence->locked_for_write = 1;
    strncpy(sentence->locked_by, username, 63);
    sentence->locked_by[63] = '\0';

    pthread_rwlock_unlock(&sentence->lock);

    printf("[FS] Sentence %d locked by %s in file %s\n", 
           sentence_num, username, fs->filename);
    return 0;
}

int fs_unlock_sentence(FileStructure *fs, int sentence_num) {
    if (!fs || sentence_num < 0) {
        return -1;
    }

    // Read lock on file to find sentence
    pthread_rwlock_rdlock(&fs->file_lock);

    SentenceNode *sentence = find_sentence(fs, sentence_num);
    if (!sentence) {
        pthread_rwlock_unlock(&fs->file_lock);
        return -1;
    }

    // Write lock on sentence to modify lock state
    pthread_rwlock_wrlock(&sentence->lock);
    pthread_rwlock_unlock(&fs->file_lock); // Release file lock early

    // Unlock the sentence
    sentence->locked_for_write = 0;
    sentence->locked_by[0] = '\0';

    pthread_rwlock_unlock(&sentence->lock);

    printf("[FS] Sentence %d unlocked in file %s\n", sentence_num, fs->filename);
    return 0;
}

int fs_write_word(FileStructure *fs, int sentence_num, int word_index, 
                  const char *content, const char *username) {
    if (!fs || !content || !username || sentence_num < 0 || word_index < 0) {
        return -1;
    }

    // Parse the content first
    ParsedContent *parsed = parse_write_content(content);
    if (!parsed) {
        fprintf(stderr, "[FS] Failed to parse write content\n");
        return -1;
    }

    // Read lock on file for sentence access (lightweight check)
    pthread_rwlock_rdlock(&fs->file_lock);

    // Find the starting sentence - DO NOT CREATE if it doesn't exist
    SentenceNode *start_sentence = find_sentence(fs, sentence_num);
    if (!start_sentence) {
        pthread_rwlock_unlock(&fs->file_lock);
        fprintf(stderr, "[FS] ERROR: Sentence index %d out of range (file has %d sentences)\n", 
                sentence_num, fs->sentence_count);
        free_parsed_content(parsed);
        return -5;  // New error code for out of range
    }

    // Write lock on starting sentence ONLY for modification
    pthread_rwlock_wrlock(&start_sentence->lock);
    pthread_rwlock_unlock(&fs->file_lock); // Release file lock immediately

    // Check if sentence is locked by this user
    if (!start_sentence->locked_for_write) {
        pthread_rwlock_unlock(&start_sentence->lock);
        // pthread_rwlock_unlock(&fs->file_lock);
        fprintf(stderr, "[FS] Sentence %d is not locked\n", sentence_num);
        free_parsed_content(parsed);
        return -2;
    }

    if (strcmp(start_sentence->locked_by, username) != 0) {
        pthread_rwlock_unlock(&start_sentence->lock);
        // pthread_rwlock_unlock(&fs->file_lock);
        fprintf(stderr, "[FS] Sentence %d is locked by %s, not %s\n", 
                sentence_num, start_sentence->locked_by, username);
        free_parsed_content(parsed);
        return -3;
    }
    
    // Validate word_index for the starting sentence
    if (word_index > start_sentence->word_count) {
        pthread_rwlock_unlock(&start_sentence->lock);
        // pthread_rwlock_unlock(&fs->file_lock);
        fprintf(stderr, "[FS] Word index %d out of range (sentence has %d words, valid range is 0-%d)\n", 
                word_index, start_sentence->word_count, start_sentence->word_count);
        free_parsed_content(parsed);
        return -4;
    }

    // Process each parsed sentence
    ParsedSentence *parsed_sentence = parsed->sentences;
    SentenceNode *current_sentence = start_sentence;
    int current_sentence_num = sentence_num;
    int insert_pos = word_index;
    int first_sentence = 1;

    while (parsed_sentence) {
        // Handle leading whitespace for first word
        if (parsed_sentence->word_count > 0 && parsed_sentence->leading_whitespace[0] != '\0') {
            if (insert_pos > 0) {
                // Insert after an existing word - set its whitespace_after
                WordNode *prev_word = find_word_at_position(current_sentence, insert_pos - 1);
                if (prev_word) {
                    strncpy(prev_word->whitespace_after, parsed_sentence->leading_whitespace, 
                           MAX_WHITESPACE - 1);
                    prev_word->whitespace_after[MAX_WHITESPACE - 1] = '\0';
                }
            }
        }
        
        // Insert words at the specified position
        for (int i = 0; i < parsed_sentence->word_count; i++) {
            if (parsed_sentence->words[i].has_word) {
                const char *ws_before = NULL;
                
                if (i == 0 && insert_pos == 0) {
                    // First word at beginning of sentence - no whitespace before
                    ws_before = NULL;
                } else if (i == 0) {
                    // First word being inserted but not at position 0
                    // whitespace_before was already set above from leading_whitespace
                    ws_before = NULL;
                } else {
                    // Subsequent words - use whitespace from previous parsed word
                    ws_before = parsed_sentence->words[i - 1].whitespace_after;
                }
                
                insert_word_at_position(current_sentence, parsed_sentence->words[i].word, insert_pos, ws_before);
                insert_pos++;
                
                // Set whitespace after this word
                WordNode *inserted_word = find_word_at_position(current_sentence, insert_pos - 1);
                if (inserted_word) {
                    strncpy(inserted_word->whitespace_after, parsed_sentence->words[i].whitespace_after, 
                           MAX_WHITESPACE - 1);
                    inserted_word->whitespace_after[MAX_WHITESPACE - 1] = '\0';
                }
            }
        }

        // Handle delimiters
        if (parsed_sentence->delimiters[0] != '\0') {
            if (current_sentence->delimiters[0] == '\0') {
                // Sentence had no delimiter, set it
                strncpy(current_sentence->delimiters, parsed_sentence->delimiters, MAX_WHITESPACE - 1);
                current_sentence->delimiters[MAX_WHITESPACE - 1] = '\0';
                
                // Set whitespace after delimiters
                strncpy(current_sentence->whitespace_after_delimiters, 
                       parsed_sentence->whitespace_after_delimiters, MAX_WHITESPACE - 1);
                current_sentence->whitespace_after_delimiters[MAX_WHITESPACE - 1] = '\0';
            } else {
                // Sentence already has delimiter - append additional delimiters
                int existing_len = strlen(current_sentence->delimiters);
                int new_len = strlen(parsed_sentence->delimiters);
                
                if (existing_len + new_len < MAX_WHITESPACE - 1) {
                    strcat(current_sentence->delimiters, parsed_sentence->delimiters);
                }
            }
        }

        // Move to next parsed sentence
        parsed_sentence = parsed_sentence->next;
        
        if (parsed_sentence) {
            // Unlock current sentence before moving to next
            pthread_rwlock_unlock(&current_sentence->lock);
            
            // Need file write lock to create new sentence
            pthread_rwlock_wrlock(&fs->file_lock);
            // Move to next sentence in file structure (or create it)
            current_sentence_num++;
            SentenceNode *next_sentence = ensure_sentence_exists(fs, current_sentence_num);
            pthread_rwlock_unlock(&fs->file_lock); // Release immediately after creation
            
            if (!next_sentence) {
                // pthread_rwlock_unlock(&fs->file_lock);
                fprintf(stderr, "[FS] Failed to create sentence %d\n", current_sentence_num);
                free_parsed_content(parsed);
                return -1;
            }
            
            // Lock the next sentence for writing
            pthread_rwlock_wrlock(&next_sentence->lock);
            current_sentence = next_sentence;
            
            // For subsequent sentences, always insert at position 0
            insert_pos = 0;
            first_sentence = 0;
        }
    }

    // Unlock the last sentence we worked on
    pthread_rwlock_unlock(&current_sentence->lock);
    // pthread_rwlock_unlock(&fs->file_lock);

    free_parsed_content(parsed);

    printf("[FS] Write completed: inserted at position %d in sentence %d (spanned %d sentences)\n",
           word_index, sentence_num, parsed->sentence_count);
    return 0;
}

int fs_commit_write(FileStructure *fs, const char *base_path) {
    if (!fs || !base_path) {
        return -1;
    }

    // Only need read lock to read data for writing to disk
    // Write lock only needed to update last_modified timestamp
    pthread_rwlock_rdlock(&fs->file_lock);
    int result = fs_write_to_disk(fs, base_path);
    pthread_rwlock_unlock(&fs->file_lock);
    
    if (result == 0) {
        // Briefly acquire write lock just to update timestamp
        pthread_rwlock_wrlock(&fs->file_lock);
        fs->last_modified = time(NULL);
        pthread_rwlock_unlock(&fs->file_lock);
    }

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
        // Lock the sentence for reading while copying
        pthread_rwlock_rdlock(&current->lock);
        
        SentenceNode *new_sentence = sentence_create('\0');
        if (!new_sentence) {
            pthread_rwlock_unlock(&current->lock);
            // Cleanup on failure
            while (new_head) {
                SentenceNode *next = new_head->next;
                sentence_destroy(new_head);
                new_head = next;
            }
            return NULL;
        }

        strncpy(new_sentence->delimiters, current->delimiters, MAX_WHITESPACE - 1);
        new_sentence->delimiters[MAX_WHITESPACE - 1] = '\0';
        strncpy(new_sentence->whitespace_after_delimiters, 
               current->whitespace_after_delimiters, MAX_WHITESPACE - 1);
        new_sentence->whitespace_after_delimiters[MAX_WHITESPACE - 1] = '\0';

        WordNode *word = current->words;
        while (word) {
            sentence_add_word(new_sentence, word->word, new_sentence->word_count);
            
            WordNode *new_word = new_sentence->words;
            int pos = 0;
            while (new_word && pos < new_sentence->word_count - 1) {
                new_word = new_word->next;
                pos++;
            }
            if (new_word) {
                strncpy(new_word->whitespace_after, word->whitespace_after, MAX_WHITESPACE - 1);
                new_word->whitespace_after[MAX_WHITESPACE - 1] = '\0';
            }
            
            word = word->next;
        }

        pthread_rwlock_unlock(&current->lock);

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

    pthread_rwlock_rdlock(&fs->file_lock);

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

    // Create new snapshot (deep copy will lock each sentence individually)
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