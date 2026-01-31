#ifndef CLI_H
#define CLI_H
#include <stdint.h>

typedef int (*CommandFunc)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *help;
    const char *usage;
    CommandFunc func;
} Command;

void CLI_Init(void);
void CLI_Process(const char *line);

const Command* CLI_FindCommand(const char *name);
const Command* CLI_GetCommandList(uint8_t *count);

#endif