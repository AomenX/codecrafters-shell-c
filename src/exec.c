// exec.c — External command execution: resolve PATH, fork, redirect in child, wait.

#include "exec.h"
#include "path.h"
#include "redirect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_JOBS 256

typedef struct {
  int job_id;
  pid_t pid;
  char *command;
  int done;
} Job;

static Job jobs[MAX_JOBS];
static int num_jobs = 0;
static int next_job_id = 1;

static void add_job(int job_id, pid_t pid, const char *command) {
  if (num_jobs >= MAX_JOBS)
    return;
  jobs[num_jobs].job_id = job_id;
  jobs[num_jobs].pid = pid;
  jobs[num_jobs].command = strdup(command);
  jobs[num_jobs].done = 0;
  num_jobs++;
}

static void update_job_statuses(void) {
  for (int i = 0; i < num_jobs; i++) {
    if (jobs[i].done)
      continue;
    int status;
    pid_t result = waitpid(jobs[i].pid, &status, WNOHANG);
    if (result == jobs[i].pid && WIFEXITED(status)) {
      jobs[i].done = 1;
    }
  }
}

static int compare_jobs(const void *a, const void *b) {
  const Job *ja = (const Job *)a;
  const Job *jb = (const Job *)b;
  return ja->job_id - jb->job_id;
}

static char job_marker(int job_id, Job *sorted, int count) {
  int max_id = -1;
  int second_max = -1;

  for (int i = 0; i < count; i++) {
    if (sorted[i].job_id > max_id) {
      second_max = max_id;
      max_id = sorted[i].job_id;
    } else if (sorted[i].job_id > second_max) {
      second_max = sorted[i].job_id;
    }
  }

  if (job_id == max_id)
    return '+';
  if (job_id == second_max)
    return '-';
  return ' ';
}

static void print_job_line(const Job *job, char marker) {
  const char *status = job->done ? "Done" : "Running";
  char command[1024];

  strncpy(command, job->command, sizeof(command) - 1);
  command[sizeof(command) - 1] = '\0';

  if (job->done) {
    size_t len = strlen(command);
    if (len >= 2 && command[len - 2] == ' ' && command[len - 1] == '&') {
      command[len - 2] = '\0';
    }
  }

  printf("[%d]%c  %-24s %s\n", job->job_id, marker, status, command);
}

static void remove_done_jobs(void) {
  int write = 0;
  for (int i = 0; i < num_jobs; i++) {
    if (!jobs[i].done) {
      jobs[write++] = jobs[i];
    } else {
      free(jobs[i].command);
    }
  }
  num_jobs = write;
}

static void print_jobs(int done_only) {
  if (num_jobs == 0)
    return;

  update_job_statuses();

  Job sorted[MAX_JOBS];
  memcpy(sorted, jobs, (size_t)num_jobs * sizeof(Job));
  qsort(sorted, (size_t)num_jobs, sizeof(Job), compare_jobs);

  for (int i = 0; i < num_jobs; i++) {
    if (done_only && !sorted[i].done)
      continue;
    char marker = job_marker(sorted[i].job_id, sorted, num_jobs);
    print_job_line(&sorted[i], marker);
  }

  remove_done_jobs();
}

void reap_done_jobs(void) {
  print_jobs(1);
}

void list_jobs(void) {
  print_jobs(0);
}

int exec_external(int argc, char **argv, const char *redirect_file, int background,
                  const char *command_line) {
  if (argc == 0)
    return 0;

  char *path = find_in_path(argv[0]);
  if (path == NULL) {
    return 0;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork failed");
    free(path);
    return 0;
  } else if (pid == 0) {
    if (redirect_file != NULL) {
      if (apply_redirect(redirect_file) == -1) {
        exit(1);
      }
    }

    execvp(argv[0], argv);
    perror("execvp failed");
    exit(1);
  } else if (background) {
    add_job(next_job_id, pid, command_line);
    printf("[%d] %d\n", next_job_id, (int)pid);
    fflush(stdout);
    next_job_id++;
  } else {
    waitpid(pid, NULL, 0);
  }

  free(path);
  return 1;
}
