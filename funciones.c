#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include "funciones.h"


#define MAXPROCESS 100 //100 son los procesos background máximos (100 número arbitrario, puede ser o más o menos)
typedef struct { //Este struct permite tener una lista de comandos background (Sive para el comando interno 'jobs')
    pid_t pid;         
    char comando[256]; 
    int activo; //1 si activo, 0 sino. Util para marcar el orden de los
} ProcesoHijo;
ProcesoHijo listaHijos[MAXPROCESS];
int totalHijosActivos = 0; // Cuenta total de los hijos activos (Permite reutilizar la tabla de procesos background)

int agregarProceso(pid_t pid, const char *cmd) {
    int indiceLibre = -1;
    pid_t finished_pid;
    int status;

    //Verifica que un proceso hijo terminó para liberar el espacio de la lista
    while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAXPROCESS; i++) {
            if (listaHijos[i].pid == finished_pid) {
                listaHijos[i].activo = 0; // Lo marcamos como inactivo
            }
        }
    }
    //Busca algun espacio en la tabla que no se esté usando para agregar un nuevo proceso ahí.
    for (int i = 0; i < MAXPROCESS; i++) {
        if (listaHijos[i].activo == 0) { 
            indiceLibre = i; 
            break;
        }
    }
    //Esto significa que la tabla se llenó.
    if (indiceLibre == -1) {
        fprintf(stderr, "Error: Tabla de procesos llena.\n");
        return -1;
    }

    //Si se encontró un espacio, se asignan los valores.
    listaHijos[indiceLibre].pid = pid;
    strncpy(listaHijos[indiceLibre].comando, cmd, sizeof(listaHijos[indiceLibre].comando) - 1);
    listaHijos[indiceLibre].activo = 1;

    printf("[%d] PID: %d | Comando: %s\n", indiceLibre + 1 ,listaHijos[indiceLibre].pid, listaHijos[indiceLibre].comando);

    return indiceLibre;
}
void builtin_jobs(void) {
    pid_t finished_pid;
    int status;

    //Verifica que un proceso hijo terminó para liberar el espacio de la lista
    while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAXPROCESS; i++) {
            if (listaHijos[i].pid == finished_pid) {
                listaHijos[i].activo = 0; // Lo marcamos como inactivo
            } 
        }
    }

    //Simplemente recorre y muestra la información
    for (int i = 0; i < MAXPROCESS; i++) {
        if (listaHijos[i].activo == 1) {
            printf("[%d] PID: %d | Comando: %s\n", i+1, listaHijos[i].pid, listaHijos[i].comando);
        }
    }
}

char **parseLine(const char *line, char delimiter, int *count) {
    char *copy;
    char *token;
    char *saveptr;
    char delimiters[2];
    char **tokens;
    int capacity = 8;

    *count = 0;

    copy = malloc(strlen(line) + 1);
    if (copy == NULL) {
        perror("Error de memoria");
        return NULL;
    }

    strcpy(copy, line);

    delimiters[0] = delimiter;
    delimiters[1] = '\0';

    tokens = malloc(sizeof(char *) * capacity);
    if (tokens == NULL) {
        perror("Error de memoria");
        free(copy);
        return NULL;
    }

    token = strtok_r(copy, delimiters, &saveptr);

    while (token != NULL) {
        if (*count >= capacity - 1) { //El "-1" para tener los espacios para count MÁS el último token 
            capacity *= 2;
            tokens = realloc(tokens, sizeof(char *) * capacity);

            if (tokens == NULL) {
                perror("Error de memoria");
                free(copy);
                return NULL;
            }
        }

        tokens[*count] = malloc(strlen(token) + 1);

        if (tokens[*count] == NULL) {
            perror("Error de memoria");
            free(copy);
            freeTokens(tokens, *count);
            return NULL;
        }

        strcpy(tokens[*count], token);
        (*count)++;

        token = strtok_r(NULL, delimiters, &saveptr);
    }
    tokens[*count] = NULL;
    free(copy);
    return tokens;
}

void freeTokens(char **tokens, int count) {
    int i;

    if (tokens == NULL) {
        return;
    }

    for (i = 0; i < count; i++) {
        free(tokens[i]);
    }

    free(tokens);
}

void executeNormalCommand(char **args) {
    int argumentCount = 0, background = 0;
    pid_t pid;

    if (args == NULL || args[0] == NULL) {
        return;
    }

    while (args[argumentCount] != NULL) {argumentCount++;} //Cuenta la cantidad de argumentos ingresados

    if (argumentCount > 0 && strcmp(args[argumentCount-1], "&") == 0) //Si el elemento final es '&' dejar el proceso en background
    {
        background = 1; //flag para dejar en background
        args[argumentCount-1] = NULL; //Eliminamos el '&' en los argumentos (pues da error en el excvp) y lo dejamos como NULL
    }

    pid = fork();
    if (pid == -1) {
        perror("Error en fork");
        return;
    }

    if (pid == 0) {
        execvp(args[0], args);
        perror("Error al ejecutar el comando");
        exit(EXIT_FAILURE);
    }
    if(background){ //Si el proceso es background, agregar a tabla de background, sino, esperar por él.
        agregarProceso(pid,args[0]);
    } else{waitpid(pid, NULL, 0);}
}

