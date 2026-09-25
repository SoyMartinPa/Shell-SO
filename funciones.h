#ifndef FUNCIONES_H
#define FUNCIONES_H

char **parseLine(const char *line, char delimiter, int *count);
void freeTokens(char **tokens, int count);

void executeNormalCommand(char **args);
void executePipelineCommand(char ***commands, int commandCount);
void builtin_cd(char **args);
int builtin_exit(char **args);
int agregarProceso(pid_t pid, const char *cmd);
void builtin_jobs(void);
void verificarRedireccion(char **args);
void manejador_sigint(int sig);
void builtin_pmon(char **args);
#endif