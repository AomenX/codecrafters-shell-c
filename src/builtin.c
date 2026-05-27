// builtin.c — Shell built-in commands executed in the parent process (no fork).
//
// All builtins are dispatched from exec_builtin().  Because they run directly in
// the shell process rather than a forked child, they can mutate process state
// (working directory, environment variables, history, job table) and share memory
// with the REPL.
//
// Modules contained here:
//   • Shell variable store  — declare/get_shell_var (separate from the environment)
//   • Built-in commands     — exit, echo, type, pwd, cd, history, complete, declare, jobs
//   • History subsystem     — ring-buffer storage, file I/O, readline navigation glue
//   • Completion registry   — maps command names to completer script paths

#include "builtin.h"
#include "path.h"
#include "parse.h"
#include "redirect.h"
#include "exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

// ---------------------------------------------------------------------------
// Forward declarations for static builtin handlers
// ---------------------------------------------------------------------------
static int  cmd_exit(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_type(int argc, char **argv);
static void cmd_pwd(void);
static int  cmd_cd(int argc, char **argv);
static void cmd_history(int argc, char **argv);
static void cmd_complete(int argc, char **argv);
static void cmd_declare(int argc, char **argv);
static void cmd_jobs(int argc, char **argv);

// Table of builtin names; used by cmd_type to identify shell builtins.
static const char *builtins_names[] = {
  "exit", "echo", "type", "pwd", "cd",
  "history", "complete", "declare", "jobs"
};

// ---------------------------------------------------------------------------
// Shell variable store
//
// Shell variables are distinct from environment variables.  They are set with
// `declare name=value` and expanded via $name or ${name} in the parser.
// They are not inherited by child processes.
// ---------------------------------------------------------------------------
#define MAX_SHELL_VARS 256
static struct {
  char *name;
  char *value;
} shell_vars[MAX_SHELL_VARS];
static int num_shell_vars = 0;

// get_shell_var — look up a shell variable by name.
// Returns the value string or NULL if the variable is not set.
const char *get_shell_var(const char *name) {
  for (int i = 0; i < num_shell_vars; i++) {
    if (strcmp(shell_vars[i].name, name) == 0)
      return shell_vars[i].value;
  }
  return NULL;
}

// set_shell_var — create or update a shell variable.
// If the variable already exists its value is replaced; otherwise a new entry
// is appended.  Strings are heap-allocated so old values are freed on update.
static void set_shell_var(const char *name, const char *value) {
  for (int i = 0; i < num_shell_vars; i++) {
    if (strcmp(shell_vars[i].name, name) == 0) {
      free(shell_vars[i].value);
      shell_vars[i].value = strdup(value);
      return;
    }
  }
  if (num_shell_vars < MAX_SHELL_VARS) {
    shell_vars[num_shell_vars].name  = strdup(name);
    shell_vars[num_shell_vars].value = strdup(value);
    num_shell_vars++;
  }
}

// is_valid_identifier — return 1 if name is a legal shell variable identifier
// (starts with a letter or underscore, followed by letters, digits, or underscores).
static int is_valid_identifier(const char *name) {
  if (name == NULL || name[0] == '\0')
    return 0;
  if (!isalpha((unsigned char)name[0]) && name[0] != '_')
    return 0;
  for (int i = 1; name[i] != '\0'; i++) {
    if (!isalnum((unsigned char)name[i]) && name[i] != '_')
      return 0;
  }
  return 1;
}

// ---------------------------------------------------------------------------
// History subsystem
//
// History is stored in a fixed-size ring buffer of MAX_HISTORY_SIZE entries.
// history_count is the total number of commands ever added (not capped at
// MAX_HISTORY_SIZE), so the display index matches what the user expects.
// The actual storage index for entry i is: i % MAX_HISTORY_SIZE.
//
// last_saved_index tracks how many entries have been written to $HISTFILE so
// that append_history_to_file() only writes commands added since the last save.
// ---------------------------------------------------------------------------
static char *history_entries[MAX_HISTORY_SIZE];
static int   history_count       = 0;
static int   history_pos         = 0;   // cursor for up/down-arrow navigation
static int   history_initialized = 0;
static int   last_saved_index    = 0;   // index of the last entry persisted to disk

// ---------------------------------------------------------------------------
// exec_builtin — dispatch table entry point
//
// Checks whether argv[0] names a known builtin and runs it.
// Returns 1 if the command was handled, 0 if it is not a builtin.
// ---------------------------------------------------------------------------
int exec_builtin(int argc, char **argv) {
  if (argc == 0)
    return 0;

  const char *cmd = argv[0];

  if      (strcmp(cmd, "exit")     == 0) { cmd_exit(argc, argv);    return 1; }
  else if (strcmp(cmd, "echo")     == 0) { cmd_echo(argc, argv);    return 1; }
  else if (strcmp(cmd, "type")     == 0) { cmd_type(argc, argv);    return 1; }
  else if (strcmp(cmd, "pwd")      == 0) { cmd_pwd();               return 1; }
  else if (strcmp(cmd, "cd")       == 0) { cmd_cd(argc, argv);      return 1; }
  else if (strcmp(cmd, "history")  == 0) { cmd_history(argc, argv); return 1; }
  else if (strcmp(cmd, "complete") == 0) { cmd_complete(argc, argv);return 1; }
  else if (strcmp(cmd, "declare")  == 0) { cmd_declare(argc, argv); return 1; }
  else if (strcmp(cmd, "jobs")     == 0) { cmd_jobs(argc, argv);    return 1; }

  return 0;
}

// ---------------------------------------------------------------------------
// cd [path]
//
// Changes the working directory.  An empty path or "~" resolves to $HOME.
// Updates OLDPWD (previous directory) and PWD (current directory) in the
// environment so that `cd -` style patterns and $PWD references work correctly.
// ---------------------------------------------------------------------------
static int cmd_cd(int argc, char **argv) {
  char cwd[1024];
  const char *path = (argc > 1) ? argv[1] : "";

  // Treat bare "~" and empty path as the home directory.
  if (strcmp(path, "~") == 0 || strcmp(path, "") == 0)
    path = getenv("HOME");

  // Snapshot the current directory before changing so we can update OLDPWD.
  if (getcwd(cwd, sizeof(cwd)))
    setenv("OLDPWD", cwd, 1);

  if (chdir(path) == -1) {
    fprintf(stderr, "cd: %s: No such file or directory\n", path);
    return -1;
  }

  // Re-read the cwd after the change (chdir may have resolved symlinks).
  if (getcwd(cwd, sizeof(cwd)))
    setenv("PWD", cwd, 1);

  return 0;
}

// ---------------------------------------------------------------------------
// jobs
//
// Lists all background jobs currently in the job table.
// Delegates to list_jobs() in exec.c which also handles status polling and
// removal of completed entries.
// ---------------------------------------------------------------------------
static void cmd_jobs(int argc, char **argv) {
  (void)argc;
  (void)argv;
  list_jobs();
}

// ---------------------------------------------------------------------------
// declare [name=value | -p name]
//
// Without -p: parse "name=value" and store the shell variable.
// With -p:    print the current value of the named variable in a format
//             that could be re-evaluated ("declare -- name=\"value\"").
// ---------------------------------------------------------------------------
static void cmd_declare(int argc, char **argv) {
  if (argc < 2) return;

  if (strcmp(argv[1], "-p") == 0) {
    // Print mode: require a variable name as the third argument.
    if (argc < 3) return;
    const char *name  = argv[2];
    const char *value = get_shell_var(name);
    if (value == NULL)
      printf("declare: %s: not found\n", name);
    else
      printf("declare -- %s=\"%s\"\n", name, value);

  } else {
    // Assignment mode: expect "name=value" as argv[1].
    const char *eq = strchr(argv[1], '=');
    if (eq != NULL) {
      size_t name_len = (size_t)(eq - argv[1]);
      char   name[256];
      if (name_len >= sizeof(name))
        name_len = sizeof(name) - 1;
      memcpy(name, argv[1], name_len);
      name[name_len] = '\0';

      if (!is_valid_identifier(name)) {
        printf("declare: `%s': not a valid identifier\n", argv[1]);
        return;
      }
      set_shell_var(name, eq + 1);
    }
  }
}

// ---------------------------------------------------------------------------
// exit [code]
//
// Save history to $HISTFILE (if set) then terminate the process.
// The exit status defaults to 0; any numeric argument overrides it.
// ---------------------------------------------------------------------------
static int cmd_exit(int argc, char **argv) {
  int code = (argc > 1) ? atoi(argv[1]) : 0;

  const char *histfile = getenv("HISTFILE");
  if (histfile != NULL && strlen(histfile) > 0)
    write_history_to_file(histfile);

  exit(code);
  return code; // unreachable; satisfies non-void return type
}

// ---------------------------------------------------------------------------
// echo [args...]
//
// Prints all arguments separated by single spaces, followed by a newline.
// No flag processing (e.g. no -n or -e); keeps things simple and portable.
// ---------------------------------------------------------------------------
static void cmd_echo(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (i > 1) printf(" ");
    printf("%s", argv[i]);
  }
  printf("\n");
}

