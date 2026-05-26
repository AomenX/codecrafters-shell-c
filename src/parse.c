// parse.c — Shell-like word splitting: single/double quotes and backslash escapes.

#include "parse.h"
#include "builtin.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int is_name_start(char c) {
  return isalpha((unsigned char)c) || c == '_';
}

static int is_name_char(char c) {
  return isalnum((unsigned char)c) || c == '_';
}

static char *expand_word(const char *word) {
  char buf[4096];
  int pos = 0;
  int i = 0;

  while (word[i] != '\0') {
    if (word[i] == '$') {
      i++;
      if (word[i] == '{') {
        i++;
        char name[256];
        int name_pos = 0;
        while (word[i] != '\0' && word[i] != '}' &&
               name_pos < (int)sizeof(name) - 1) {
          name[name_pos++] = word[i++];
        }
        name[name_pos] = '\0';
        if (word[i] == '}')
          i++;

        const char *value = get_shell_var(name);
        if (value != NULL) {
          for (int j = 0; value[j] != '\0' && pos < (int)sizeof(buf) - 1; j++) {
            buf[pos++] = value[j];
          }
        }
      } else if (is_name_start(word[i])) {
        char name[256];
        int name_pos = 0;
        name[name_pos++] = word[i++];
        while (word[i] != '\0' && is_name_char(word[i]) &&
               name_pos < (int)sizeof(name) - 1) {
          name[name_pos++] = word[i++];
        }
        name[name_pos] = '\0';

        const char *value = get_shell_var(name);
        if (value) {
          for (int j = 0; value[j] != '\0' && pos < (int)sizeof(buf) - 1; j++) {
            buf[pos++] = value[j];
          }
        }
      } else {
        buf[pos++] = '$';
      }
    } else {
      buf[pos++] = word[i++];
    }
  }

  buf[pos] = '\0';
  return strdup(buf);
}

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

int parse_input(char *input, char **argv) {
  int argc = 0;
  char buf[1024];
  int buf_pos = 0;
  int in_single_quotes = 0;
  int in_double_quotes = 0;

  for (int i = 0; input[i] != '\0'; i++) {
    char c = input[i];

    // Outside quotes: backslash includes the next character literally.
    if (c == '\\' && !in_single_quotes && !in_double_quotes) {
      i++;
      buf[buf_pos++] = input[i];
      continue;
    }

    // Inside double quotes: only \, ", $, and ` are escapable.
    if (c == '\\' && in_double_quotes) {
      if (input[i + 1] == '"' || input[i + 1] == '\\' || input[i + 1] == '$' ||
          input[i + 1] == '`') {
        buf[buf_pos++] = input[i + 1];
        i++;
        continue;
      }
    }

    if (c == '\'' && !in_double_quotes) {
      in_single_quotes = !in_single_quotes;
      continue;
    }

    if (c == '"' && !in_single_quotes) {
      in_double_quotes = !in_double_quotes;
      continue;
    }

    if (c == '|' && !in_single_quotes && !in_double_quotes) {
      if (buf_pos > 0) {
        buf[buf_pos] = '\0';
        argv[argc++] = strdup(buf);
        buf_pos = 0;
      }
      argv[argc++] = strdup("|");
      continue;
    }

    if (c == ' ' && !in_single_quotes && !in_double_quotes) {
      if (buf_pos > 0) {
        buf[buf_pos] = '\0';
        argv[argc++] = strdup(buf);
        buf_pos = 0;
      }
      continue;
    }

    buf[buf_pos++] = c;
  }

  if (buf_pos > 0) {
    buf[buf_pos] = '\0';
    argv[argc++] = strdup(buf);
  }

  argv[argc] = NULL;
  return argc;
}

void free_args(int argc, char **argv) {
  for (int i = 0; i < argc; i++) {
    free(argv[i]);
  }
}
