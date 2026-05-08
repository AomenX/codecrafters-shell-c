#include "redirect.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int extract_redirect(int argc, char **argv, char **redirect_file,
                     int *target_fd, int *append) {
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], "1>") == 0 ||
        strcmp(argv[i], "2>") == 0 || strcmp(argv[i], "2>>") == 0 ||
        strcmp(argv[i], ">>") == 0) {
      if (argv[i][0] == '2') {
        *target_fd = STDERR_FILENO;
      } else {
        *target_fd = STDOUT_FILENO;
      }
      *append = (strstr(argv[i], ">>") != NULL);
      *redirect_file = argv[i + 1];
      argv[i] = NULL;
      // Shift remaining arguments
      for (int j = i + 2; j < argc; j++) {
        argv[j - 2] = argv[j];
      }
      return argc - 2;
    }
  }
  *redirect_file = NULL;
  *target_fd = -1;
  *append = 0;
  return argc;
}

int apply_redirect(const char *redirect_file, int target_fd, int append) {
  if (redirect_file == NULL)
    return -1;
  int flags = O_WRONLY | O_CREAT;
  if (append) {
    flags |= O_APPEND;
  } else {
    flags |= O_TRUNC;
  }
  int fd = open(redirect_file, flags, 0644);
  if (fd == -1) {
    perror("open failed");
    return -1;
  }
  int saved_fd = dup(target_fd);
  if (saved_fd == -1) {
    perror("dup failed");
    close(fd);
    return -1;
  }
  if (dup2(fd, target_fd) == -1) {
    perror("dup2 failed");
    close(saved_fd);
    close(fd);
    return -1;
  }
  close(fd);
  return saved_fd;
}

void restore_redirect(int saved_fd, int target_fd) {
  if (saved_fd == -1)
    return;
  if (dup2(saved_fd, target_fd) == -1) {
    perror("dup2 failed");
    return;
  }
  close(saved_fd);
}
