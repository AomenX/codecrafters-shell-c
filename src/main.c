#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"
#include "exec.h"
#include "parse.h"
#include "redirect.h"

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

    // Extract redirect (e.g., "> output.txt", "2> error.txt") from argv
    char *redirect_file = NULL;
    int target_fd = -1;
    int append = 0;
    argc = extract_redirect(argc, argv, &redirect_file, &target_fd, &append);

    // Apply redirect if present (for builtins — externals handle it in child)
    int saved_fd = apply_redirect(redirect_file, target_fd, append);

    // Dispatch: builtin → external → not found
    if (exec_builtin(argc, argv)) {
      restore_redirect(saved_fd, target_fd);
      free_args(argc, argv);
      continue;
    }

    // Restore stdout/stderr before trying external/not-found (they handle
    // redirect themselves)
    restore_redirect(saved_fd, target_fd);

    if (exec_external(argc, argv, redirect_file, target_fd, append)) {
      free_args(argc, argv);
      continue;
    } else {
      printf("%s: command not found\n", argv[0]);
      free_args(argc, argv);
    }
  }
  return 0;
}