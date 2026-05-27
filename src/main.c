// main.c — REPL entry point, GNU Readline integration, and tab-completion engine.
//
// Responsibilities:
//   - Initialise Readline and register completion/display hooks.
//   - Load and save command history from/to $HISTFILE.
//   - Run the main Read-Eval-Print Loop:
//       1. Reap completed background jobs before printing each prompt.
//       2. Read a line with readline(3), add it to history.
//       3. Parse the line into tokens (handles quotes, pipes, & suffix).
//       4. Dispatch: pipelines → execute_pipeline(); builtins → exec_builtin();
//          external commands → exec_external(); unknown → error message.
//   - Provide the Readline completion callback (my_completion) that handles:
//       • Command-position completion (builtins + PATH executables).
//       • Argument-position completion via registered completer scripts.
//       • Filename completion fall-through to Readline's built-in engine.

#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

#include "builtin.h"
#include "exec.h"
#include "parse.h"
#include "pipe.h"
#include "redirect.h"

// Forward-declare main so that functions defined above it can reference it if needed.
int main(int argc_unused, char *argv_unused[]);

// Forward declaration required by C99: split_and_sort_lines is defined later in this
// file but called from my_completion which appears earlier.
int split_and_sort_lines(char *buf, char **out, int max);

#include <dirent.h>
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// run_completer_script — invoke an external completer script and capture stdout
//
// Implements the Bash programmable-completion protocol:
//   - Sets COMP_LINE (the full current input line) and COMP_POINT (cursor position)
//     in the child's environment, matching the variables Bash exposes to completers.
//   - Passes three positional arguments: the command name, the current word being
//     completed, and the preceding word.
//   - Reads all stdout from the child into a dynamically-grown heap buffer.
//   - Strips the trailing newline so callers get clean candidate strings.
//
// Returns a heap-allocated string containing the script's output (caller must free),
// or NULL if the fork/exec failed or the script produced no output.
// ---------------------------------------------------------------------------
char *run_completer_script(const char *script_path, const char *cmd,
                           const char *current, const char *prev,
                           const char *comp_line, int comp_point) {
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    return NULL;
  }

  pid_t pid = fork();
  if (pid == 0) {
    // ---- Child: wire stdout to the pipe write-end then exec the script ----
    close(pipefd[0]);                      // close unused read-end
    dup2(pipefd[1], STDOUT_FILENO);        // redirect stdout → pipe
    close(pipefd[1]);

    // Expose completion context via environment variables (Bash compat).
    char comp_point_str[32];
    snprintf(comp_point_str, sizeof(comp_point_str), "%d", comp_point);
    setenv("COMP_LINE",  comp_line,       1);
    setenv("COMP_POINT", comp_point_str,  1);

    // Try executing the script directly (requires the executable bit to be set).
    // Fall back to /bin/sh if direct exec fails (e.g., the script is not +x).
    execl(script_path, script_path, cmd, current, prev, NULL);
    execl("/bin/sh", "sh", script_path,  cmd, current, prev, NULL);
    exit(1);

  } else if (pid > 0) {
    // ---- Parent: read all bytes from the pipe into a growing buffer ----
    close(pipefd[1]);   // close write-end; EOF will arrive when child exits

    size_t cap  = 1024;
    size_t used = 0;
    char *buffer = malloc(cap);
    if (buffer == NULL) {
      close(pipefd[0]);
      int status;
      waitpid(pid, &status, 0);
      return NULL;
    }

    while (1) {
      // Grow the buffer if it is nearly full.
      if (used + 1 >= cap) {
        size_t new_cap = cap * 2;
        char *new_buf  = realloc(buffer, new_cap);
        if (new_buf == NULL) {
          free(buffer);
          close(pipefd[0]);
          int status;
          waitpid(pid, &status, 0);
          return NULL;
        }
        buffer = new_buf;
        cap    = new_cap;
      }

      ssize_t n = read(pipefd[0], buffer + used, cap - used - 1);
      if (n <= 0)
        break;   // EOF or error
      used += (size_t)n;
    }

    close(pipefd[0]);
    int status;
    waitpid(pid, &status, 0);

    if (used > 0) {
      buffer[used] = '\0';
      // Strip trailing newline so callers receive a clean candidate string.
      size_t len = strlen(buffer);
      if (len > 0 && buffer[len - 1] == '\n')
        buffer[len - 1] = '\0';
      return buffer;
    }

    free(buffer);
  }

  return NULL;
}

