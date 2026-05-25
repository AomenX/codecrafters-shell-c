// parse.c — Shell-like word splitting: single/double quotes and backslash escapes.

#include "parse.h"
#include <stdlib.h>
#include <string.h>

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