// ---------------------------------------------------------------------------
// type <name>
//
// Reports the kind of command that name refers to:
//   • "name is a shell builtin"  — listed in builtins_names[]
//   • "name is /path/to/name"    — found via $PATH resolution
//   • "name: not found"          — neither of the above
// ---------------------------------------------------------------------------
static void cmd_type(int argc, char **argv) {
  if (argc < 2) return;
  const char *arg = argv[1];

  // Check the static builtin name table first.
  for (int i = 0; i < (int)(sizeof(builtins_names) / sizeof(builtins_names[0])); i++) {
    if (strcmp(arg, builtins_names[i]) == 0) {
      printf("%s is a shell builtin\n", arg);
      return;
    }
  }

  // Attempt PATH resolution.
  char *path = find_in_path(arg);
  if (path != NULL) {
    printf("%s is %s\n", arg, path);
    free(path);
    return;
  }

  printf("%s: not found\n", arg);
}

// ---------------------------------------------------------------------------
// pwd
//
// Prints the absolute path of the current working directory.
// Uses getcwd(NULL, 0) which allocates a suitably-sized buffer (POSIX extension).
// ---------------------------------------------------------------------------
static void cmd_pwd(void) {
  char *path = getcwd(NULL, 0);
  if (path != NULL) {
    printf("%s\n", path);
    free(path);
  }
}

