// ==================== Folder Operations ====================

#include "file_structure.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>

/**
 * Initialize folder system for file manager
 */
int fm_init_folders(FileManager *manager) {
    if (!manager) {
        return -1;
    }
    
    if (pthread_mutex_init(&manager->folder_lock, NULL) != 0) {
        return -1;
    }
    
    // Create root folder
    manager->root_folder = folder_create("root", "root", "system", NULL);
    if (!manager->root_folder) {
        pthread_mutex_destroy(&manager->folder_lock);
        return -1;
    }
    
    // Create root directory on disk
    char root_path[MAX_PATH_LENGTH];
    snprintf(root_path, sizeof(root_path), "%s/root", manager->base_path);
    mkdir(root_path, 0755);
    
    printf("[FM-FOLDERS] Folder system initialized with root\n");
    return 0;
}

/**
 * Cleanup folder tree
 */
void fm_cleanup_folders(FileManager *manager) {
    if (!manager || !manager->root_folder) {
        return;
    }
    
    pthread_mutex_lock(&manager->folder_lock);
    folder_destroy_recursive(manager->root_folder);
    manager->root_folder = NULL;
    pthread_mutex_unlock(&manager->folder_lock);
    
    pthread_mutex_destroy(&manager->folder_lock);
}

/**
 * Create a folder node
 */
FolderNode* folder_create(const char *folder_name, const char *full_path, 
                          const char *owner, FolderNode *parent) {
    if (!folder_name || !full_path) {
        return NULL;
    }
    
    FolderNode *folder = (FolderNode *)malloc(sizeof(FolderNode));
    if (!folder) {
        return NULL;
    }
    
    strncpy(folder->folder_name, folder_name, MAX_FOLDER_NAME - 1);
    folder->folder_name[MAX_FOLDER_NAME - 1] = '\0';
    
    strncpy(folder->full_path, full_path, MAX_PATH_LENGTH - 1);
    folder->full_path[MAX_PATH_LENGTH - 1] = '\0';
    
    if (owner) {
        strncpy(folder->owner, owner, 63);
        folder->owner[63] = '\0';
    } else {
        folder->owner[0] = '\0';
    }
    
    folder->parent = parent;
    folder->children = NULL;
    folder->next_sibling = NULL;
    folder->created_at = time(NULL);
    
    if (pthread_rwlock_init(&folder->lock, NULL) != 0) {
        free(folder);
        return NULL;
    }
    
    return folder;
}

/**
 * Recursively destroy folder tree
 */
void folder_destroy_recursive(FolderNode *folder) {
    if (!folder) {
        return;
    }
    
    // Destroy all children first
    FolderNode *child = folder->children;
    while (child) {
        FolderNode *next = child->next_sibling;
        folder_destroy_recursive(child);
        child = next;
    }
    
    pthread_rwlock_destroy(&folder->lock);
    free(folder);
}

/**
 * Find folder by path (e.g., "root/documents/work")
 */
FolderNode* folder_find_by_path(FileManager *manager, const char *path) {
    if (!manager || !path) {
        return NULL;
    }
    
    pthread_mutex_lock(&manager->folder_lock);
    
    // Handle root
    if (strcmp(path, "root") == 0 || strcmp(path, "") == 0) {
        pthread_mutex_unlock(&manager->folder_lock);
        return manager->root_folder;
    }
    
    // Tokenize path
    char path_copy[MAX_PATH_LENGTH];
    strncpy(path_copy, path, MAX_PATH_LENGTH - 1);
    path_copy[MAX_PATH_LENGTH - 1] = '\0';
    
    FolderNode *current = manager->root_folder;
    char *token = strtok(path_copy, "/");
    
    // Skip "root" token if present
    if (token && strcmp(token, "root") == 0) {
        token = strtok(NULL, "/");
    }
    
    while (token && current) {
        pthread_rwlock_rdlock(&current->lock);
        FolderNode *child = current->children;
        FolderNode *found = NULL;
        
        while (child) {
            if (strcmp(child->folder_name, token) == 0) {
                found = child;
                break;
            }
            child = child->next_sibling;
        }
        
        pthread_rwlock_unlock(&current->lock);
        
        if (!found) {
            pthread_mutex_unlock(&manager->folder_lock);
            return NULL;
        }
        
        current = found;
        token = strtok(NULL, "/");
    }
    
    pthread_mutex_unlock(&manager->folder_lock);
    return current;
}

