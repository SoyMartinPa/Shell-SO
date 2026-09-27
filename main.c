#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <termios.h>
#include "funciones.h"

volatile sig_atomic_t hijo_termino = 0;
pid_t shell_pgid; //Variable globalizada



int main(void) {
    char cwd[2048];
    char *line = NULL;
    size_t lineCapacity = 0;
    ssize_t lineLength;

    //La idea de estas tres lineas de código son: obtener id de la shell, ponerla en un grupo, identificar el grupo como foregroung
    //De esta manera poder diferenciar procesos foreground y background para el control + c
    shell_pgid = getpid(); 
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);
    
    struct sigaction sa_ignore;
    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_ignore, NULL);
    sigaction(SIGQUIT, &sa_ignore, NULL);
    sigaction(SIGTTOU, &sa_ignore, NULL);

    //Esta struc se complementa con los comandos background para imprimir que terminó un proceso background
    struct sigaction sa_chld;
    sa_chld.sa_handler = manejador_sigchld;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = 0;
    sigaction(SIGCHLD, &sa_chld, NULL);

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

            if (errno == EINTR) { //Si terminó por una interrupción de sigchild, revisar hijos
                if (hijo_termino) {
                    revisarHijosTerminados(true);
                    tcflush(STDIN_FILENO, TCIFLUSH); //tcflush permite limpiar la entrada una vez interrumpida, así evitar
                }

                clearerr(stdin); //Reinicia stdin a un estado sin error. (por defecto)
                continue;
            }

            if (feof(stdin)) { //Esto pasa cuando se hace control + d
                break;
            }

            perror("getline");
            continue;
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

            commands[i] = parseArguments(pipeLines[i], &argumentCount);
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
                else if(strcmp(commands[0][0], "jobs") == 0){
                    builtin_jobs();
                }
                else if (strcmp(commands[0][0], "pmon") == 0) {
                    builtin_pmon(commands[0]);
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