#include "exec.h"
#include "path.h"
#include "redirect.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int exec_external(int argc, char **argv, const char *redirect_file) {
  if (argc == 0)
    return 0;

  char *path = find_in_path(argv[0]);
  if (path == NULL) {
    return 0;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork failed");
    free(path);
    return 0;
  } else if (pid == 0) {
    // --- CHILD PROCESS ---
    if (redirect_file != NULL) {
      // This helper handles O_TRUNC/O_APPEND and STDOUT/STDERR automatically
      if (apply_redirect(redirect_file) == -1) {
        exit(1);
      }
    }

    execvp(argv[0], argv);
    perror("execvp failed");
    exit(1);
  } else {
    // --- PARENT PROCESS ---
    waitpid(pid, NULL, 0);
  }

  free(path);
  return 1;
}