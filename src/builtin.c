// builtin.c — Built-in commands: exit, echo, type, pwd, history.

#include "builtin.h"
#include "path.h"
#include "parse.h"
#include "redirect.h"
#include "exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int cmd_exit(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_type(int argc, char **argv);
static void cmd_pwd(void);
static int cmd_cd(int argc, char **argv);
static void cmd_history(int argc, char **argv);

static const char *builtins_names[] = {"exit", "echo", "type", "pwd", "cd", "history"};

// History storage
static char *history_entries[MAX_HISTORY_SIZE];
static int history_count = 0;
static int history_pos = 0;
static int history_initialized = 0;
static int last_saved_index = 0;  // Track last command index saved to file

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
  } else if (strcmp(cmd, "history") == 0) {
    cmd_history(argc, argv);
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

  const char *histfile = getenv("HISTFILE");
  if (histfile != NULL && strlen(histfile) > 0) {
    write_history_to_file(histfile);
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

// History management functions

void init_history(void) {
  if (!history_initialized) {
    for (int i = 0; i < MAX_HISTORY_SIZE; i++) {
      history_entries[i] = NULL;
    }
    history_count = 0;
    history_pos = 0;
    history_initialized = 1;
  }
}

void add_to_history(const char *cmd) {
  if (!history_initialized) {
    init_history();
  }
  
  // Don't add empty commands or duplicates of the last command
  if (cmd == NULL || strlen(cmd) == 0) {
    return;
  }
  if (history_count > 0 && strcmp(cmd, history_entries[(history_count - 1) % MAX_HISTORY_SIZE]) == 0) {
    return;
  }
  
  // Add to history
  int index = history_count % MAX_HISTORY_SIZE;
  if (history_entries[index] != NULL) {
    free(history_entries[index]);
  }
  history_entries[index] = strdup(cmd);
  
  if (history_count < MAX_HISTORY_SIZE) {
    history_count++;
  }
  
  // Reset position to end when adding new command
  history_pos = history_count;
}

void list_history(void) {
  if (!history_initialized) {
    init_history();
  }
  
  int start = 0;
  if (history_count > 0) {
    start = (history_count > MAX_HISTORY_SIZE) ? (history_count - MAX_HISTORY_SIZE) : 0;
  }
  
  for (int i = start; i < history_count; i++) {
    int index = i % MAX_HISTORY_SIZE;
    if (history_entries[index] != NULL) {
      printf("%d  %s\n", i + 1, history_entries[index]);
    }
  }
}

int get_history_size(void) {
  return history_count;
}

void list_history_last(int limit) {
  if (!history_initialized) {
    init_history();
  }
  
  if (limit <= 0) {
    return;
  }
  
  int start = 0;
  if (history_count > limit) {
    start = history_count - limit;
  }
  
  for (int i = start; i < history_count; i++) {
    int index = i % MAX_HISTORY_SIZE;
    if (history_entries[index] != NULL) {
      printf("%5d  %s\n", i + 1, history_entries[index]);
    }
  }
}

const char *get_history_entry(int index) {
  if (index < 1 || index > history_count) {
    return NULL;
  }
  return history_entries[(index - 1) % MAX_HISTORY_SIZE];
}

void set_history_pos(int pos) {
  if (pos < 0) pos = 0;
  if (pos > history_count) pos = history_count;
  history_pos = pos;
}

int get_history_pos(void) {
  return history_pos;
}

void history_move_next(void) {
  if (history_pos < history_count) {
    history_pos++;
  }
}

void history_move_prev(void) {
  if (history_pos > 0) {
    history_pos--;
  }
}

void execute_history_command(int index) {
  if (index < 1 || index > history_count) {
    fprintf(stderr, "history command not found: %d\n", index);
    return;
  }
  
  const char *cmd = get_history_entry(index);
  if (cmd != NULL) {
    // Parse and execute the command
    char *argv[256];
    char *cmd_copy = strdup(cmd);
    int argc = parse_input(cmd_copy, argv);
    
    if (argc > 0) {
      if (!exec_builtin(argc, argv)) {
        // Not a builtin, try external
        char *redirect_file = NULL;
        argc = extract_redirect(argc, argv, &redirect_file);
        
        int saved_fd = apply_redirect(redirect_file);
        if (exec_external(argc, argv, redirect_file)) {
          restore_redirect(saved_fd);
        } else {
          restore_redirect(saved_fd);
          printf("%s: command not found\n", argv[0]);
        }
      }
    }
    
    free(cmd_copy);
    free_args(argc, argv);
  }
}

void read_history_from_file(const char *path) {
  if (path == NULL) {
    fprintf(stderr, "history: file path required\n");
    return;
  }
  
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    fprintf(stderr, "history: cannot open file: %s\n", path);
    return;
  }
  
  char line[MAX_HISTORY_LINE_LEN];
  while (fgets(line, sizeof(line), file) != NULL) {
    // Remove trailing newline
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }
    
    // Add non-empty lines to history
    if (strlen(line) > 0) {
      add_to_history(line);
    }
  }
  
  fclose(file);
  
  // Mark all current history as saved
  last_saved_index = history_count;
}

