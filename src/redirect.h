#ifndef REDIRECT_H
#define REDIRECT_H

/**
 * Parses argv for redirection operators (>, >>, 2>, 2>>).
 * Sets redirect_file to the target filename and modifies argv to "cut" the
 * command. Returns the new argc (index of the NULL terminator).
 */
int extract_redirect(int argc, char **argv, char **redirect_file);

/**
 * Opens the redirect_file and redirects the target FD (stdout or stderr).
 * Returns a "saved" FD that can be used to restore the original state.
 */
int apply_redirect(const char *redirect_file);

/**
 * Restores the original FD using the saved FD.
 */
void restore_redirect(int saved_fd);

#endif