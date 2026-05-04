#ifndef EXEC_H
#define EXEC_H

// Executes the command specified in argv.
// Returns 1 if handled, 0 otherwise.
int exec_external(int argc, char **argv);

#endif