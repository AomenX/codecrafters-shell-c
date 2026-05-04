#include "builtin.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Forward declarations (defined below)
static int cmd_exit(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_type(int argc, char **argv);
static void cmd_pwd(void);
static int cmd_cd(int argc, char **argv);

// All builtin names — used by cmd_type to identify builtins
static const char *builtins_names[] = {"exit", "echo", "type", "pwd", "cd"};

// Checks if argv[0] is a builtin, and if so, executes it.
// Returns 1 if handled, 0 otherwise.
int exec_builtin(int argc, char **argv) {
  if (argc == 0)
    return 0;

  const char *cmd = argv[0];

  if (strcmp(cmd, "exit") == 0) {
    cmd_exit(argc, argv);
    return 1;
  } else if (strcmp(cmd, "echo") == 0) {
    cmd_echo(argc, argv);
    return 1;
  } else if (strcmp(cmd, "type") == 0) {
    cmd_type(argc, argv);
    return 1;
  } else if (strcmp(cmd, "pwd") == 0) {
    cmd_pwd();
    return 1;
  } else if (strcmp(cmd, "cd") == 0) {
    cmd_cd(argc, argv);
    return 1;
  }
  return 0; // Not a builtin
}

// ========== cd builtin ==========
static int cmd_cd(int argc, char **argv) {
  char cwd[1024];
  const char *path = (argc > 1) ? argv[1] : "";

  if (strcmp(path, "~") == 0 || strcmp(path, "") == 0) {
    path = getenv("HOME");
  }
  // save old path
  if (getcwd(cwd, sizeof(cwd)))
    setenv("OLDPWD", cwd, 1);

  if (chdir(path) == -1) {
    fprintf(stderr, "cd: %s: No such file or directory\n", path);
    return -1;
  }
  // save new path
  if (getcwd(cwd, sizeof(cwd)))
    setenv("PWD", cwd, 1);

  return 0;
}

// ========== exit builtin ==========
static int cmd_exit(int argc, char **argv) {
  int code = 0;
  if (argc > 1) {
    code = atoi(argv[1]);
  }
  exit(code);
  return code; // Never reached, but keeps the compiler happy
}

// ========== echo builtin ==========
static void cmd_echo(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (i > 1)
      printf(" ");
    printf("%s", argv[i]);
  }
  printf("\n");
}

// ========== type builtin ==========
// Identifies a command as: builtin → external (in PATH) → not found
static void cmd_type(int argc, char **argv) {
  if (argc < 2) {
    return;
  }
  const char *arg = argv[1];

  // 1. Check if it's a builtin
  for (int i = 0; i < (int)(sizeof(builtins_names) / sizeof(builtins_names[0]));
       i++) {
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

// ========== pwd builtin ==========
static void cmd_pwd(void) {
  char *path = getcwd(NULL, 0);
  if (path != NULL) {
    printf("%s\n", path);
    free(path);
  }
}