/**
 * Create folder hierarchy (creates all parent folders if needed)
 */
int folder_create_hierarchy(FileManager *manager, const char *path, const char *owner) {
    if (!manager || !path) {
        return -1;
    }
    
    pthread_mutex_lock(&manager->folder_lock);
    
    // Parse path
    char path_copy[MAX_PATH_LENGTH];
    strncpy(path_copy, path, MAX_PATH_LENGTH - 1);
    path_copy[MAX_PATH_LENGTH - 1] = '\0';
    
    FolderNode *current = manager->root_folder;
    char current_path[MAX_PATH_LENGTH] = "root";
    
    char *token = strtok(path_copy, "/");
    
    // Skip "root" if present
    if (token && strcmp(token, "root") == 0) {
        token = strtok(NULL, "/");
    }
    
    while (token) {
        pthread_rwlock_wrlock(&current->lock);
        
        // Check if child exists
        FolderNode *child = current->children;
        FolderNode *found = NULL;
        
        while (child) {
            if (strcmp(child->folder_name, token) == 0) {
                found = child;
                break;
            }
            child = child->next_sibling;
        }
        
        if (!found) {
            // Create new folder
            char new_full_path[MAX_PATH_LENGTH];
            snprintf(new_full_path, sizeof(new_full_path), "%s/%s", current_path, token);
            
            FolderNode *new_folder = folder_create(token, new_full_path, owner, current);
            if (!new_folder) {
                pthread_rwlock_unlock(&current->lock);
                pthread_mutex_unlock(&manager->folder_lock);
                return -1;
            }
            
            // Add as first child
            new_folder->next_sibling = current->children;
            current->children = new_folder;
            
            // Create on disk
            char disk_path[MAX_PATH_LENGTH];
            snprintf(disk_path, sizeof(disk_path), "%s/%s", 
                     manager->base_path, new_full_path);
            mkdir(disk_path, 0755);
            
            printf("[FM-FOLDERS] Created folder: %s\n", new_full_path);
            found = new_folder;
        }
        
        pthread_rwlock_unlock(&current->lock);
        
        // Update current path
        strncat(current_path, "/", MAX_PATH_LENGTH - strlen(current_path) - 1);
        strncat(current_path, token, MAX_PATH_LENGTH - strlen(current_path) - 1);
        
        current = found;
        token = strtok(NULL, "/");
    }
    
    pthread_mutex_unlock(&manager->folder_lock);
    return 0;
}

/**
 * Move file to a different folder
 */
