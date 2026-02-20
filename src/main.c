#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  fflush(stdout);

  // display prompt
  printf("$ ");

  char input[1024];
  // read input
  fgets(input, sizeof(input), stdin);

  // remove trailing newline
  input[strcspn(input, "\n")] = 0;

  // print error message
  printf("%s: command not found\n", input);

  return 0;
}
