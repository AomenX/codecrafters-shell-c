// path.h — Resolve command names against the PATH environment variable.

#ifndef PATH_H
#define PATH_H

// Search PATH for an executable named cmd. Caller must free the returned string.
// Returns NULL if cmd is not found or PATH is unset.
char *find_in_path(const char *cmd);

#endif
