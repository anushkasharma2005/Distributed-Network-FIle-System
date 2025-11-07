#ifndef FILE_STRUCTURE_H
#define FILE_STRUCTURE_H

#include <pthread.h>
#include <time.h>

#define MAX_WORD_LENGTH 256
#define MAX_FILENAME 256

// Node representing a word in a sentence
typedef struct WordNode {
    char word[MAX_WORD_LENGTH];
    struct WordNode *next;
} WordNode;

// Node representing a sentence (head of word list)
typedef struct SentenceNode {
    WordNode *words;           // Linked list of words
    int word_count;            // Number of words in sentence
    char delimiter;            // '.', '!', or '?'
    pthread_rwlock_t lock;     // Read-write lock for this sentence
    int locked_for_write;      // Flag indicating if locked for editing
    char locked_by[64];        // Username who locked it
    struct SentenceNode *next;
} SentenceNode;

// Structure to store file state for undo
typedef struct FileSnapshot {
    SentenceNode *sentences;   // Copy of sentence structure
    time_t timestamp;
    char modified_by[64];
    struct FileSnapshot *next; // For maintaining history (only 1 undo needed)
} FileSnapshot;

// Main file structure
typedef struct FileStructure {
    char filename[MAX_FILENAME];
    SentenceNode *sentences;
    int sentence_count;
    pthread_rwlock_t file_lock;  // Global file lock
    FileSnapshot *last_snapshot;  // For undo functionality
    time_t last_modified;
    char owner[64];
    struct FileStructure *next;   // For hash table chaining
} FileStructure;

// File manager to maintain all file structures
typedef struct FileManager {
    FileStructure **files;        // Hash table of file structures
    int table_size;
    pthread_mutex_t manager_lock;
    char base_path[MAX_FILENAME];
} FileManager;

// Function declarations

// File Manager Operations
int fm_init(FileManager *manager, const char *base_path, int table_size);
void fm_cleanup(FileManager *manager);
FileStructure* fm_get_or_create_file(FileManager *manager, const char *filename, const char *owner);
FileStructure* fm_get_file(FileManager *manager, const char *filename);

// File Structure Operations
FileStructure* fs_create(const char *filename, const char *owner);
void fs_destroy(FileStructure *fs);
int fs_load_from_disk(FileStructure *fs, const char *base_path);
int fs_write_to_disk(FileStructure *fs, const char *base_path);

// Sentence Operations
SentenceNode* sentence_create(char delimiter);
void sentence_destroy(SentenceNode *sentence);
int sentence_add_word(SentenceNode *sentence, const char *word, int position);
int sentence_get_word_count(SentenceNode *sentence);
char* sentence_to_string(SentenceNode *sentence, char *buffer, size_t buffer_size);

// Word Operations
WordNode* word_create(const char *word);
void word_destroy(WordNode *word);

// Write Operations
int fs_lock_sentence(FileStructure *fs, int sentence_num, const char *username);
int fs_unlock_sentence(FileStructure *fs, int sentence_num);
int fs_write_word(FileStructure *fs, int sentence_num, int word_index, 
                  const char *content, const char *username);
int fs_commit_write(FileStructure *fs, const char *base_path);

// Read Operations
char* fs_read_all(FileStructure *fs, char *buffer, size_t buffer_size);
char* fs_read_sentence(FileStructure *fs, int sentence_num, char *buffer, size_t buffer_size);

// Undo Operations
int fs_create_snapshot(FileStructure *fs, const char *username);
int fs_undo(FileStructure *fs, const char *base_path);

// Utility Functions
int fs_get_sentence_count(FileStructure *fs);
void fs_parse_content(FileStructure *fs, const char *content);
SentenceNode* fs_deep_copy_sentences(SentenceNode *original);

#endif // FILE_STRUCTURE_H