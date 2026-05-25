// path.c — Walk PATH (colon-separated) and return the first executable match.

#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *find_in_path(const char *cmd) {
  char full_path[1024];
  char *path = getenv("PATH");
  if (path == NULL) {
    return NULL;
  }

  // strtok mutates its input; getenv's buffer must not be modified.
  char *path_copy = strdup(path);
  char *dir = strtok(path_copy, ":");

  while (dir != NULL) {
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
    if (access(full_path, X_OK) == 0) {
      char *result = strdup(full_path);
      free(path_copy);
      return result;
    }
    dir = strtok(NULL, ":");
  }

  free(path_copy);
  return NULL;
}
