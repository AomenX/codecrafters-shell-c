#ifndef REDIRECT_H
#define REDIRECT_H

// Scans argv for ">" operator. If found:
//   - Sets *redirect_file to the filename (the token after ">")
//   - Removes ">" and the filename from argv
//   - Returns the new argc
// If not found, sets *redirect_file to NULL and returns argc unchanged.
int extract_redirect(int argc, char **argv, char *redirect_file);

// Opens redirect_file for writing and redirects stdout to it.
// Returns the saved stdout fd (for later restore), or -1 if redirect_file is
// NULL.
int apply_redirect(const char *redirect_file);

// Restores stdout from a previously saved fd.
// Pass the value returned by apply_redirect.
void restore_redirect(int saved_fd);

#endif
