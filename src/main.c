#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtin.h"
#include "exec.h"

int main(int argc, char *argv[]) {
  char input[1024];
  // REPL
  while (1) {
    // Flush after every printf
    fflush(stdout);
    // display prompt
    printf("$ ");
    // read input
    fgets(input, sizeof(input), stdin);
    // remove trailing newline
    input[strcspn(input, "\n")] = 0;

    // Try to execute as a builtin, otherwise print error
    if (!exec_builtin(input)) {
      printf("%s: command not found\n", input);
    } else {
      exec_external(input);
    }
  }
  return 0;
}