#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "funciones.h"

int main(void) {
    char cwd[2048];
    char *line = NULL;
    size_t lineCapacity = 0;
    ssize_t lineLength;

    while (1) {

        char **pipeLines;
        char ***commands;
        int pipeCount;
        int i;


        if (getcwd(cwd,sizeof(cwd)) == NULL){ //Verifica el cwd y da error si no se pudo obtener
            perror("getcwd");
            return 1;
        }

        printf("miShell:%s$>  ", cwd);
        fflush(stdout);

        lineLength = getline(&line, &lineCapacity, stdin); //Lee una linea de stdin y devuelve la cantidad de caracteres leidos

        if (lineLength == -1) {
            break;
        }

        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0') {
            continue;
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
            if (commands[0] != NULL && commands[0][0] != NULL) {
                if (strcmp(commands[0][0], "cd") == 0) { // Se ejecuta el comando cd directamente en el proceso principal
                    builtin_cd(commands[0]);
                } 
                else if (strcmp(commands[0][0], "exit") == 0) { // Se ejecuta el comando exit directamente en el proceso principal
                    if (builtin_exit(commands[0]) == 0) {
                        for (i = 0; i < pipeCount; i++) { // Libera la memoria de los tokens y las líneas de pipe antes de salir
                            int argumentCount = 0;
                            while (commands[i] != NULL && commands[i][argumentCount] != NULL) {
                                argumentCount++;
                            }
                            freeTokens(commands[i], argumentCount);
                        }
                        free(commands);
                        freeTokens(pipeLines, pipeCount);
                        continue;
                    }
                } 
                else {
                    executeNormalCommand(commands[0]);
                }
            }
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