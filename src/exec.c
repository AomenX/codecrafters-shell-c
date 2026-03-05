#include "exec.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int exec_external(char *input) {
  char *input_copy = strdup(input);
  char *argv[64];
  int i = 0;

  argv[0] = strtok(input_copy, " ");
  while (argv[i] != NULL) {
    i++;
    argv[i] = strtok(NULL, " ");
  }
  char *path = find_in_path(argv[0]);
  if (path == NULL) {
    free(input_copy);
    return 0;
  }

  pid_t pid = fork();

  if (pid < 0) {
    perror("fork failed");
    free(path);
    free(input_copy);
    return 0;
  } else if (pid == 0) {
    execvp(argv[0], argv);
    perror("execvp failed");
    free(path);
    free(input_copy);
    exit(1);
  } else {
    waitpid(pid, NULL, 0);
    free(path);
    free(input_copy);
    return 1;
  }
}