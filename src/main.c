// main.c — REPL entry point: read a line, parse, redirect, dispatch builtins/externals.

#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"
#include "exec.h"
#include "parse.h"
#include "pipe.h"
#include "redirect.h"

int main(int argc_unused, char *argv_unused[]) {
  (void)argc_unused;
  (void)argv_unused;

  char input[1024];
  char *argv[256];

  while (1) {
    fflush(stdout);

    char *line = readline("$ ");
    if (line == NULL) {
      break; // EOF (Ctrl+D)
    }

    strncpy(input, line, sizeof(input) - 1);
    input[sizeof(input) - 1] = '\0';
    free(line);

    int argc = parse_input(input, argv);
    if (argc == 0)
      continue;

    int has_pipe = 0;
    for (int i = 0; i < argc; i++) {
      if (strcmp(argv[i], "|") == 0) {
        has_pipe = 1;
        break;
      }
    }

    if (has_pipe) {
      execute_pipeline(argv);
      free_args(argc, argv);
      continue;
    }

    char *redirect_file = NULL;
    argc = extract_redirect(argc, argv, &redirect_file);

    // Builtins run in-process; redirect stdout/stderr here before dispatch.
    int saved_fd = apply_redirect(redirect_file);

    if (exec_builtin(argc, argv)) {
      restore_redirect(saved_fd);
      free_args(argc, argv);
      continue;
    }

    // Externals fork a child that applies redirect itself; restore parent fds first.
    restore_redirect(saved_fd);

    if (exec_external(argc, argv, redirect_file)) {
      free_args(argc, argv);
      continue;
    }

    printf("%s: command not found\n", argv[0]);
    free_args(argc, argv);
  }
  return 0;
}
