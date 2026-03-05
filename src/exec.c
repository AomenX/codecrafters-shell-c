#include "exec.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int exec_external(char *input) {
  char *args[1024];
  int i = 0;
  while (input != NULL) {
    args[i] = strtok(input, " ");
    input = strtok(NULL, " ");
    i++;
  }
  char *path = find_in_path(args[0]);
  if (path == NULL) {
    free(args);
    return 0;
  }

  pid_t pid = fork();

  if (pid < 0) {
    perror("fork failed");
    free(path);
    free(args);
    return 0;
  } else if (pid == 0) {
    execvp(args[0], args);
    free(path);
    free(args);
    return 0;
  } else {
    waitpid(pid, NULL, 0);
    free(path);
    free(args);
    return 1;
  }
}