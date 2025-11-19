#include "client.h"
#include <ctype.h>

// Helper function to convert string to uppercase
static void to_upper(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
}

// Helper function to trim whitespace
static char* trim(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int parse_command(const char *input, Command *cmd) {
    if (!input || !cmd) {
        return ERR_INVALID_COMMAND;
    }

    // Initialize command structure
    memset(cmd, 0, sizeof(Command));
    
    // Make a copy of input for parsing
    char buffer[MAX_COMMAND_LENGTH];
    strncpy(buffer, input, MAX_COMMAND_LENGTH - 1);
    buffer[MAX_COMMAND_LENGTH - 1] = '\0';

    // Tokenize the input
    char *token = strtok(buffer, " \t");
    if (!token) {
        return ERR_INVALID_COMMAND;
    }

    // Convert command to uppercase for case-insensitive matching
    char command[32];
    strncpy(command, token, sizeof(command) - 1);
    command[sizeof(command) - 1] = '\0';
    to_upper(command);

    // Parse VIEW command
    if (strcmp(command, "VIEW") == 0) {
        cmd->type = MSG_VIEW;
        cmd->flag_all = false;
        cmd->flag_list = false;

        // Check for flags
        token = strtok(NULL, " \t");
        if (token) {
            if (token[0] == '-') {
                for (int i = 1; token[i]; i++) {
                    if (token[i] == 'a' || token[i] == 'A') {
                        cmd->flag_all = true;
                    } else if (token[i] == 'l' || token[i] == 'L') {
                        cmd->flag_list = true;
                    }
                }
            }
        }
        return SUCCESS;
    }

    // Parse READ command
    if (strcmp(command, "READ") == 0) {
        cmd->type = MSG_READ;
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        return SUCCESS;
    }

    // Parse CREATE command
    if (strcmp(command, "CREATE") == 0) {
        cmd->type = MSG_CREATE;
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        return SUCCESS;
    }

    // Parse WRITE command
    if (strcmp(command, "WRITE") == 0) {
        cmd->type = MSG_WRITE;
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        cmd->sentence_num = atoi(token);
        return SUCCESS;
    }

    // Parse UNDO command
    if (strcmp(command, "UNDO") == 0) {
        cmd->type = MSG_UNDO;
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        return SUCCESS;
    }

    // Parse INFO command
    if (strcmp(command, "INFO") == 0) {
        cmd->type = MSG_INFO;
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        return SUCCESS;
    }

    // Parse DELETE command
    if (strcmp(command, "DELETE") == 0) {
        cmd->type = MSG_DELETE;
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        return SUCCESS;
    }

    // Parse STREAM command
    if (strcmp(command, "STREAM") == 0) {
        cmd->type = MSG_STREAM;
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        return SUCCESS;
    }

    // Parse LIST command
    if (strcmp(command, "LIST") == 0) {
        cmd->type = MSG_LIST_USERS;
        return SUCCESS;
    }

    // Parse ADDACCESS command
    if (strcmp(command, "ADDACCESS") == 0) {
        cmd->type = MSG_ADD_ACCESS;
        
        token = strtok(NULL, " \t");
        if (!token || token[0] != '-') {
            return ERR_INVALID_COMMAND;
        }
        
        // Check for -R or -W flag
        if (token[1] == 'R' || token[1] == 'r') {
            cmd->read_access = true;
            cmd->write_access = false;
        } else if (token[1] == 'W' || token[1] == 'w') {
            cmd->read_access = true;
            cmd->write_access = true;
        } else {
            return ERR_INVALID_COMMAND;
        }
        
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->username, token, MAX_USERNAME_LENGTH - 1);
        return SUCCESS;
    }

    // Parse REMACCESS command
    if (strcmp(command, "REMACCESS") == 0) {
        cmd->type = MSG_REM_ACCESS;
        
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->username, token, MAX_USERNAME_LENGTH - 1);
        return SUCCESS;
    }

    // Parse EXEC command
    if (strcmp(command, "EXEC") == 0) {
        cmd->type = MSG_EXEC;
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        return SUCCESS;
    }

    if (strcmp(command, "CHECKPOINT") == 0) {
        cmd->type = MSG_CHECKPOINT;
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->content, token, sizeof(cmd->content) - 1);
        return SUCCESS;
    }

    // Parse VIEWCHECKPOINT command
    if (strcmp(command, "VIEWCHECKPOINT") == 0) {
        cmd->type = MSG_VIEWCHECKPOINT;
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->content, token, sizeof(cmd->content) - 1);
        return SUCCESS;
    }

    // Parse REVERT command
    if (strcmp(command, "REVERT") == 0) {
        cmd->type = MSG_REVERT;
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->content, token, sizeof(cmd->content) - 1);
        return SUCCESS;
    }

    // Parse LISTCHECKPOINTS command
    if (strcmp(command, "LISTCHECKPOINTS") == 0) {
        cmd->type = MSG_LISTCHECKPOINTS;
        token = strtok(NULL, " \t");
        if (!token) {
            return ERR_INVALID_COMMAND;
        }
        strncpy(cmd->filename, token, MAX_FILENAME_LENGTH - 1);
        return SUCCESS;
    }

    return ERR_INVALID_COMMAND;
}

void print_error(int error_code) {
    switch (error_code) {
        case ERR_INVALID_COMMAND:
            fprintf(stderr, "Error: Invalid command\n");
            break;
        case ERR_CONNECTION:
            fprintf(stderr, "Error: Connection failed\n");
            break;
        case ERR_FILE_NOT_FOUND:
            fprintf(stderr, "Error: File not found\n");
            break;
        case ERR_ACCESS_DENIED:
            fprintf(stderr, "Error: Access denied\n");
            break;
        case ERR_SERVER_ERROR:
            fprintf(stderr, "Error: Server error\n");
            break;
        case ERR_INVALID_INDEX:
            fprintf(stderr, "Error: Invalid index\n");
            break;
        case ERR_FILE_EXISTS:
            fprintf(stderr, "Error: File already exists\n");
            break;
        case ERR_SENTENCE_LOCKED:
            fprintf(stderr, "Error: Sentence is locked by another user\n");
            break;
        default:
            fprintf(stderr, "Error: Unknown error (code: %d)\n", error_code);
    }
}