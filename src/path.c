#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *find_in_path(const char *cmd) {
  char full_path[1024];
  // get the PATH string (return NULL if not set)
  char *path = getenv("PATH");
  if (path == NULL) {
    return NULL;
  }

  // strdup because strtok modifies the string, and getenv's result must not be
  // modified
  char *path_copy = strdup(path);
  char *dir = strtok(path_copy, ":");

  // Search each PATH directory for an executable matching cmd
  while (dir != NULL) {
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
    // check if executable exists
    // If found: free the copy, return strdup(full_path)
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