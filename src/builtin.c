#include "builtin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Returns 1 if the command is a builtin, 0 otherwise
int if_builtin(char *input) {
  // Extract the command name (first word before any space)
  char cmd[256];
  int i = 0;
  while (input[i] != '\0' && input[i] != ' ' && i < 255) {
    cmd[i] = input[i];
    i++;
  }
  cmd[i] = '\0';

  // Check against known builtins
  if (strcmp(cmd, "exit") == 0)
    return 1;
  if (strcmp(cmd, "echo") == 0)
    return 1;
  return 0; // Not a builtin
}
// ========== exit builtin ==========
int cmd_exit(char *input) {
  int code = 0;
  // If there's an argument after "exit ", parse it as the exit code
  if (input[4] == ' ') {
    code = atoi(input + 5);
  }
  exit(code);
  return code; // Never reached, but keeps the compiler happy
}
// ========== echo builtin ==========
void cmd_echo(char *input) {
  char *arg = input + 5;
  printf("%s\n", arg);
}