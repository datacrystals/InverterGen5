#include "cli.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

extern int cmd_help(int argc, char **argv);
extern int cmd_pwm(int argc, char **argv);
extern int cmd_status(int argc, char **argv);
extern int cmd_can(int argc, char **argv);

// Explicit, readable, debugger-friendly
static const Command command_table[] = {
    {"help",    "Show available commands",    "[command]",    cmd_help},
    {"pwm",     "Set motor PWM duty",         "<0-100>",      cmd_pwm},
    // Add new commands here. That's it.
};
#define NUM_COMMANDS (sizeof(command_table) / sizeof(Command))

void CLI_Init(void) {
    printf("\r\n=== Traction Inverter Ready ===\r\n");
    printf("%d commands available. Type 'help'.\r\n", NUM_COMMANDS);
}

const Command* CLI_FindCommand(const char *name) {
    for (int i = 0; i < NUM_COMMANDS; i++) {
        if (strcasecmp(command_table[i].name, name) == 0) {
            return &command_table[i];
        }
    }
    return NULL;
}

const Command* CLI_GetCommandList(uint8_t *count) {
    if (count) *count = NUM_COMMANDS;
    return command_table;
}

void CLI_Process(const char *line) {
    if (!line || !*line) return;
    
    // Tokenize into argv (modifies copy)
    char buf[128];
    strncpy(buf, line, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    
    char *argv[8] = {0};
    int argc = 0;
    char *token = strtok(buf, " \t\r\n");
    while (token && argc < 8) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    
    if (argc == 0) return;
    
    const Command *cmd = CLI_FindCommand(argv[0]);
    if (cmd) {
        int ret = cmd->func(argc, argv);
        if (ret != 0) printf("Error: %d\r\n", ret);
    } else {
        printf("Unknown: '%s'\r\n", argv[0]);
    }
}