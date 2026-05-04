#ifndef BUILTIN_H
#define BUILTIN_H

// Checks if argv[0] is a builtin command and executes it.
// Returns 1 if handled, 0 otherwise.
int exec_builtin(int argc, char **argv);

#endif