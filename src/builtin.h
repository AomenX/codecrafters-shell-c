// builtin.h — Shell builtins executed in the parent process (no fork).

#ifndef BUILTIN_H
#define BUILTIN_H

// If argv[0] names a builtin, run it and return 1; otherwise return 0.
int exec_builtin(int argc, char **argv);

#endif