void executePipelineCommand(char ***commands, int commandCount) {
    int argumentCount = 0, background = 0;
    int previousInput = STDIN_FILENO;
    int pipefd[2];
    int i;
    pid_t *pids;

    if (commands == NULL || commandCount == 0) {
        return;
    }

    pids = calloc(commandCount, sizeof(pid_t));

    if (pids == NULL) {
        perror("Error de memoria");
        return;
    }

    while (commands[commandCount-1][argumentCount-1] != NULL) {argumentCount++;} //Cuenta la cantidad de argumentos ingresados en el ultimo pipe

    if (argumentCount > 0 && strcmp(commands[commandCount-1][argumentCount-1], "&") == 0) //Si el elemento final es '&' dejar el proceso en background
    {
        background = 1; //flag para dejar en background
        commands[commandCount -1][argumentCount-1] = NULL; //Eliminamos el '&' en los argumentos (pues da error en el excvp) y lo dejamos como NULL
    }

    for (i = 0; i < commandCount; i++) {
        pid_t pid;

        if (commands[i] == NULL || commands[i][0] == NULL) {
            
            if (previousInput != STDIN_FILENO){ // Esto se ocupa para limpiar la pipeline vacia y poder reutilizar los descriptores 
                close(previousInput);
                previousInput = STDIN_FILENO;
            }
            pids[i] = 0;
            continue;
        }

        if (i < commandCount - 1) {
            if (pipe(pipefd) == -1) {
                perror("Error al crear pipe");

                for (int j = 0; j<i ; j++){ // Con esto esperamos a los procesos lanzados y así evitamos procesos zombies.
                    if (pids[j] > 0){
                        waitpid(pids[j], NULL, 0);
                    }
                }
                free(pids);
                return;
            }
        }

        pid = fork();

        if (pid == -1) {
            perror("Error en fork");
            free(pids);
            return;
        }

        if (pid == 0) {
            if (previousInput != STDIN_FILENO) {
                dup2(previousInput, STDIN_FILENO);
                close(previousInput);
            }

            if (i < commandCount - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[0]);
                close(pipefd[1]);
            }

            execvp(commands[i][0], commands[i]);
            perror("Error al ejecutar comando en pipe");
            exit(EXIT_FAILURE);
        }

        pids[i] = pid;

        if (previousInput != STDIN_FILENO) {
            close(previousInput);
        }

        if (i < commandCount - 1) {
            close(pipefd[1]);
            previousInput = pipefd[0];
        }
    }

    if (previousInput != STDIN_FILENO) {
        close(previousInput);
    }
    if(!background){
        for (i = 0; i < commandCount; i++) {
            if (pids[i] > 0) {
                waitpid(pids[i], NULL, 0);
                }
            }
    } else {
        for (i = 0; i < commandCount; i++) {
            if (pids[i] > 0) {
                agregarProceso(pids[i], commands[i][0]);
                }
            }
    }
    free(pids);

}

void builtin_cd(char **args) {
    char *target_dir;

    if (args[1] == NULL) {
        target_dir = getenv("HOME");
        if (target_dir == NULL) {
            fprintf(stderr,"cd: HOME no definido\n");
            return;
        }
    } 
    
    else if (args[2] != NULL) {
        fprintf(stderr,"cd: demasiados argumentos\n");
        return;
    } 
    
    else {
        target_dir = args[1];
    }

    if (chdir(target_dir) != 0) {
        perror("cd");
    }
}

int builtin_exit(char **args) {
    int exit_code = 0;

    if (args[1] != NULL) {
        int j = (args[1][0] == '-' || args[1][0] == '+') ? 1 : 0;
        int is_numeric = 1;

        if (args[1][j] == '\0') {
            is_numeric = 0;
        }

        for (; args[1][j] != '\0'; j++) {
            if (!isdigit(args[1][j])) {
                is_numeric = 0;
                break;
            }
        }

        if (!is_numeric) {
            fprintf(stderr,"exit: %s: requiere un argumento numérico\n",args[1]);
            exit(2);
        }

        if (args[2] != NULL) {
            fprintf(stderr,"exit: demasiados argumentos\n");
            return 0;
        }

        exit_code = atoi(args[1]);
    }

    exit(exit_code);
}