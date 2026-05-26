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

#include <dirent.h>
#include <sys/stat.h>

void my_display_matches(char **matches, int num_matches, int max_length) {
    printf("\n");
    for (int i = 1; i <= num_matches; i++) {
        struct stat st;
        if (stat(matches[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            printf("%s/  ", matches[i]);
        } else {
            printf("%s  ", matches[i]);
        }
    }
    printf("\n");
    rl_on_new_line();
    rl_redisplay();
}

char *my_generator(const char *text, int state) {
  static int list_index, len;
  const char *builtins[] = {"echo", "exit", "history", NULL}; // Add history as it was likely added previously
  static int search_phase; // 0 = builtins, 1 = PATH
  static char *path_copy = NULL;
  static char *path_token = NULL;
  static DIR *dir = NULL;
  
  if (!state) {
    list_index = 0;
    len = strlen(text);
    search_phase = 0;
    
    if (path_copy) {
        free(path_copy);
        path_copy = NULL;
    }
    if (dir) {
        closedir(dir);
        dir = NULL;
    }
  }

  if (search_phase == 0) {
    char *name;
    while ((name = (char *)builtins[list_index++])) {
      if (strncmp(name, text, len) == 0) {
        return strdup(name);
      }
    }
    search_phase = 1;
    
    char *path_env = getenv("PATH");
    if (path_env) {
        path_copy = strdup(path_env);
        path_token = strtok(path_copy, ":");
        if (path_token) {
            dir = opendir(path_token);
        }
    }
  }

  if (search_phase == 1) {
    while (path_token != NULL) {
        if (dir == NULL) {
            path_token = strtok(NULL, ":");
            if (path_token) {
                dir = opendir(path_token);
            }
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            if (strncmp(entry->d_name, text, len) == 0) {
                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s/%s", path_token, entry->d_name);
                if (access(full_path, X_OK) == 0) {
                    return strdup(entry->d_name);
                }
            }
        }
        
        closedir(dir);
        dir = NULL;
        
        path_token = strtok(NULL, ":");
        if (path_token) {
            dir = opendir(path_token);
        }
    }
  }

  if (path_copy) {
      free(path_copy);
      path_copy = NULL;
  }
  if (dir) {
      closedir(dir);
      dir = NULL;
  }
  
  return NULL;
}

char **my_completion(const char *text, int start, int end) {
  if (start == 0) {
    rl_attempted_completion_over = 1; // Don't fall back to default filename completion for commands
    return rl_completion_matches(text, my_generator);
  } else {
    rl_attempted_completion_over = 0; // Allow default filename completion for arguments
    return NULL; // Let readline handle it
  }
}

int main(int argc_unused, char *argv_unused[]) {
  (void)argc_unused;
  (void)argv_unused;

  rl_attempted_completion_function = my_completion;
  rl_completion_display_matches_hook = my_display_matches;

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
