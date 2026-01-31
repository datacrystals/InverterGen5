#include "cli.h"
#include <stdio.h>
#include <string.h>

int cmd_help(int argc, char **argv) {
    if (argc > 1) {
        const Command *cmd = CLI_FindCommand(argv[1]);
        if (cmd) {
            printf("%s - %s\r\n", cmd->name, cmd->help);
            if (cmd->usage) printf("Usage: %s %s\r\n", cmd->name, cmd->usage);
        } else {
            printf("Unknown command: %s\r\n", argv[1]);
        }
        return 0;
    }

    printf("\r\nCommands:\r\n");
    uint8_t count;
    const Command *list = CLI_GetCommandList(&count);
    
    for (int i = 0; i < count; i++) {
        printf("  %-10s %s\r\n", list[i].name, list[i].help);
    }
    printf("\r\nType 'help <command>' for details.\r\n");
    return 0;
}