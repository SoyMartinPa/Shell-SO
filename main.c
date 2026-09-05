#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "funciones.h"

int main(void) {
    char *line = NULL;
    size_t lineCapacity = 0;
    ssize_t lineLength;

    while (1) {
        char **pipeLines;
        char ***commands;
        int pipeCount;
        int i;

        printf("mishell$ ");
        fflush(stdout);

        lineLength = getline(&line, &lineCapacity, stdin);

        if (lineLength == -1) {
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0') {
            continue;
        }

        if (strcmp(line, "exit") == 0) {
            break;
        }

        pipeLines = parseLine(line, '|', &pipeCount);

        if (pipeLines == NULL || pipeCount == 0) {
            continue;
        }

        commands = malloc(sizeof(char **) * pipeCount);

        if (commands == NULL) {
            perror("Error de memoria");
            freeTokens(pipeLines, pipeCount);
            continue;
        }

        for (i = 0; i < pipeCount; i++) {
            int argumentCount;

            commands[i] = parseLine(pipeLines[i], ' ', &argumentCount);
        }

        if (pipeCount == 1) {
            executeNormalCommand(commands[0]);
        } else {
            executePipelineCommand(commands, pipeCount);
        }

        for (i = 0; i < pipeCount; i++) {
            int argumentCount = 0;

            while (commands[i] != NULL && commands[i][argumentCount] != NULL) {
                argumentCount++;
            }

            freeTokens(commands[i], argumentCount);
        }

        free(commands);
        freeTokens(pipeLines, pipeCount);
    }

    free(line);
    return 0;
}