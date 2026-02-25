#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *find_in_path(const char *cmd) {
  // TODO: Implement PATH search
  // 1. getenv("PATH") — get the PATH string (return NULL if not set)
  // 2. strdup() — make a copy (strtok modifies the string!)
  // 3. strtok() loop — split by ":" to get each directory
  // 4. snprintf() — build full path: "<dir>/<cmd>"
  // 5. access(full_path, X_OK) — check if executable exists
  //    If found: free the copy, return strdup(full_path)
  // 6. After loop: free the copy, return NULL (not found)
  char full_path[1024];
  char *path = getenv("PATH");
  if (path == NULL) {
    return NULL;
  }
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