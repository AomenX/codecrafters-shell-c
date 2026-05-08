#ifndef REDIRECT_H
#define REDIRECT_H

// Scans argv for ">", ">>", "2>", "2>>" operators. If found:
//   - Sets *redirect_file to the filename (the token after the operator)
//   - Sets *target_fd to the file descriptor (STDOUT_FILENO or STDERR_FILENO)
//   - Removes operator and the filename from argv
//   - Returns the new argc
// If not found, sets *redirect_file to NULL and returns argc unchanged.
int extract_redirect(int argc, char **argv, char **redirect_file,
                     int *target_fd, int *append);

// Opens redirect_file for writing and redirects target_fd to it.
// Returns the saved fd (for later restore), or -1 if redirect_file is NULL.
int apply_redirect(const char *redirect_file, int target_fd, int append);

// Restores the original file descriptor from a previously saved fd.
// Pass the value returned by apply_redirect and the target_fd used.
void restore_redirect(int saved_fd, int target_fd);

#endif
