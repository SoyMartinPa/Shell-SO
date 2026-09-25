#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/time.h>
#include "funciones.h"

#define MAXPROCESS 100 //100 son los procesos background máximos (100 número arbitrario, puede ser o más o menos)

typedef struct { //Este struct permite tener una lista de comandos background (Sive para el comando interno 'jobs')
    pid_t pid;         
    char comando[256]; 
    char estado[32];
    bool activo; 
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
                listaHijos[i].activo = false; // Lo marcamos como inactivo
            }
        }
    }
    //Busca algun espacio en la tabla que no se esté usando para agregar un nuevo proceso ahí.
    for (int i = 0; i < MAXPROCESS; i++) {
        if (listaHijos[i].activo == false) { 
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
    listaHijos[indiceLibre].comando[sizeof(listaHijos[indiceLibre].comando) - 1] = '\0';
    strncpy(listaHijos[indiceLibre].estado, "Ejecutando", sizeof(listaHijos[indiceLibre].estado) - 1);
    listaHijos[indiceLibre].activo = true;

    printf("[%d] %d\n", indiceLibre + 1, listaHijos[indiceLibre].pid);

    return indiceLibre;
}
void builtin_jobs(void) {
    pid_t finished_pid;
    int status;

    //Verifica que un proceso hijo terminó para liberar el espacio de la lista
    while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAXPROCESS; i++) {
            if (listaHijos[i].pid == finished_pid) {
                listaHijos[i].activo = false; // Lo marcamos como inactivo
            } 
        }
    }

    //Simplemente recorre y muestra la información
    for (int i = 0; i < MAXPROCESS; i++) {
        if (listaHijos[i].activo == true) {
            printf("[%d] %-12s %s\n", i + 1, listaHijos[i].estado, listaHijos[i].comando);
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
    int argumentCount = 0;
    bool background = false;
    pid_t pid;

    if (args == NULL || args[0] == NULL) {
        return;
    }

    while (args[argumentCount] != NULL) {argumentCount++;} //Cuenta la cantidad de argumentos ingresados

    if (argumentCount > 0 && strcmp(args[argumentCount-1], "&") == 0) //Si el elemento final es '&' dejar el proceso en background
    {
        background = true; //flag para dejar en background
        args[argumentCount-1] = NULL; //Eliminamos el '&' en los argumentos (pues da error en el excvp) y lo dejamos como NULL
    }

    pid = fork();
    if (pid == -1) {
        perror("Error en fork");
        return;
    }

    if (pid == 0) {
        struct sigaction sa_dfl;
        sa_dfl.sa_handler = SIG_DFL;
        sigemptyset(&sa_dfl.sa_mask);
        sa_dfl.sa_flags = 0;
        sigaction(SIGINT, &sa_dfl, NULL);

        verificarRedireccion(args); //Aplica la redirección de entrada/salida si es necesario

        execvp(args[0], args);
        perror("Error al ejecutar el comando");
        exit(EXIT_FAILURE);
    }
    if(background){ //Si el proceso es background, agregar a tabla de background, sino, esperar por él.
        char full_cmd[256] = "";
        for (int k = 0; args[k] != NULL; k++) {
            strncat(full_cmd, args[k], sizeof(full_cmd) - strlen(full_cmd) - 1);
            if (args[k + 1] != NULL) {
                strncat(full_cmd, " ", sizeof(full_cmd) - strlen(full_cmd) - 1);
            }
        }
        agregarProceso(pid, full_cmd);
    } 
    else{waitpid(pid, NULL, 0);}
}

void executePipelineCommand(char ***commands, int commandCount) {
    int argumentCount = 0;
    bool background = false;
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
        background = true; //flag para dejar en background
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
            struct sigaction sa_dfl;
            sa_dfl.sa_handler = SIG_DFL;
            sigemptyset(&sa_dfl.sa_mask);
            sa_dfl.sa_flags = 0;
            sigaction(SIGINT, &sa_dfl, NULL);
            
            verificarRedireccion(commands[i]); //Aplica la redirección de entrada/salida si es necesario

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
                char full_cmd[256] = "";
                for (int k = 0; commands[i][k] != NULL; k++) {
                    strncat(full_cmd, commands[i][k], sizeof(full_cmd) - strlen(full_cmd) - 1);
                    if (commands[i][k + 1] != NULL) {
                        strncat(full_cmd, " ", sizeof(full_cmd) - strlen(full_cmd) - 1);
                    }
                }
                agregarProceso(pids[i], full_cmd);
            }
        }
    }
    free(pids);

}