// ---------------------------------------------------------------------------
// Readline completion hooks
// ---------------------------------------------------------------------------

// Global match list produced by my_completion; used by completer_generator.
static char **completer_matches = NULL;

// completer_generator — a Readline rl_completion_entry_function that simply
// returns the first (and only) entry in completer_matches on the initial call
// (state == 0) and NULL on subsequent calls.  Used when a completer script
// returns a single candidate so Readline will substitute it directly.
char *completer_generator(const char *text, int state) {
  (void)text;
  if (state == 0 && completer_matches && completer_matches[0]) {
    return strdup(completer_matches[0]);
  }
  return NULL;
}

// my_display_matches — custom display hook for Readline.
//
// Invoked instead of the default completion displayer when there are multiple
// matches.  Directories are shown with a trailing '/' so the user can tell
// them apart from plain files.  After printing, the prompt is redrawn.
//
// Guard: libedit (macOS) does not expose rl_completion_display_matches_hook,
// so this function and its registration are excluded on Apple platforms.
#ifndef __APPLE__
void my_display_matches(char **matches, int num_matches, int max_length) {
  (void)max_length;
  printf("\n");
  for (int i = 1; i <= num_matches; i++) {
    struct stat st;
    // Append '/' after directory names to match standard shell behaviour.
    if (matches[i] != NULL && stat(matches[i], &st) == 0 && S_ISDIR(st.st_mode)) {
      printf("%s/  ", matches[i]);
    } else {
      printf("%s  ", matches[i]);
    }
  }
  printf("\n");
  rl_on_new_line();
  rl_redisplay();
}
#endif

// ---------------------------------------------------------------------------
// my_generator — Readline rl_completion_entry_function for command-position
//
// Called repeatedly by rl_completion_matches() until it returns NULL.
// On the first call (state == 0) it resets all static state and begins
// iterating through two candidate sources in order:
//   Phase 0 — the hard-coded list of shell builtins.
//   Phase 1 — every executable file in each directory listed in $PATH.
//
// For PATH scanning we open one directory at a time with opendir/readdir and
// advance to the next directory when the current one is exhausted.  Each
// candidate is checked with access(..., X_OK) to confirm it is actually
// executable before being returned.
// ---------------------------------------------------------------------------
char *my_generator(const char *text, int state) {
  // Static state persists across repeated calls for the same completion event.
  static int   list_index;
  static int   len;
  static int   search_phase;  // 0 = builtins, 1 = PATH executables
  static char *path_copy  = NULL;
  static char *path_token = NULL;
  static DIR  *dir        = NULL;

  const char *builtins[] = {
    "exit", "echo", "type", "pwd", "cd",
    "history", "complete", "declare", "jobs", NULL
  };

  if (!state) {
    // --- Reset on the first call for a new completion event ---
    list_index   = 0;
    len          = strlen(text);
    search_phase = 0;

    if (path_copy) { free(path_copy); path_copy = NULL; }
    if (dir)       { closedir(dir);   dir       = NULL; }
  }

  // --- Phase 0: match against builtin names ---
  if (search_phase == 0) {
    char *name;
    while ((name = (char *)builtins[list_index++])) {
      if (strncmp(name, text, len) == 0)
        return strdup(name);
    }
    // Builtins exhausted — move to PATH scanning.
    search_phase = 1;
    char *path_env = getenv("PATH");
    if (path_env) {
      path_copy  = strdup(path_env);
      path_token = strtok(path_copy, ":");
      if (path_token)
        dir = opendir(path_token);
    }
  }

  // --- Phase 1: walk every directory in $PATH ---
  if (search_phase == 1) {
    while (path_token != NULL) {
      if (dir == NULL) {
        // Current directory could not be opened; advance to the next token.
        path_token = strtok(NULL, ":");
        if (path_token)
          dir = opendir(path_token);
        continue;
      }

      struct dirent *entry;
      while ((entry = readdir(dir)) != NULL) {
        // Skip the current- and parent-directory entries.
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
          continue;

        if (strncmp(entry->d_name, text, len) == 0) {
          // Confirm the file is actually executable before returning it.
          char full_path[1024];
          snprintf(full_path, sizeof(full_path), "%s/%s", path_token, entry->d_name);
          if (access(full_path, X_OK) == 0)
            return strdup(entry->d_name);
        }
      }

      // This directory is exhausted; close it and move to the next one.
      closedir(dir);
      dir        = NULL;
      path_token = strtok(NULL, ":");
      if (path_token)
        dir = opendir(path_token);
    }
  }

  // Cleanup static resources when the generator is fully exhausted.
  if (path_copy) { free(path_copy); path_copy = NULL; }
  if (dir)       { closedir(dir);   dir       = NULL; }

  return NULL;
}

