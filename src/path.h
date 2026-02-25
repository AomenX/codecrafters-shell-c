#ifndef PATH_H
#define PATH_H

// Searches the PATH environment variable for an executable matching `cmd`.
// Returns the full path (e.g. "/bin/cat") if found, or NULL if not found.
char *find_in_path(const char *cmd);

#endif