// ===========================================================================
// History subsystem implementation
// ===========================================================================

// init_history — initialise the ring-buffer on first use.
// Safe to call multiple times; subsequent calls are no-ops.
void init_history(void) {
  if (!history_initialized) {
    for (int i = 0; i < MAX_HISTORY_SIZE; i++)
      history_entries[i] = NULL;
    history_count       = 0;
    history_pos         = 0;
    history_initialized = 1;
  }
}

// add_to_history — append a command string to the ring buffer.
//
// The ring buffer wraps at MAX_HISTORY_SIZE: once full, the oldest entry is
// overwritten.  history_count keeps counting upward so indices displayed to
// the user are always monotonically increasing across the session.
void add_to_history(const char *cmd) {
  if (!history_initialized)
    init_history();

  if (cmd == NULL || strlen(cmd) == 0)
    return;

  // Write into the slot at history_count % MAX_HISTORY_SIZE, freeing any
  // previously stored string to avoid a memory leak.
  int index = history_count % MAX_HISTORY_SIZE;
  if (history_entries[index] != NULL)
    free(history_entries[index]);
  history_entries[index] = strdup(cmd);

  // history_count tracks the absolute number of commands added, capped so we
  // never exceed the ring size (overflow would corrupt old indices).
  if (history_count < MAX_HISTORY_SIZE)
    history_count++;

  // After every insertion the navigation cursor sits at the "past-the-end"
  // position so the next up-arrow returns the most recent command.
  history_pos = history_count;
}

// list_history — print all entries in the ring buffer with right-aligned indices.
//
// Output format matches bash: "%5d  %s\n" where the number is the 1-based
// sequence index of that command in the overall history.
void list_history(void) {
  if (!history_initialized)
    init_history();

  // When the ring has wrapped, start is the oldest surviving entry.
  int start = (history_count > MAX_HISTORY_SIZE)
                ? (history_count - MAX_HISTORY_SIZE) : 0;

  for (int i = start; i < history_count; i++) {
    int index = i % MAX_HISTORY_SIZE;
    if (history_entries[index] != NULL)
      printf("%5d  %s\n", i + 1, history_entries[index]);
  }
}

// get_history_size — return the number of commands currently stored.
int get_history_size(void) {
  return history_count;
}

// list_history_last — print only the most recent `limit` history entries.
// Equivalent to `history N` in bash.
void list_history_last(int limit) {
  if (!history_initialized)
    init_history();
  if (limit <= 0)
    return;

  int start = (history_count > limit) ? (history_count - limit) : 0;

  for (int i = start; i < history_count; i++) {
    int index = i % MAX_HISTORY_SIZE;
    if (history_entries[index] != NULL)
      printf("%5d  %s\n", i + 1, history_entries[index]);
  }
}