void verificarRedireccion(char **args) {
    int i = 0, j = 0;
    char *inputFile = NULL;
    char *outputFile = NULL;
    bool appendMode = false;

    while (args[i] != NULL) {
        if (strcmp(args[i], "<") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: falta archivo para redirección de entrada\n");
                exit(EXIT_FAILURE);
            }
            inputFile = args[i + 1];
            i += 2;
            continue;
        }

        if (strcmp(args[i], ">") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: falta archivo para redirección de salida\n");
                exit(EXIT_FAILURE);
            }
            outputFile = args[i + 1];
            appendMode = false;
            i += 2;
            continue;
        }

        if (strcmp(args[i], ">>") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: falta archivo para redirección de salida\n");
                exit(EXIT_FAILURE);
            }
            outputFile = args[i + 1];
            appendMode = true;
            i += 2;
            continue;
        }

        args[j++] = args[i++];
    }

    args[j] = NULL;

    if (inputFile != NULL) {
        int fd = open(inputFile, O_RDONLY);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (outputFile != NULL) {

        int flags = O_WRONLY | O_CREAT;

        if (appendMode) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }

        int fd = open(outputFile, flags, 0644);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
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
// Variables para avisar cuando suena la alarma o cuando se presiona ctrl+c
static volatile sig_atomic_t pmon_tick = 0;
static volatile sig_atomic_t pmon_exit = 0;

static void manejador_pmon_alrm(int sig) {
    (void)sig;
    pmon_tick = 1;
}

// Se ejecuta si el usuario presiona ctrl+c dentro del pmon
static void manejador_pmon_sigint(int sig) {
    (void)sig;
    pmon_exit = 1;
}

// Estructura para recordar los datos anteriores de cada proceso y calcular el %cpu
typedef struct {
    pid_t pid;
    unsigned long long prev_total_time;
    struct timeval prev_timestamp;
    bool tracked;
} CpuTracker;

static CpuTracker trackers[MAXPROCESS];

static const char *describir_estado(char state_char) {
    switch (state_char) {
        case 'R': return "ejecutando";
        case 'S': return "durmiendo";
        case 'D': return "durmiendo";
        case 'Z': return "zombie";
        case 'T': return "detenido";
        default:  return "desconocido";
    }
}

// Lee los archivos de texto que linux crea para cada proceso en /proc
static bool obtener_datos_proc(pid_t pid, char *estado_str, unsigned long long *cpu_time, long *rss_kb) {
    char ruta[64];
    FILE *fp;

    snprintf(ruta, sizeof(ruta), "/proc/%d/stat", pid);
    fp = fopen(ruta, "r");
    if (!fp) {
        return false;
    }

    char stat_buffer[1024];
    if (fgets(stat_buffer, sizeof(stat_buffer), fp) == NULL) {
        fclose(fp);
        return false;
    }
    fclose(fp);

    char *cierre_parentesis = strrchr(stat_buffer, ')');
    if (!cierre_parentesis) {
        return false;
    }

    char state_char;
    unsigned long utime = 0, stime = 0;
    sscanf(cierre_parentesis + 2,
           "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
           &state_char, &utime, &stime);

    strcpy(estado_str, describir_estado(state_char));
    *cpu_time = (unsigned long long)(utime + stime);

    *rss_kb = 0;
    snprintf(ruta, sizeof(ruta), "/proc/%d/status", pid);
    fp = fopen(ruta, "r");
    if (fp) {
        char linea[256];
        while (fgets(linea, sizeof(linea), fp)) {
            if (strncmp(linea, "VmRSS:", 6) == 0) {
                sscanf(linea + 6, "%ld", rss_kb);
                break;
            }
        }
        fclose(fp);
    }

    return true;
}

// Función pmon monitor de procesos
void builtin_pmon(char **args) {
    int intervalo = 2;
    if (args[1] != NULL) {
        int segs = atoi(args[1]);
        if (segs > 0) {
            intervalo = segs;
        }
    }

    struct sigaction sa_alrm, sa_int, old_alrm, old_int;

    // Se prepara el receptor para la alarma periódica
    memset(&sa_alrm, 0, sizeof(sa_alrm));
    sa_alrm.sa_handler = manejador_pmon_alrm;
    sigemptyset(&sa_alrm.sa_mask);
    sa_alrm.sa_flags = 0;
    sigaction(SIGALRM, &sa_alrm, &old_alrm);

    // Se prepara el receptor para capturar ctrl+c sin cerrar la shell
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = manejador_pmon_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, &old_int);

    // Se limpian datos viejos del cpu
    memset(trackers, 0, sizeof(trackers));
    long clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0) clk_tck = 100;

    pmon_exit = 0;
    pmon_tick = 1;

    while (!pmon_exit) {
        if (pmon_tick) {
            pmon_tick = 0;

            printf("\033[H\033[J");
            printf("%-8s | %-20s | %-12s | %-12s | %-8s\n", 
                   "PID", "COMANDO", "ESTADO", "%CPU (aprox)", "RSS (KB)");

            struct timeval now;
            gettimeofday(&now, NULL);

            for (int i = 0; i < MAXPROCESS; i++) {
                if (!listaHijos[i].activo) {
                    trackers[i].tracked = false;
                    continue;
                }

                char estado_str[32] = {0};
                unsigned long long cpu_time = 0;
                long rss_kb = 0;

                if (!obtener_datos_proc(listaHijos[i].pid, estado_str, &cpu_time, &rss_kb)) {
                    listaHijos[i].activo = false;
                    trackers[i].tracked = false;
                    continue;
                }

                double cpu_percent = 0.0;
                if (trackers[i].tracked && trackers[i].pid == listaHijos[i].pid) {
                    double delta_time_sec = (now.tv_sec - trackers[i].prev_timestamp.tv_sec) +
                                           (now.tv_usec - trackers[i].prev_timestamp.tv_usec) / 1000000.0;
                    unsigned long long delta_cpu = (cpu_time >= trackers[i].prev_total_time) ? 
                                                   (cpu_time - trackers[i].prev_total_time) : 0;

                    if (delta_time_sec > 0.0) {
                        cpu_percent = ((double)delta_cpu / clk_tck) / delta_time_sec * 100.0;
                    }
                }

                trackers[i].pid = listaHijos[i].pid;
                trackers[i].prev_total_time = cpu_time;
                trackers[i].prev_timestamp = now;
                trackers[i].tracked = true;

                printf("%-8d | %-20.20s | %-12s | %-12.1f | %-8ld\n",
                       listaHijos[i].pid,
                       listaHijos[i].comando,
                       estado_str,
                       cpu_percent,
                       rss_kb);
            }
            fflush(stdout);

            alarm(intervalo);
        }

        pause();
    }

    alarm(0);

    sigaction(SIGALRM, &old_alrm, NULL);
    sigaction(SIGINT, &old_int, NULL);

    printf("\n");
}