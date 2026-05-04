#include "exec.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int exec_external(int argc, char **argv) {
  if (argc == 0)
    return 0;

  // Check if command exists in PATH before forking
  char *path = find_in_path(argv[0]);
  if (path == NULL) {
    return 0;
  }

  // Fork a child process to run the external program
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork failed");
    free(path);
    return 0;
  } else if (pid == 0) {
    // Child: replace this process with the external program
    execvp(argv[0], argv);
    perror("execvp failed");
    exit(1);
  } else {
    // Parent: wait for child to finish before returning to the REPL
    waitpid(pid, NULL, 0);
  }

  free(path);
  return 1;
}