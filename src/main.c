#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtin.h"

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
    // check the input if it's a builtin
    if (if_builtin(input)) {
      cmd_exit(input); // execute the builtin exit
    } else {
      // print error message
      printf("%s: command not found\n", input);
    }
  }
  return 0;
}
