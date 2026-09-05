#ifndef FUNCIONES_H
#define FUNCIONES_H

char **parseLine(const char *line, char delimiter, int *count);
void freeTokens(char **tokens, int count);

void executeNormalCommand(char **args);
void executePipelineCommand(char ***commands, int commandCount);

#endif