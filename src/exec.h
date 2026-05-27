// exec.h — Run external programs found on PATH via fork + execvp.

#ifndef EXEC_H
#define EXEC_H

// Fork and exec argv[0] if it exists on PATH. redirect_file is applied in the child.
// If background is non-zero, do not wait and print "[job_id] pid".
// command_line is stored for the jobs builtin when background is non-zero.
// Returns 1 when the command was found and run, 0 if not on PATH or fork failed.
int exec_external(int argc, char **argv, const char *redirect_file, int background,
                  const char *command_line);

// Display completed jobs as Done and remove them from the job table.
void reap_done_jobs(void);

// List all jobs (Running and Done), then remove completed jobs.
void list_jobs(void);

#endif
