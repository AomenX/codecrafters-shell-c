#include "builtin.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Forward declarations (defined below)
static int cmd_exit(char *input);
static void cmd_echo(char *input);
static void cmd_type(char *input);

// All builtin names — used by cmd_type to identify builtins
static const char *builtins_names[] = {"exit", "echo", "type"};

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
  } else if (strcmp(cmd, "echo") == 0) {
    cmd_echo(input);
    return 1;
  } else if (strcmp(cmd, "type") == 0) {
    cmd_type(input);
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
  if (input[4] == '\0') {
    printf("\n");
  }
  printf("%s\n", input + 5);
}

// ========== type builtin ==========
// Identifies a command as: builtin → external (in PATH) → not found
static void cmd_type(char *input) {
  char *arg = input + 5;
  int i;

  // 1. Check if it's a builtin
  for (i = 0; i < sizeof(builtins_names) / sizeof(builtins_names[0]); i++) {
    if (strcmp(arg, builtins_names[i]) == 0) {
      printf("%s is a shell builtin\n", arg);
      return;
    }
  }

  // 2. Search PATH for an external executable
  char *path = find_in_path(arg);
  if (path != NULL) {
    printf("%s is %s\n", arg, path);
    free(path);
    return;
  }

  // 3. Not found anywhere
  printf("%s: not found\n", arg);
}