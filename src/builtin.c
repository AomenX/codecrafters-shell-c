// builtin.c — Built-in commands: exit, echo, type, pwd, cd.

#include "builtin.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int cmd_exit(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_type(int argc, char **argv);
static void cmd_pwd(void);
static int cmd_cd(int argc, char **argv);

static const char *builtins_names[] = {"exit", "echo", "type", "pwd", "cd"};

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
  return 0;
}

// cd [path] — change directory; empty path or "~" uses $HOME.
// Updates OLDPWD and PWD when getcwd succeeds.
static int cmd_cd(int argc, char **argv) {
  char cwd[1024];
  const char *path = (argc > 1) ? argv[1] : "";

  if (strcmp(path, "~") == 0 || strcmp(path, "") == 0) {
    path = getenv("HOME");
  }

  if (getcwd(cwd, sizeof(cwd)))
    setenv("OLDPWD", cwd, 1);

  if (chdir(path) == -1) {
    fprintf(stderr, "cd: %s: No such file or directory\n", path);
    return -1;
  }

  if (getcwd(cwd, sizeof(cwd)))
    setenv("PWD", cwd, 1);

  return 0;
}

// exit [code] — terminate the shell (default status 0).
static int cmd_exit(int argc, char **argv) {
  int code = 0;
  if (argc > 1) {
    code = atoi(argv[1]);
  }
  exit(code);
  return code; // unreachable
}

// echo args... — print arguments separated by spaces, then newline.
static void cmd_echo(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (i > 1)
      printf(" ");
    printf("%s", argv[i]);
  }
  printf("\n");
}

// type name — report whether name is a builtin, external, or missing.
static void cmd_type(int argc, char **argv) {
  if (argc < 2) {
    return;
  }
  const char *arg = argv[1];

  for (int i = 0; i < (int)(sizeof(builtins_names) / sizeof(builtins_names[0]));
       i++) {
    if (strcmp(arg, builtins_names[i]) == 0) {
      printf("%s is a shell builtin\n", arg);
      return;
    }
  }

  char *path = find_in_path(arg);
  if (path != NULL) {
    printf("%s is %s\n", arg, path);
    free(path);
    return;
  }

  printf("%s: not found\n", arg);
}

// pwd — print the current working directory.
static void cmd_pwd(void) {
  char *path = getcwd(NULL, 0);
  if (path != NULL) {
    printf("%s\n", path);
    free(path);
  }
}
