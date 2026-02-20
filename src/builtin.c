#include "builtin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations (defined below)
static int cmd_exit(char *input);
static void cmd_echo(char *input);

// Checks if the command is a builtin, and if so, executes it.
// Returns 1 if handled, 0 otherwise.
int exec_builtin(char *input) {
  // Extract the command name (first word before any space)
  char cmd[256];
  int i = 0;
  while (input[i] != '\0' && input[i] != ' ' && i < 255) {
    cmd[i] = input[i];
    i++;
  }
  cmd[i] = '\0';

  // Check and dispatch to the correct builtin
  if (strcmp(cmd, "exit") == 0) {
    cmd_exit(input);
    return 1;
  }
  if (strcmp(cmd, "echo") == 0) {
    cmd_echo(input);
    return 1;
  }
  return 0; // Not a builtin
}
// ========== exit builtin ==========
static int cmd_exit(char *input) {
  int code = 0;
  // If there's an argument after "exit ", parse it as the exit code
  if (input[4] == ' ') {
    code = atoi(input + 5);
  }
  exit(code);
  return code; // Never reached, but keeps the compiler happy
}
// ========== echo builtin ==========
static void cmd_echo(char *input) {
  char *arg = input + 5;
  printf("%s\n", arg);
}