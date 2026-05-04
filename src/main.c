#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtin.h"
#include "exec.h"
#include "parse.h"

int main(int argc_unused, char *argv_unused[]) {
  (void)argc_unused;
  (void)argv_unused;

  char input[1024];
  char *argv[256];

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

    // Parse input into argc/argv (handles single quotes)
    int argc = parse_input(input, argv);
    if (argc == 0)
      continue;

    // Dispatch: builtin → external → not found
    if (exec_builtin(argc, argv)) {
      free_args(argc, argv);
      continue;
    } else if (exec_external(argc, argv)) {
      free_args(argc, argv);
      continue;
    } else {
      printf("%s: command not found\n", argv[0]);
      free_args(argc, argv);
    }
  }
  return 0;
}