// cmd_history — handle history builtin command
static void cmd_history(int argc, char **argv) {
  if (!history_initialized) {
    init_history();
  }
  
  if (argc == 1) {
    // List all history
    list_history();
  } else if (argc == 2) {
    // Check if it's a number (history limit) or "history" command
    char *endptr;
    long num = strtol(argv[1], &endptr, 10);
    if (*endptr == '\0') {
      // It's a number - show last N entries
      list_history_last((int)num);
    } else if (argv[1][0] == '!') {
      // Handle !n syntax to execute commands
      long num = strtol(argv[1] + 1, &endptr, 10);
      if (*endptr == '\0') {
        execute_history_command((int)num);
      } else {
        fprintf(stderr, "history: invalid argument: %s\n", argv[1]);
      }
    } else {
      // Unknown argument
      fprintf(stderr, "history: invalid argument: %s\n", argv[1]);
    }
  } else if (argc == 3) {
    if (strcmp(argv[1], "-r") == 0) {
      // Read history from file: history -r <path>
      read_history_from_file(argv[2]);
    } else if (strcmp(argv[1], "-w") == 0) {
      // Write history to file: history -w <path>
      write_history_to_file(argv[2]);
    } else if (strcmp(argv[1], "-a") == 0) {
      // Append history to file: history -a <path>
      append_history_to_file(argv[2]);
    } else {
      fprintf(stderr, "history: invalid option: %s\n", argv[1]);
    }
  } else {
    fprintf(stderr, "history: too many arguments\n");
  }
}

void write_history_to_file(const char *path) {
  if (path == NULL) {
    fprintf(stderr, "history: file path required\n");
    return;
  }
  
  FILE *file = fopen(path, "w");
  if (file == NULL) {
    fprintf(stderr, "history: cannot open file for writing: %s\n", path);
    return;
  }
  
  // Write all history entries to file
  int start = 0;
  if (history_count > 0) {
    start = (history_count > MAX_HISTORY_SIZE) ? (history_count - MAX_HISTORY_SIZE) : 0;
  }
  
  for (int i = start; i < history_count; i++) {
    int index = i % MAX_HISTORY_SIZE;
    if (history_entries[index] != NULL) {
      fprintf(file, "%s\n", history_entries[index]);
    }
  }
  
  fclose(file);
  
  // Mark all current history as saved
  last_saved_index = history_count;
}

void append_history_to_file(const char *path) {
  if (path == NULL) {
    fprintf(stderr, "history: file path required\n");
    return;
  }
  
  FILE *file = fopen(path, "a");
  if (file == NULL) {
    fprintf(stderr, "history: cannot open file for appending: %s\n", path);
    return;
  }
  
  // Append only new commands (from last_saved_index onwards)
  for (int i = last_saved_index; i < history_count; i++) {
    int index = i % MAX_HISTORY_SIZE;
    if (history_entries[index] != NULL) {
      fprintf(file, "%s\n", history_entries[index]);
    }
  }
  
  fclose(file);
  
  // Mark all current history as saved
  last_saved_index = history_count;
}
