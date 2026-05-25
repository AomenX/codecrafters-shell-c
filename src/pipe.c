#include "pipe.h"
#include "builtin.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

void execute_pipeline(char **argv) {
    int num_cmds = 1;
    for (int i = 0; argv[i] != NULL; i++) {
        if (strcmp(argv[i], "|") == 0) {
            num_cmds++;
        }
    }

    char ***cmds = malloc(num_cmds * sizeof(char **));
    int cmd_idx = 0;
    cmds[cmd_idx++] = argv;
    for (int i = 0; argv[i] != NULL; i++) {
        if (strcmp(argv[i], "|") == 0) {
            argv[i] = NULL;
            cmds[cmd_idx++] = &argv[i + 1];
        }
    }

    int prev_fd = -1;
    pid_t *pids = malloc(num_cmds * sizeof(pid_t));

    for (int i = 0; i < num_cmds; i++) {
        int fd[2];
        if (i < num_cmds - 1) {
            if (pipe(fd) == -1) {
                perror("pipe failed");
                free(cmds);
                free(pids);
                return;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            if (i > 0) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (i < num_cmds - 1) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }

            int argc_cmd = 0;
            while (cmds[i][argc_cmd]) argc_cmd++;

            if (exec_builtin(argc_cmd, cmds[i])) {
                exit(0);
            }

            char *cmd_path = find_in_path(cmds[i][0]);
            if (!cmd_path) {
                printf("%s: command not found\n", cmds[i][0]);
                exit(1);
            }
            execv(cmd_path, cmds[i]);
            perror("execv failed");
            exit(1);
        }

        pids[i] = pid;
        if (i > 0) {
            close(prev_fd);
        }
        if (i < num_cmds - 1) {
            close(fd[1]);
            prev_fd = fd[0];
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        waitpid(pids[i], NULL, 0);
    }

    free(cmds);
    free(pids);
}
