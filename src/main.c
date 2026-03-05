#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtin.h"
#include "exec.h"

int main(int argc, char *argv[]) {
  char input[1024];

  // REPL: Read-Eval-Print Loop
  while (1) {
    // Flush after every printf
    fflush(stdout);
    // display prompt
    printf("$ ");
    // read input
    fgets(input, sizeof(input), stdin);
    // remove trailing newline
    input[strcspn(input, "\n")] = 0;

    // Dispatch: builtin → external → not found
    if (exec_builtin(input)) {
      continue;
    } else if (exec_external(input)) {
      continue;
    } else {
      printf("%s: command not found\n", input);
    }
  }
  return 0;
}