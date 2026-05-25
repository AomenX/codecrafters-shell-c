// builtin.h — Shell builtins executed in the parent process (no fork).

#ifndef BUILTIN_H
#define BUILTIN_H

#include <stdio.h>

// If argv[0] names a builtin, run it and return 1; otherwise return 0.
int exec_builtin(int argc, char **argv);

// History management functions
void init_history(void);
void add_to_history(const char *cmd);
void list_history(void);
void list_history_last(int limit);
int get_history_size(void);
const char *get_history_entry(int index);
void set_history_pos(int pos);
int get_history_pos(void);
void history_move_next(void);
void history_move_prev(void);
void execute_history_command(int index);
void read_history_from_file(const char *path);
void write_history_to_file(const char *path);
void append_history_to_file(const char *path);

// History configuration
#define MAX_HISTORY_SIZE 1000
#define MAX_HISTORY_LINE_LEN 1024

#endif
