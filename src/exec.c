#include "exec.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int exec_external(char *input) {
  // strdup because strtok modifies the string, and input belongs to main
  char *input_copy = strdup(input);
  char *argv[64];
  int i = 0;

  // Build NULL-terminated argv array for execvp
  argv[0] = strtok(input_copy, " ");
  while (argv[i] != NULL) {
    i++;
    argv[i] = strtok(NULL, " ");
  }

  // Check if command exists in PATH before forking
  char *path = find_in_path(argv[0]);
  if (path == NULL) {
    free(input_copy);
    return 0;
  }

  // Fork a child process to run the external program
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork failed");
    free(path);
    free(input_copy);
    return 0;
  } else if (pid == 0) {
    // Child: replace this process with the external program
    // execvp only returns if it fails
    execvp(argv[0], argv);
    perror("execvp failed");
    free(path);
    free(input_copy);
    exit(1);
  }
  // Parent: wait for child to finish before returning to the REPL
  waitpid(pid, NULL, 0);
  free(path);
  free(input_copy);
  return 1;
}