#include "parse.h"
#include <stdlib.h>
#include <string.h>

int parse_input(char *input, char **argv) {
  int argc = 0;
  char buf[1024];
  int buf_pos = 0;
  int in_single_quotes = 0;

  for (int i = 0; input[i] != '\0'; i++) {
    char c = input[i];

    if (c == '\'') {
      in_single_quotes = !in_single_quotes;
      continue; // don't add quote char to buffer
    }

    if (c == ' ' && !in_single_quotes) {
      // space outside quotes → delimiter
      if (buf_pos > 0) {
        buf[buf_pos] = '\0';
        argv[argc++] = strdup(buf);
        buf_pos = 0;
      }
      continue; // skip (collapses consecutive spaces)
    }

    // regular char, or space inside quotes
    buf[buf_pos++] = c;
  }

  // flush last token
  if (buf_pos > 0) {
    buf[buf_pos] = '\0';
    argv[argc++] = strdup(buf);
  }

  argv[argc] = NULL; // NULL-terminate for execvp
  return argc;
}

void free_args(int argc, char **argv) {
  for (int i = 0; i < argc; i++) {
    free(argv[i]);
  }
}
