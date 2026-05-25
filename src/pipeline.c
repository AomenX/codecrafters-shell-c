#include "pipeline.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

void execute_pipeline(char **argv) {
    int pipe_idx = -1;
    for (int i = 0; argv[i] != NULL; i++) {
        if (strcmp(argv[i], "|") == 0) {
            pipe_idx = i;
            break;
        }
    }

    if (pipe_idx == -1) return; // Should not happen if called correctly

    argv[pipe_idx] = NULL;
    char **left_argv = argv;
    char **right_argv = &argv[pipe_idx + 1];

    char *left_path = find_in_path(left_argv[0]);
    if (!left_path) {
        printf("%s: command not found\n", left_argv[0]);
        return;
    }
    char *right_path = find_in_path(right_argv[0]);
    if (!right_path) {
        printf("%s: command not found\n", right_argv[0]);
        free(left_path);
        return;
    }

    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe failed");
        free(left_path);
        free(right_path);
        return;
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);
        execv(left_path, left_argv);
        perror("execv failed");
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        close(fd[1]);
        execv(right_path, right_argv);
        perror("execv failed");
        exit(1);
    }

    close(fd[0]);
    close(fd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    free(left_path);
    free(right_path);
}
