// parse.c — Shell word-splitting and variable expansion.
//
// Implements the two parsing phases applied to every line of user input:
//
//   Phase 1 — parse_input():
//     Converts a raw input string into a NULL-terminated argv[] array of
//     heap-allocated strings.  Handles:
//       • Single quotes  — all characters literal, no escaping possible.
//       • Double quotes  — \, ", $, and ` can be escaped; all else is literal.
//       • Backslash outside quotes — the following character is taken literally.
//       • Unquoted '|'  — emitted as a separate token so the REPL can detect pipelines.
//       • Unquoted whitespace — acts as a token delimiter.
//
//   Phase 2 — expand_args():
//     Walks each token produced by parse_input() and replaces $name / ${name}
//     references with the corresponding shell variable value.  Tokens that
//     expand to empty strings are dropped from argv.

#include "parse.h"
#include "builtin.h"   // get_shell_var()
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ---------------------------------------------------------------------------
// Variable-expansion helpers
// ---------------------------------------------------------------------------

// is_name_start — true for characters that may begin a shell variable name.
static int is_name_start(char c) {
  return isalpha((unsigned char)c) || c == '_';
}

// is_name_char — true for characters that may appear after the first character
// of a shell variable name.
static int is_name_char(char c) {
  return isalnum((unsigned char)c) || c == '_';
}

// expand_word — expand all $VAR and ${VAR} occurrences in a single token.
//
// The result is built into a 4 KB stack buffer then strdup'd onto the heap.
// Unrecognised '$' sequences (e.g. bare '$' or '$ ') are passed through as-is.
static char *expand_word(const char *word) {
  char buf[4096];
  int  pos = 0;
  int  i   = 0;

  while (word[i] != '\0') {
    if (word[i] == '$') {
      i++;
      if (word[i] == '{') {
        // --- ${name} form ---
        i++;
        char name[256];
        int  name_pos = 0;
        // Collect characters up to the closing '}'.
        while (word[i] != '\0' && word[i] != '}' &&
               name_pos < (int)sizeof(name) - 1)
          name[name_pos++] = word[i++];
        name[name_pos] = '\0';
        if (word[i] == '}') i++;  // consume '}'

        const char *value = get_shell_var(name);
        if (value != NULL) {
          for (int j = 0; value[j] != '\0' && pos < (int)sizeof(buf) - 1; j++)
            buf[pos++] = value[j];
        }

      } else if (is_name_start(word[i])) {
        // --- $name form ---
        char name[256];
        int  name_pos = 0;
        name[name_pos++] = word[i++];
        while (word[i] != '\0' && is_name_char(word[i]) &&
               name_pos < (int)sizeof(name) - 1)
          name[name_pos++] = word[i++];
        name[name_pos] = '\0';

        const char *value = get_shell_var(name);
        if (value) {
          for (int j = 0; value[j] != '\0' && pos < (int)sizeof(buf) - 1; j++)
            buf[pos++] = value[j];
        }

      } else {
        // Bare '$' not followed by a name — pass through literally.
        buf[pos++] = '$';
      }

    } else {
      buf[pos++] = word[i++];
    }
  }

  buf[pos] = '\0';
  return strdup(buf);
}

// expand_args — apply expand_word() to every token in argv.
//
// Tokens that expand to an empty string are silently dropped (same behaviour
// as bash with unset variables).  The argv array is updated in-place.
// Returns the new argc.
int expand_args(int argc, char **argv) {
  int write = 0;
  for (int read = 0; read < argc; read++) {
    char *expanded = expand_word(argv[read]);
    free(argv[read]);
    if (expanded != NULL && expanded[0] != '\0') {
      argv[write++] = expanded;
    } else {
      free(expanded);
    }
  }
  argv[write] = NULL;
  return write;
}

// ---------------------------------------------------------------------------
// parse_input — tokenise a raw command line into an argv array.
//
// The function iterates over `input` character-by-character, maintaining two
// boolean flags (in_single_quotes, in_double_quotes) to track quoting state.
// Characters are accumulated into `buf`; a token is emitted whenever an
// unquoted whitespace or pipe character is encountered.
//
// After tokenisation argv[argc] == NULL, making the array safe to pass
// directly to execv/execvp.
//
// Parameters:
//   input  — NUL-terminated command line (mutated in place via strdup calls).
//   argv   — caller-provided array; must be large enough for all tokens + NULL.
// Returns:
//   argc (number of tokens, excluding the trailing NULL).
// ---------------------------------------------------------------------------
int parse_input(char *input, char **argv) {
  int argc             = 0;
  char buf[1024];
  int  buf_pos         = 0;
  int  in_single_quotes = 0;
  int  in_double_quotes = 0;

  for (int i = 0; input[i] != '\0'; i++) {
    char c = input[i];

    // --- Backslash outside all quotes: escape the next character literally ---
    if (c == '\\' && !in_single_quotes && !in_double_quotes) {
      i++;
      buf[buf_pos++] = input[i];
      continue;
    }

    // --- Backslash inside double quotes: only some characters are escapable ---
    // POSIX specifies that only \, ", $, and ` have special meaning after \.
    // Everything else keeps both the backslash and the following character.
    if (c == '\\' && in_double_quotes) {
      if (input[i + 1] == '"'  || input[i + 1] == '\\'
       || input[i + 1] == '$'  || input[i + 1] == '`') {
        buf[buf_pos++] = input[i + 1];
        i++;
        continue;
      }
      // Not an escapable character inside double quotes — fall through.
    }

    // --- Toggle single-quote mode (not active inside double quotes) ---
    if (c == '\'' && !in_double_quotes) {
      in_single_quotes = !in_single_quotes;
      continue;
    }

    // --- Toggle double-quote mode (not active inside single quotes) ---
    if (c == '"' && !in_single_quotes) {
      in_double_quotes = !in_double_quotes;
      continue;
    }

    // --- Unquoted pipe: flush current token then emit "|" as its own token ---
    if (c == '|' && !in_single_quotes && !in_double_quotes) {
      if (buf_pos > 0) {
        buf[buf_pos] = '\0';
        argv[argc++] = strdup(buf);
        buf_pos = 0;
      }
      argv[argc++] = strdup("|");
      continue;
    }

    // --- Unquoted whitespace: flush current token (delimiter) ---
    if (c == ' ' && !in_single_quotes && !in_double_quotes) {
      if (buf_pos > 0) {
        buf[buf_pos] = '\0';
        argv[argc++] = strdup(buf);
        buf_pos = 0;
      }
      continue;
    }

    // --- Ordinary character: accumulate into the current token ---
    buf[buf_pos++] = c;
  }

  // Flush the final token (no trailing whitespace or pipe to trigger it above).
  if (buf_pos > 0) {
    buf[buf_pos] = '\0';
    argv[argc++] = strdup(buf);
  }

  argv[argc] = NULL;   // NULL-terminate for execv/execvp compatibility
  return argc;
}

// free_args — release every heap-allocated string in argv[0..argc-1].
// Does not free the argv array itself (it is usually stack-allocated).
void free_args(int argc, char **argv) {
  for (int i = 0; i < argc; i++)
    free(argv[i]);
}
