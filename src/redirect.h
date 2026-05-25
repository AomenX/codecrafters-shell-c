// redirect.h — Parse and apply stdout/stderr redirection operators.

#ifndef REDIRECT_H
#define REDIRECT_H

// Scan argv for >, >>, 1>, 1>>, 2>, or 2>>. On match, set *redirect_file,
// truncate argv at the operator, and return the new argc.
int extract_redirect(int argc, char **argv, char **redirect_file);

// Open redirect_file and dup2 onto the fd chosen by extract_redirect.
// Returns a duplicated fd for restore_redirect, or -1 if redirect_file is NULL.
int apply_redirect(const char *redirect_file);

// Restore the target fd (stdout or stderr) from the fd returned by apply_redirect.
void restore_redirect(int saved_fd);

#endif