// get_history_entry — retrieve a single history entry by 1-based index.
// Returns NULL if the index is out of range.
const char *get_history_entry(int index) {
  if (index < 1 || index > history_count)
    return NULL;
  return history_entries[(index - 1) % MAX_HISTORY_SIZE];
}

// set_history_pos / get_history_pos — manage the up/down-arrow navigation cursor.
void set_history_pos(int pos) {
  if (pos < 0)             pos = 0;
  if (pos > history_count) pos = history_count;
  history_pos = pos;
}
int get_history_pos(void) { return history_pos; }

// history_move_next / history_move_prev — advance or retreat the navigation cursor.
// Readline's key bindings call these when the user presses the up/down arrows.
void history_move_next(void) { if (history_pos < history_count) history_pos++; }
void history_move_prev(void) { if (history_pos > 0)             history_pos--; }

// execute_history_command — re-run the command at the given 1-based history index.
//
// The stored command string is duplicated, parsed into tokens, and dispatched
// through the same builtin/external path as normal interactive input.
void execute_history_command(int index) {
  if (index < 1 || index > history_count) {
    fprintf(stderr, "history command not found: %d\n", index);
    return;
  }

  const char *cmd = get_history_entry(index);
  if (cmd == NULL) return;

  char *argv[256];
  char *cmd_copy = strdup(cmd);
  int   argc     = parse_input(cmd_copy, argv);

  if (argc > 0)
    argc = expand_args(argc, argv);

  if (argc > 0) {
    if (!exec_builtin(argc, argv)) {
      // Not a builtin — attempt PATH resolution and exec.
      char *redirect_file = NULL;
      argc = extract_redirect(argc, argv, &redirect_file);

      int saved_fd = apply_redirect(redirect_file);
      if (exec_external(argc, argv, redirect_file, 0, NULL)) {
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

// ---------------------------------------------------------------------------
// History file I/O
// ---------------------------------------------------------------------------

// read_history_from_file — load history entries from a plain-text file.
//
// Each line becomes one history entry (newline stripped).  Called at startup
// when $HISTFILE is set so the shell resumes history across sessions.
// After loading, last_saved_index is updated so subsequent appends only write
// commands that were added after the file was read.
void read_history_from_file(const char *path) {
  if (path == NULL) {
    fprintf(stderr, "history: file path required\n");
    return;
  }

  FILE *file = fopen(path, "r");
  if (file == NULL) {
    // Silently ignore a missing history file — it may not exist yet.
    return;
  }

  char line[MAX_HISTORY_LINE_LEN];
  while (fgets(line, sizeof(line), file) != NULL) {
    // Strip the trailing newline that fgets retains.
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n')
      line[len - 1] = '\0';

    if (strlen(line) > 0)
      add_to_history(line);
  }

  fclose(file);

  // Mark all loaded entries as already-saved so append mode works correctly.
  last_saved_index = history_count;
}

// ---------------------------------------------------------------------------
// cmd_history — the `history` builtin handler
//
// Supported forms:
//   history              — list all entries
//   history N            — list the last N entries
//   history !N           — re-execute entry N
//   history -r <file>    — read history from file
//   history -w <file>    — overwrite file with full history
//   history -a <file>    — append new entries to file
// ---------------------------------------------------------------------------
static void cmd_history(int argc, char **argv) {
  if (!history_initialized)
    init_history();

  if (argc == 1) {
    list_history();

  } else if (argc == 2) {
    char *endptr;
    long num = strtol(argv[1], &endptr, 10);

    if (*endptr == '\0') {
      // Pure numeric argument → show last N entries.
      list_history_last((int)num);
    } else if (argv[1][0] == '!') {
      // !N syntax → re-execute the Nth command.
      long n = strtol(argv[1] + 1, &endptr, 10);
      if (*endptr == '\0')
        execute_history_command((int)n);
      else
        fprintf(stderr, "history: invalid argument: %s\n", argv[1]);
    } else {
      fprintf(stderr, "history: invalid argument: %s\n", argv[1]);
    }

  } else if (argc == 3) {
    if      (strcmp(argv[1], "-r") == 0) read_history_from_file(argv[2]);
    else if (strcmp(argv[1], "-w") == 0) write_history_to_file(argv[2]);
    else if (strcmp(argv[1], "-a") == 0) append_history_to_file(argv[2]);
    else
      fprintf(stderr, "history: invalid option: %s\n", argv[1]);

  } else {
    fprintf(stderr, "history: too many arguments\n");
  }
}

// write_history_to_file — overwrite path with the complete current history.
//
// Writes every entry in the ring buffer in order from oldest to newest.
// Used on exit and by `history -w`.
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

  int start = (history_count > MAX_HISTORY_SIZE)
                ? (history_count - MAX_HISTORY_SIZE) : 0;

  for (int i = start; i < history_count; i++) {
    int index = i % MAX_HISTORY_SIZE;
    if (history_entries[index] != NULL)
      fprintf(file, "%s\n", history_entries[index]);
  }

  fclose(file);
  last_saved_index = history_count;
}

// append_history_to_file — append only the commands added since the last save.
//
// Used on exit and by `history -a`.  By tracking last_saved_index we avoid
// re-writing entries that were loaded from or already flushed to the file.
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

  // Only write entries that have been added since the last save.
  for (int i = last_saved_index; i < history_count; i++) {
    int index = i % MAX_HISTORY_SIZE;
    if (history_entries[index] != NULL)
      fprintf(file, "%s\n", history_entries[index]);
  }

  fclose(file);
  last_saved_index = history_count;
}

// ===========================================================================
// Programmable completion registry
//
// Stores a mapping of command name → completer script path registered via
//   complete -C <script> <command>
// The registry is a fixed-size flat array; for typical shell usage (tens of
// entries) this is more than sufficient without the overhead of a hash table.
// ===========================================================================

#define MAX_COMPLETIONS 100
static struct {
  char *cmd;     // command name (e.g. "git", "docker")
  char *script;  // absolute path to the completer script
} registered_completions[MAX_COMPLETIONS];
static int num_completions = 0;

// cmd_complete — implement the `complete` builtin.
//
//   complete -C <script> <cmd>  — register or update a completer for cmd
//   complete -p <cmd>           — print the registered spec, or an error message
//   complete -r <cmd>           — remove the registered spec
static void cmd_complete(int argc, char **argv) {
  if (argc < 2) return;

  if (strcmp(argv[1], "-p") == 0) {
    // --- Print mode ---
    if (argc < 3) return;
    const char *cmd = argv[2];
    for (int i = 0; i < num_completions; i++) {
      if (strcmp(registered_completions[i].cmd, cmd) == 0) {
        printf("complete -C '%s' %s\n", registered_completions[i].script, cmd);
        return;
      }
    }
    printf("complete: %s: no completion specification\n", cmd);

  } else if (strcmp(argv[1], "-r") == 0) {
    // --- Remove mode ---
    if (argc < 3) return;
    const char *cmd = argv[2];
    for (int i = 0; i < num_completions; i++) {
      if (strcmp(registered_completions[i].cmd, cmd) == 0) {
        free(registered_completions[i].cmd);
        free(registered_completions[i].script);
        // Compact the array: shift entries above i down by one.
        for (int j = i + 1; j < num_completions; j++)
          registered_completions[j - 1] = registered_completions[j];
        num_completions--;
        break;
      }
    }

  } else if (strcmp(argv[1], "-C") == 0) {
    // --- Register mode ---
    if (argc < 4) return;
    const char *script = argv[2];
    const char *cmd    = argv[3];

    // Update existing entry if one already exists for this command.
    for (int i = 0; i < num_completions; i++) {
      if (strcmp(registered_completions[i].cmd, cmd) == 0) {
        free(registered_completions[i].script);
        registered_completions[i].script = strdup(script);
        return;
      }
    }

    // Otherwise insert a new entry.
    if (num_completions < MAX_COMPLETIONS) {
      registered_completions[num_completions].cmd    = strdup(cmd);
      registered_completions[num_completions].script = strdup(script);
      num_completions++;
    }
  }
}

// get_completion_script — look up the completer script registered for cmd.
// Returns the script path or NULL if no spec is registered.
const char *get_completion_script(const char *cmd) {
  for (int i = 0; i < num_completions; i++) {
    if (strcmp(registered_completions[i].cmd, cmd) == 0)
      return registered_completions[i].script;
  }
  return NULL;
}
