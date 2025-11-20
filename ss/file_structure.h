#ifndef FILE_STRUCTURE_H
#define FILE_STRUCTURE_H

#include <pthread.h>
#include <time.h>
#include "../include/constants.h"

// Word node - represents a single word in a sentence
typedef struct WordNode {
    char word[MAX_WORD_LENGTH];
    char whitespace_after[MAX_WHITESPACE];  // Exact whitespace (spaces, tabs, newlines) after this word
    struct WordNode *next;
} WordNode;

// Sentence node - represents a sentence (sequence of words)
typedef struct SentenceNode {
    WordNode *words;
    int word_count;
    char delimiters[MAX_WHITESPACE];  // Can have multiple delimiters like "!.?"
    char whitespace_after_delimiters[MAX_WHITESPACE];  // Whitespace after all delimiters
    
    // Locking for concurrent writes
    pthread_rwlock_t lock;
    int locked_for_write;
    char locked_by[64];
    
    struct SentenceNode *next;
} SentenceNode;

// Checkpoint node - represents a saved state of a file
typedef struct CheckpointNode {
    char tag[64];                    // Checkpoint identifier
    SentenceNode *sentences;         // Saved file content
    time_t timestamp;                // When checkpoint was created
    struct CheckpointNode *next;     // Next checkpoint
} CheckpointNode;

// Checkpoint list - manages all checkpoints for a file
typedef struct CheckpointList {
    CheckpointNode *head;
    int count;
    pthread_mutex_t lock;           // Lock for checkpoint operations
} CheckpointList;

// File snapshot for undo
typedef struct FileSnapshot {
    SentenceNode *sentences;
    time_t timestamp;
    char modified_by[64];
    struct FileSnapshot *next;
} FileSnapshot;

typedef struct FolderNode {
    char folder_name[MAX_FOLDER_NAME];
    char full_path[MAX_PATH_LENGTH];      // e.g., "root/documents/work"
    struct FolderNode *parent;
    struct FolderNode *children;          // First child
    struct FolderNode *next_sibling;      // Next sibling
    time_t created_at;
    char owner[64];
    pthread_rwlock_t lock;
} FolderNode;

// File structure - represents an entire file
typedef struct FileStructure {
    char filename[MAX_FILENAME];
    char owner[64];
    
    SentenceNode *sentences;
    int sentence_count;
    
    // For undo functionality
    FileSnapshot *last_snapshot;
    
    // Track active write with temp file
    char temp_filename[MAX_FILENAME];  // Temp file being written
    int has_active_write;  // Flag: is someone writing?
    char write_user[64];   // Who is writing
    int write_sentence;    // Which sentence
    
    time_t last_modified;
    pthread_rwlock_t file_lock;
    
    struct FileStructure *next;
    CheckpointList checkpoints;

    char folder_path[MAX_PATH_LENGTH];
    FolderNode *parent_folder;

} FileStructure;

// File manager - manages all files with a hash table
typedef struct FileManager {
    FileStructure **files;  // Hash table
    int table_size;
    char base_path[MAX_FILENAME];
    pthread_mutex_t manager_lock;

    FolderNode *root_folder;
    pthread_mutex_t folder_lock;

} FileManager;

// ==================== Function Declarations ====================

// Helper Functions (for internal use by write operations)
SentenceNode* find_sentence(FileStructure *fs, int sentence_num);
WordNode* find_word_at_position(SentenceNode *sentence, int position);
SentenceNode* ensure_sentence_exists(FileStructure *fs, int sentence_num);  // ADD THIS

// File Manager Operations
int fm_init(FileManager *manager, const char *base_path, int table_size);
void fm_cleanup(FileManager *manager);
FileStructure* fm_get_or_create_file(FileManager *manager, const char *filename, const char *owner);
FileStructure* fm_get_file(FileManager *manager, const char *filename);

// Word Operations
WordNode* word_create(const char *word);
void word_destroy(WordNode *word);

// Sentence Operations
SentenceNode* sentence_create(char delimiter);
void sentence_destroy(SentenceNode *sentence);
int sentence_add_word(SentenceNode *sentence, const char *word, int position);
int sentence_get_word_count(SentenceNode *sentence);
char* sentence_to_string(SentenceNode *sentence, char *buffer, size_t buffer_size);

// File Structure Operations
FileStructure* fs_create(const char *filename, const char *owner);
void fs_destroy(FileStructure *fs);
int fs_load_from_disk(FileStructure *fs, const char *base_path);
void fs_parse_content(FileStructure *fs, const char *content);
int fs_write_to_disk(FileStructure *fs, const char *base_path);

// Read Operations
char* fs_read_all(FileStructure *fs, char *buffer, size_t buffer_size);
char* fs_read_sentence(FileStructure *fs, int sentence_num, char *buffer, size_t buffer_size);
int fs_get_sentence_count(FileStructure *fs);

// Write Operations
int fs_lock_sentence(FileStructure *fs, int sentence_num, const char *username);
int fs_unlock_sentence(FileStructure *fs, int sentence_num);
int fs_write_word(FileStructure *fs, int sentence_num, int word_index, 
                  const char *content, const char *username);
int fs_commit_write(FileStructure *fs, const char *base_path);

// Undo Operations
SentenceNode* fs_deep_copy_sentences(SentenceNode *original);
int fs_create_snapshot(FileStructure *fs, const char *username);
int fs_undo(FileStructure *fs, const char *base_path);

// ==================== Checkpoint Operations ====================
int checkpoint_init(CheckpointList *list);
void checkpoint_cleanup(CheckpointList *list);
int checkpoint_create(FileStructure *fs, const char *tag);
CheckpointNode* checkpoint_find(CheckpointList *list, const char *tag);
char* checkpoint_view(FileStructure *fs, const char *tag, char *buffer, size_t buffer_size);
int checkpoint_revert(FileStructure *fs, const char *tag, const char *base_path);
char* checkpoint_list(FileStructure *fs, char *buffer, size_t buffer_size);
int checkpoint_save_to_disk(FileStructure *fs, const char *base_path);
int checkpoint_load_from_disk(FileStructure *fs, const char *base_path);

// ==================== Folder Operations ====================
int fm_init_folders(FileManager *manager);
void fm_cleanup_folders(FileManager *manager);
FolderNode* folder_create(const char *folder_name, const char *full_path, 
                          const char *owner, FolderNode *parent);
void folder_destroy_recursive(FolderNode *folder);
FolderNode* folder_find_by_path(FileManager *manager, const char *path);
int folder_create_hierarchy(FileManager *manager, const char *path, const char *owner);
int folder_delete(FileManager *manager, const char *path, const char *username);
int folder_move_file(FileManager *manager, const char *filename, 
                     const char *dest_folder_path, const char *username);
char* folder_list_contents(FileManager *manager, const char *path, 
                           char *buffer, size_t buffer_size, const char *username);
int folder_create_on_disk(FileManager *manager, const char *path);
int folder_delete_on_disk(FileManager *manager, const char *path);


#endif // FILE_STRUCTURE_H