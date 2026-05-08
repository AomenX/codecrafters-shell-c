#include "redirect.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  int target_fd;
  int flags;
} RedirectConfig;

// Global configuration to track the latest redirection parsed
static RedirectConfig current_config;

int extract_redirect(int argc, char **argv, char **redirect_file) {
  for (int i = 0; i < argc; i++) {
    int target_fd = -1;
    int flags = O_WRONLY | O_CREAT;

    if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], "1>") == 0) {
      target_fd = STDOUT_FILENO;
      flags |= O_TRUNC;
    } else if (strcmp(argv[i], ">>") == 0 || strcmp(argv[i], "1>>") == 0) {
      target_fd = STDOUT_FILENO;
      flags |= O_APPEND;
    } else if (strcmp(argv[i], "2>") == 0) {
      target_fd = STDERR_FILENO;
      flags |= O_TRUNC;
    } else if (strcmp(argv[i], "2>>") == 0) {
      target_fd = STDERR_FILENO;
      flags |= O_APPEND;
    }

    if (target_fd != -1) {
      // Safety check: ensure filename exists
      if (i + 1 < argc) {
        *redirect_file = argv[i + 1];
        current_config.target_fd = target_fd;
        current_config.flags = flags;

        // Cut the argv array at the operator
        argv[i] = NULL;
        return i;
      }
    }
  }
  *redirect_file = NULL;
  return argc;
}

int apply_redirect(const char *redirect_file) {
  if (redirect_file == NULL)
    return -1;

  int fd = open(redirect_file, current_config.flags, 0644);
  if (fd == -1) {
    perror("open failed");
    return -1;
  }

  // Save the original FD (1 or 2)
  int saved_fd = dup(current_config.target_fd);

  // Redirect target FD to our file
  if (dup2(fd, current_config.target_fd) == -1) {
    perror("dup2 failed");
    close(fd);
    return -1;
  }

  close(fd);
  return saved_fd;
}

void restore_redirect(int saved_fd) {
  if (saved_fd == -1)
    return;
  dup2(saved_fd, current_config.target_fd);
  close(saved_fd);
}