// ---------------------------------------------------------------------------
// my_completion — Readline rl_attempted_completion_function
//
// Readline calls this function whenever the user presses TAB.
//
//   start == 0  →  completing the command word (first token on the line).
//                  Delegate entirely to my_generator (builtins + PATH).
//                  Set rl_attempted_completion_over = 1 to suppress Readline's
//                  default filename completion for the command position.
//
//   start > 0   →  completing an argument.
//     1. Tokenise the line buffer to identify:
//          • cmd_name   — the command being invoked.
//          • current_word — the token at the cursor.
//          • prev_word  — the token immediately before the cursor.
//     2. Look up whether a completer script is registered for cmd_name.
//     3. If one is found, fork the script, collect its output lines, sort them,
//        compute the longest common prefix (LCP), and return a Readline match list.
//     4. If no script is registered, fall through (return NULL) so Readline's
//        built-in filename completion runs as the default.
// ---------------------------------------------------------------------------
char **my_completion(const char *text, int start, int end) {
  if (start == 0) {
    // --- Command position: complete builtins and PATH executables only ---
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, my_generator);
  }

  // --- Argument position ---

  // Tokenise the current line buffer so we can identify the command name and
  // which word is at the cursor.
  const char *line = rl_line_buffer;
  char cmd_name[256]    = "";
  char current_word[256] = "";
  char prev_word[256]   = "";

  int i = 0, word_start = 0, word_end = 0, word_count = 0;
  char words[64][256];
  int len = strlen(line);

  // Simple whitespace-based tokeniser (no quote awareness needed here because
  // we only need positions, not the actual shell tokens).
  while (i <= len) {
    while (line[i] == ' ' || line[i] == '\t') i++;
    if (line[i] == '\0') break;
    word_start = i;
    while (line[i] && line[i] != ' ' && line[i] != '\t') i++;
    word_end = i;
    int wlen = word_end - word_start;
    if (wlen > 0 && word_count < 64) {
      strncpy(words[word_count], line + word_start, wlen);
      words[word_count][wlen] = '\0';
      word_count++;
    }
  }

  // First token is always the command name.
  if (word_count > 0) {
    strncpy(cmd_name, words[0], sizeof(cmd_name) - 1);
    cmd_name[sizeof(cmd_name) - 1] = '\0';
  }

  // Determine which token sits under the cursor (by matching raw line offsets).
  int cursor      = start;
  int cur_word_idx = -1;
  int pos          = 0;
  for (int w = 0; w < word_count; w++) {
    const char *p = strstr(line + pos, words[w]);
    if (!p) continue;
    int wstart = p - line;
    int wend   = wstart + strlen(words[w]);
    if (cursor >= wstart && cursor <= wend) {
      cur_word_idx = w;
      break;
    }
    pos = wend;
  }

  // If the cursor is past all tokens (trailing space), default to last word.
  if (cur_word_idx == -1 && word_count > 0)
    cur_word_idx = word_count - 1;

  if (cur_word_idx >= 0) {
    strncpy(current_word, words[cur_word_idx], sizeof(current_word) - 1);
    current_word[sizeof(current_word) - 1] = '\0';
    if (cur_word_idx > 0) {
      strncpy(prev_word, words[cur_word_idx - 1], sizeof(prev_word) - 1);
      prev_word[sizeof(prev_word) - 1] = '\0';
    }
  }

  // Check whether a programmable completer is registered for this command.
  const char *script = get_completion_script(cmd_name);
  if (script != NULL) {
    char *result = run_completer_script(
      script, cmd_name, current_word, prev_word,
      rl_line_buffer, end   // end is the cursor byte-offset (COMP_POINT)
    );

    if (result != NULL) {
      char *lines[128];
      char *result_copy = strdup(result);
      int   n           = split_and_sort_lines(result_copy, lines, 128);

      if (n > 0) {
        // Compute the longest common prefix (LCP) of all candidate lines.
        // Readline uses this to advance the input as far as possible before
        // the user needs to make a choice.
        const char *first   = lines[0];
        size_t      lcp_len = strlen(first);
        for (int j = 1; j < n && lcp_len > 0; j++) {
          size_t k = 0;
          while (k < lcp_len && lines[j][k] && first[k] && lines[j][k] == first[k])
            k++;
          lcp_len = k;
        }

        // Choose the replacement text to insert:
        //   • Single candidate  → replace with that candidate.
        //   • LCP longer than current input → advance to LCP.
        //   • Otherwise → keep current input (just list candidates).
        const char *replacement = text;
        size_t      text_len    = strlen(text);
        if (n == 1) {
          replacement = lines[0];
        } else if (lcp_len > text_len) {
          static char lcp_buf[256];
          if (lcp_len >= sizeof(lcp_buf))
            lcp_len = sizeof(lcp_buf) - 1;
          memcpy(lcp_buf, first, lcp_len);
          lcp_buf[lcp_len] = '\0';
          replacement = lcp_buf;
        }

        // Readline expects matches[0] = replacement text,
        // matches[1..n] = individual candidates, matches[n+1] = NULL.
        char **matches = (char **)malloc((n + 2) * sizeof(char *));
        matches[0] = strdup(replacement);
        for (int j = 0; j < n; j++)
          matches[j + 1] = strdup(lines[j]);
        matches[n + 1] = NULL;

        free(result);
        free(result_copy);
        rl_attempted_completion_over = 1;
        return matches;
      }

      free(result);
      free(result_copy);
    }
  }

  // No completer script found (or it produced no output): fall through to
  // Readline's built-in filename completion by returning NULL without setting
  // rl_attempted_completion_over.
  rl_attempted_completion_over = 0;
  return NULL;
}

