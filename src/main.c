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

int main(int argc_unused, char *argv_unused[]);

char *my_generator(const char *text, int state) {
  static int list_index, len;
  const char *builtins[] = {"echo", "exit", NULL};
  char *name;

  if (!state) {
    list_index = 0;
    len = strlen(text);
  }

  while ((name = (char *)builtins[list_index++])) {
    if (strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }
  return NULL;
}

char **my_completion(const char *text, int start, int end) {
  rl_attempted_completion_over = 1; // Don't fall back to default filename completion
  return rl_completion_matches(text, my_generator);
}

int main(int argc_unused, char *argv_unused[]) {
  (void)argc_unused;
  (void)argv_unused;

  rl_attempted_completion_function = my_completion;

  // Initialize history
  init_history();
  
  // Load history from HISTFILE environment variable if set
  char *histfile = getenv("HISTFILE");
  if (histfile != NULL && strlen(histfile) > 0) {
    read_history_from_file(histfile);
  }
  
  char input[1024];
  char *argv[256];

  while (1) {
    fflush(stdout);

    char *line = readline("$ ");
    if (line == NULL) {
      // EOF (Ctrl+D) - save history and exit
      if (histfile != NULL && strlen(histfile) > 0) {
        write_history_to_file(histfile);
      }
      break;
    }

    // Add non-empty line to history
    if (strlen(line) > 0) {
      add_to_history(line);
      add_history(line);
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

    // Handle exit command specially to save history
    if (strcmp(argv[0], "exit") == 0) {
      restore_redirect(saved_fd);
      free_args(argc, argv);
      // Save history to HISTFILE and exit
      if (histfile != NULL && strlen(histfile) > 0) {
        write_history_to_file(histfile);
      }
      exit(0);
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
  
  // Save history to HISTFILE on normal exit
  if (histfile != NULL && strlen(histfile) > 0) {
    write_history_to_file(histfile);
  }
  return 0;
}
