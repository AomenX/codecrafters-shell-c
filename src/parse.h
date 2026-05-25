// parse.h — Tokenize a command line into heap-allocated argv strings.

#ifndef PARSE_H
#define PARSE_H

// Split input into tokens (quotes, escapes, whitespace). Fills argv with strdup'd strings.
// argv must hold at least as many pointers as tokens plus a trailing NULL for execvp.
// Returns argc (token count).
int parse_input(char *input, char **argv);

// Free each argv[i] allocated by parse_input.
void free_args(int argc, char **argv);

#endif