// ---------------------------------------------------------------------------
// main — shell entry point
// ---------------------------------------------------------------------------
int main(int argc_unused, char *argv_unused[]) {
  (void)argc_unused;
  (void)argv_unused;

  // Register our custom completion function with Readline.
  rl_attempted_completion_function = my_completion;

  // Register the custom match-display hook so directories get a trailing '/'.
  // Excluded on macOS because libedit does not expose this hook.
#ifndef __APPLE__
  rl_completion_display_matches_hook = my_display_matches;
#endif

  // Initialise our in-memory history ring buffer.
  init_history();

  // If $HISTFILE is set, pre-populate history from that file.  This allows
  // the shell to resume history across sessions.
  char *histfile = getenv("HISTFILE");
  if (histfile != NULL && strlen(histfile) > 0) {
    read_history_from_file(histfile);
  }

  char  input[1024];
  char *argv[256];
  int   first_prompt = 1;  // Suppress reap_done_jobs on the very first iteration.

  while (1) {
    // Before each prompt (except the very first), check for completed background
    // jobs and print their Done notifications — matching bash/zsh behaviour.
    if (!first_prompt) {
      reap_done_jobs();
    }
    first_prompt = 0;

    fflush(stdout);

    // readline(3) prints the prompt, handles line-editing, and returns a
    // heap-allocated string.  It returns NULL on EOF (Ctrl+D).
    char *line = readline("$ ");
    if (line == NULL) {
      // EOF: save history and exit cleanly.
      if (histfile != NULL && strlen(histfile) > 0)
        write_history_to_file(histfile);
      break;
    }

    // Record every non-empty command in both our ring buffer and Readline's
    // own history list (the latter enables up/down-arrow navigation).
    if (strlen(line) > 0) {
      add_to_history(line);
      add_history(line);
    }

    // Copy into a fixed buffer (parse_input mutates the string in-place).
    strncpy(input, line, sizeof(input) - 1);
    input[sizeof(input) - 1] = '\0';
    free(line);

    int argc = parse_input(input, argv);
    if (argc == 0)
      continue;

    // Perform $VAR / ${VAR} substitution on every token.
    argc = expand_args(argc, argv);
    if (argc == 0)
      continue;

    // Check for a trailing '&' which means run the command in the background.
    int background = 0;
    if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
      free(argv[argc - 1]);
      argc--;
      argv[argc] = NULL;
      background  = 1;
    }

    // Check whether the token list contains a pipe operator.
    int has_pipe = 0;
    for (int i = 0; i < argc; i++) {
      if (strcmp(argv[i], "|") == 0) {
        has_pipe = 1;
        break;
      }
    }

    if (has_pipe) {
      // Pipeline: hand off to the dedicated pipeline executor which forks one
      // child per stage and wires pipe(2) file descriptors between them.
      execute_pipeline(argv);
      free_args(argc, argv);
      continue;
    }

    // Extract any redirection operator (>, >>, 2>, …) from argv.
    char *redirect_file = NULL;
    argc = extract_redirect(argc, argv, &redirect_file);

    // Builtins run in the parent process, so we must apply the redirect here
    // (saving the original fd) and restore it afterwards.
    int saved_fd = apply_redirect(redirect_file);

    if (exec_builtin(argc, argv)) {
      restore_redirect(saved_fd);
      free_args(argc, argv);
      continue;
    }

    // Special-case: 'exit' is already handled by exec_builtin above.
    // The block below is a safety fallback that also saves history.
    if (strcmp(argv[0], "exit") == 0) {
      restore_redirect(saved_fd);
      free_args(argc, argv);
      if (histfile != NULL && strlen(histfile) > 0)
        write_history_to_file(histfile);
      exit(0);
    }

    // External commands apply the redirect inside the forked child, so restore
    // the parent's fds before forking.
    restore_redirect(saved_fd);

    if (exec_external(argc, argv, redirect_file, background, input)) {
      free_args(argc, argv);
      continue;
    }

    // Neither a builtin nor a PATH-resolvable external.
    printf("%s: command not found\n", argv[0]);
    free_args(argc, argv);
  }

  // Save history on normal (non-exit-builtin) loop termination.
  if (histfile != NULL && strlen(histfile) > 0)
    write_history_to_file(histfile);

  return 0;
}

// ---------------------------------------------------------------------------
// Utility helpers used by the completion engine
// ---------------------------------------------------------------------------

// cmpstr — qsort comparator for arrays of (char *) pointers.
int cmpstr(const void *a, const void *b) {
  const char *sa = *(const char **)a;
  const char *sb = *(const char **)b;
  return strcmp(sa, sb);
}

// split_and_sort_lines — tokenise buf on newlines, collect non-empty lines into
// out[], sort them lexicographically, and return the count.
//
// buf is modified in-place by strtok_r.  Leading whitespace on each line is
// skipped so that scripts that indent their output still work correctly.
// At most max entries are stored.
int split_and_sort_lines(char *buf, char **out, int max) {
  int   count   = 0;
  char *saveptr = NULL;
  char *line    = strtok_r(buf, "\n", &saveptr);

  while (line && count < max) {
    // Advance past any leading whitespace on this line.
    while (*line && isspace((unsigned char)*line))
      line++;
    if (*line)
      out[count++] = line;
    line = strtok_r(NULL, "\n", &saveptr);
  }

  if (count > 1)
    qsort(out, count, sizeof(char *), cmpstr);

  return count;
}
