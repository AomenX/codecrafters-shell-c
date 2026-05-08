#include "redirect.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int extract_redirect(int argc, char **argv, char *redirect_file) {
  // TODO: loop through argv, find ">", extract filename, shorten argv
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], "1>") == 0) {
      redirect_file = argv[i + 1];
      argv[i] = NULL;
      return i;
    } else if (strcmp(argv[i], "2>") == 0) {
      redirect_file = argv[i + 1];
      argv[i] = NULL;
      return i;
    } else if (strcmp(argv[i], "2>>") == 0) {
      redirect_file = argv[i + 1];
      argv[i] = NULL;
      return i;
    } else if (strcmp(argv[i], ">>") == 0) {
      redirect_file = argv[i + 1];
      argv[i] = NULL;
      return i;
    }
  }
  redirect_file = NULL;
  return argc;
}

int apply_redirect(const char *redirect_file) {
  // TODO: open file, dup2 stdout to it, return saved stdout fd
  if (redirect_file == NULL)
    return -1;
  int fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    perror("open failed");
    return -1;
  }

  int target_fd =
      (strcmp(redirect_file, "2>") == 0 || strcmp(redirect_file, "2>>") == 0)
          ? STDERR_FILENO
          : STDOUT_FILENO;
  int saved_fd = dup(target_fd);
  if (saved_fd == -1) {
    perror("dup failed");
    return -1;
  }
  if (dup2(fd, target_fd) == -1) {
    perror("dup2 failed");
    return -1;
  }
  close(fd);
  return saved_fd;
}

void restore_redirect(int saved_fd) {
  // TODO: dup2 saved_fd back to stdout, close saved_fd
  if (saved_fd == -1)
    return;
  if (dup2(saved_fd, STDOUT_FILENO) == -1) {
    perror("dup2 failed");
    return;
  }
  close(saved_fd);
}