int folder_move_file(FileManager *manager, const char *filename, 
                     const char *dest_folder_path, const char *username) {
    if (!manager || !filename || !dest_folder_path || !username) {
        return -1;
    }
    
    // Find file
    // pthread_mutex_lock(&manager->manager_lock);
    FileStructure *fs = fm_get_or_create_file(manager, filename, username);
    
    if (!fs) {
        pthread_mutex_unlock(&manager->manager_lock);
        fprintf(stderr, "[FM-FOLDERS] File not found: %s\n", filename);
        return -1;
    }

    pthread_mutex_lock(&manager->manager_lock);
    
    // Check ownership
    if (strcmp(fs->owner, username) != 0) {
        pthread_mutex_unlock(&manager->manager_lock);
        fprintf(stderr, "[FM-FOLDERS] Access denied: only owner can move files\n");
        return -2;
    }

    // Find destination folder (this needs folder_lock, not manager_lock)
    pthread_mutex_unlock(&manager->manager_lock);
    
    // Find destination folder
    FolderNode *dest_folder = folder_find_by_path(manager, dest_folder_path);
    if (!dest_folder) {
        // pthread_mutex_unlock(&manager->manager_lock);
        fprintf(stderr, "[FM-FOLDERS] Destination folder not found: %s\n", dest_folder_path);
        return -3;
    }

    // Lock again for file path operations
    pthread_mutex_lock(&manager->manager_lock);
    
    // Build old and new file paths
    char old_path[MAX_PATH_LENGTH];
    char new_path[MAX_PATH_LENGTH];
    
    // Handle file in root or subfolder
    if (strlen(fs->folder_path) == 0 || strcmp(fs->folder_path, "") == 0) {
        // File was just created, assume it's in root
        snprintf(old_path, sizeof(old_path), "%s/%s", 
                 manager->base_path, fs->filename);
    } else if (strcmp(fs->folder_path, "root") == 0) {
        // File is in root
        snprintf(old_path, sizeof(old_path), "%s/root/%s", 
                 manager->base_path, fs->filename);
    } else {
        // File is in a subfolder
        snprintf(old_path, sizeof(old_path), "%s/%s/%s", 
                 manager->base_path, fs->folder_path, fs->filename);
    }
    
    snprintf(new_path, sizeof(new_path), "%s/%s/%s", 
             manager->base_path, dest_folder->full_path, fs->filename);

    printf("[FM-FOLDERS] Moving from: %s\n", old_path);
    printf("[FM-FOLDERS] Moving to: %s\n", new_path);

    // Unlock before file system operation to avoid holding lock during I/O
    pthread_mutex_unlock(&manager->manager_lock);
    
    // Move file on disk (no locks held)
    if (rename(old_path, new_path) != 0) {
        pthread_mutex_unlock(&manager->manager_lock);
        fprintf(stderr, "[FM-FOLDERS] Failed to move file on disk: %s\n", strerror(errno));
        return -4;
    }

    // Lock again to update metadata
    pthread_mutex_lock(&manager->manager_lock);
    
    // Update file structure
    strncpy(fs->folder_path, dest_folder->full_path, MAX_PATH_LENGTH - 1);
    fs->folder_path[MAX_PATH_LENGTH - 1] = '\0';
    fs->parent_folder = dest_folder;
    
    pthread_mutex_unlock(&manager->manager_lock);
    
    printf("[FM-FOLDERS] Moved file '%s' to folder '%s'\n", filename, dest_folder_path);
    return 0;
}

/**
 * List folder contents
 */
char* folder_list_contents(FileManager *manager, const char *path, 
                           char *buffer, size_t buffer_size, const char *username) {
    if (!manager || !path || !buffer || buffer_size == 0) {
        return NULL;
    }
    
    FolderNode *folder = folder_find_by_path(manager, path);
    if (!folder) {
        snprintf(buffer, buffer_size, "ERROR: Folder not found");
        return NULL;
    }
    
    buffer[0] = '\0';
    size_t offset = 0;
    
    offset += snprintf(buffer + offset, buffer_size - offset,
                      "Contents of folder '%s':\n\n", path);
    
    // List subfolders
    pthread_rwlock_rdlock(&folder->lock);
    FolderNode *child = folder->children;
    
    if (child) {
        offset += snprintf(buffer + offset, buffer_size - offset, "Folders:\n");
        while (child && offset < buffer_size - 100) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                              "  [DIR]  %s/\n", child->folder_name);
            child = child->next_sibling;
        }
        offset += snprintf(buffer + offset, buffer_size - offset, "\n");
    }
    
    pthread_rwlock_unlock(&folder->lock);
    
    // List files in this folder
    pthread_mutex_lock(&manager->manager_lock);
    
    offset += snprintf(buffer + offset, buffer_size - offset, "Files:\n");
    int file_count = 0;
    
    for (int i = 0; i < manager->table_size && offset < buffer_size - 100; i++) {
        FileStructure *fs = manager->files[i];
        while (fs) {
            if (strcmp(fs->folder_path, folder->full_path) == 0) {
                offset += snprintf(buffer + offset, buffer_size - offset,
                                  "  [FILE] %s (owner: %s)\n", 
                                  fs->filename, fs->owner);
                file_count++;
            }
            fs = fs->next;
        }
    }
    
    if (file_count == 0) {
        offset += snprintf(buffer + offset, buffer_size - offset, "  (no files)\n");
    }
    
    pthread_mutex_unlock(&manager->manager_lock);
    
    return buffer;
}