#include "exec.h"
#include "path.h"
#include "redirect.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int exec_external(int argc, char **argv, const char *redirect_file) {
  if (argc == 0)
    return 0;

  char *path = find_in_path(argv[0]);
  if (path == NULL)
    return 0;

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork failed");
    free(path);
    return 0;
  } else if (pid == 0) {
    // CHILD PROCESS
    if (redirect_file != NULL) {
      // 1. Open with the correct flags (O_TRUNC vs O_APPEND)
      int fd = open(redirect_file, redirect_flags, 0644);
      if (fd == -1) {
        perror("open failed");
        exit(1);
      }

      // 2. Use the correct target (STDOUT or STDERR)
      if (dup2(fd, redirect_target_fd) == -1) {
        perror("dup2 failed");
        exit(1);
      }
      close(fd);
    }

    // 3. execvp now sees a NULL-terminated argv that was "cut" by
    // extract_redirect
    execvp(argv[0], argv);
    perror("execvp failed");
    exit(1);
  } else {
    // PARENT PROCESS
    waitpid(pid, NULL, 0);
  }

  free(path);
  return 1;
}