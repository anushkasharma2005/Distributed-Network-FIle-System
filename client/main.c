#include "client.h"

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <NM_IP> <NM_PORT> <USERNAME>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *nm_ip = argv[1];
    int nm_port = atoi(argv[2]);
    const char *username = argv[3];

    // Initialize client
    Client client;
    if (client_init_struct(&client, nm_ip, nm_port, username) != SUCCESS)
    {
        fprintf(stderr, "Failed to initialize client\n");
        return EXIT_FAILURE;
    }

    // Connect to Name Server
    if (client_connect_to_nm(&client) != SUCCESS)
    {
        fprintf(stderr, "Failed to connect to Name Server\n");
        client_cleanup(&client);
        return EXIT_FAILURE;
    }

    printf("Connected to Name Server as '%s'\n", username);
    printf("Type 'help' for available commands or 'exit' to quit\n\n");

    // Main command loop
    char input[MAX_COMMAND_LENGTH];
    Command cmd;

    while (1)
    {
        printf("%s> ", username);
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            break;
        }

        // Remove newline
        input[strcspn(input, "\n")] = 0;

        // Skip empty lines
        if (strlen(input) == 0)
        {
            continue;
        }

        // Handle exit command
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0)
        {
            printf("Disconnecting...\n");
            break;
        }

        // Handle help command
        if (strcmp(input, "help") == 0)
        {
            print_help();
            continue;
        }

        // Parse command
        if (parse_command(input, &cmd) != SUCCESS)
        {
            fprintf(stderr, "Invalid command. Type 'help' for usage.\n");
            continue;
        }

        // Execute command
        int result = SUCCESS;
        switch (cmd.type)
        {
        case MSG_VIEW:
            result = cmd_view(&client, cmd.flag_all, cmd.flag_list);
            break;
        case MSG_READ:
            result = cmd_read(&client, cmd.filename);
            break;
        case MSG_CREATE:
            result = cmd_create(&client, cmd.filename);
            break;
        case MSG_WRITE:
            result = cmd_write(&client, cmd.filename, cmd.sentence_num);
            break;
        case MSG_UNDO:
            result = cmd_undo(&client, cmd.filename);
            break;
        case MSG_INFO:
            result = cmd_info(&client, cmd.filename);
            break;
        case MSG_DELETE:
            result = cmd_delete(&client, cmd.filename);
            break;
        case MSG_STREAM:
            result = cmd_stream(&client, cmd.filename);
            break;
        case MSG_LIST_USERS:
            result = cmd_list_users(&client);
            break;
        case MSG_ADD_ACCESS:
            result = cmd_add_access(&client, cmd.filename, cmd.username, cmd.write_access);
            break;
        case MSG_REM_ACCESS:
            result = cmd_rem_access(&client, cmd.filename, cmd.username);
            break;
        case MSG_EXEC:
            result = cmd_exec(&client, cmd.filename);
            break;

        case MSG_CHECKPOINT:
            result = cmd_checkpoint(&client, cmd.filename, cmd.content);
            break;
        case MSG_VIEWCHECKPOINT:
            result = cmd_viewcheckpoint(&client, cmd.filename, cmd.content);
            break;
        case MSG_REVERT:
            result = cmd_revert(&client, cmd.filename, cmd.content);
            break;
        case MSG_RESTORE:
            result = cmd_restore(&client, cmd.filename);
            break;
        case MSG_LISTCHECKPOINTS:
            result = cmd_listcheckpoints(&client, cmd.filename);
            break;
        case MSG_CREATEFOLDER:
            result = cmd_createfolder(&client, cmd.filename);
            break;
        case MSG_MOVE:
            result = cmd_move(&client, cmd.filename, cmd.content);
            break;
        case MSG_VIEWFOLDER:
            result = cmd_viewfolder(&client, cmd.filename);
            break;

        default:
            fprintf(stderr, "Unknown command type\n");
        }

        if (result != SUCCESS)
        {
            print_error(result);
        }
    }

    // Cleanup
    client_cleanup(&client);
    return EXIT_SUCCESS;
}

void print_help()
{
    printf("\nAvailable Commands:\n");
    printf("==================\n\n");
    printf("File Operations:\n");
    printf("  VIEW [-a] [-l]          - List files (use -a for all, -l for details)\n");
    printf("  READ <filename>         - Read and display file contents\n");
    printf("  CREATE <filename>       - Create a new empty file\n");
    printf("  WRITE <filename> <sent#>- Write to a file (interactive mode)\n");
    printf("  UNDO <filename>         - Undo last change to file\n");
    printf("  INFO <filename>         - Display file information\n");
    printf("  DELETE <filename>       - Delete a file\n");
    printf("  STREAM <filename>       - Stream file contents word-by-word\n");
    printf("  EXEC <filename>         - Execute file contents as shell commands\n\n");
    printf("Folder Operations:\n");
    printf("  CREATEFOLDER <path>     - Create folder hierarchy (e.g., root/docs/work)\n");
    printf("  MOVE <file> <folder>    - Move file to folder\n");
    printf("  VIEWFOLDER <path>       - List folder contents\n\n");
    printf("User & Access Control:\n");
    printf("  LIST                    - List all users in system\n");
    printf("  ADDACCESS -R <file> <user> - Grant read access\n");
    printf("  ADDACCESS -W <file> <user> - Grant write access\n");
    printf("  REMACCESS <file> <user>    - Remove all access\n\n");
    printf("Checkpoint Operations:\n");
    printf("  CHECKPOINT <file> <tag>       - Create a checkpoint with given tag\n");
    printf("  VIEWCHECKPOINT <file> <tag>   - View checkpoint content\n");
    printf("  REVERT <file> <tag>           - Revert file to checkpoint\n");
    printf("  LISTCHECKPOINTS <file>        - List all checkpoints for file\n\n");
    printf("Other:\n");
    printf("  help                    - Show this help message\n");
    printf("  exit                    - Exit the client\n\n");
}