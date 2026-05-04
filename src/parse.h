#ifndef PARSE_H
#define PARSE_H

// Parses input string into an array of allocated strings (argv).
// Returns the number of arguments (argc).
// Caller must supply an argv array large enough to hold all tokens (e.g. 256).
int parse_input(char *input, char **argv);

// Helper to free the memory created by parse_input
void free_args(int argc, char **argv);

#endif
