[![progress-banner](https://backend.codecrafters.io/progress/shell/277122b5-057c-4946-8543-cc5188b0a93d)](https://app.codecrafters.io/users/codecrafters-bot?r=2qF)

# 🐚 Shell — Build Your Own Shell in C

A fully-featured, POSIX-inspired Unix shell written from scratch in C as part of the [CodeCrafters "Build Your Own Shell"](https://app.codecrafters.io/courses/shell/overview) challenge. This project was built stage-by-stage, implementing every subsystem by hand — from raw line reading to tab completion and background job control.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Project Structure](#project-structure)
- [Architecture](#architecture)
- [Building and Running](#building-and-running)
- [Usage Examples](#usage-examples)
- [Implementation Notes & Challenges](#implementation-notes--challenges)
- [What I Learned](#what-i-learned)

---

## Overview

This shell is a from-scratch C implementation of a Unix shell. It is not a wrapper around `/bin/sh` — every feature was implemented manually: the REPL loop, tokenizer, quote handling, redirection, pipes, job control, command history, and interactive tab completion.

The project was completed incrementally through the CodeCrafters challenge, where each stage introduced a new requirement verified by an automated test runner on a Linux container.

---

## Features

### Core Shell Mechanics
| Feature | Description |
|---------|-------------|
| **REPL** | Read-Eval-Print Loop with GNU Readline for interactive input |
| **Command parsing** | Full word-splitting with single quotes, double quotes, and backslash escaping |
| **Variable expansion** | `$VAR` and `${VAR}` expansions using shell-local variables |
| **Output redirection** | `>`, `>>`, `1>`, `1>>`, `2>`, `2>>` redirect stdout and stderr |
| **Pipelines** | Arbitrary N-command pipelines: `cmd1 \| cmd2 \| cmd3 \| ...` |
| **Background jobs** | Append `&` to run a command in the background |

### Built-in Commands
| Command | Description |
|---------|-------------|
| `echo [args]` | Print arguments separated by spaces |
| `exit [code]` | Exit the shell with an optional status code |
| `type <name>` | Report whether a name is a builtin, external command, or unknown |
| `pwd` | Print the current working directory |
| `cd [path]` | Change directory; `~` and empty path resolve to `$HOME` |
| `history [N]` | List command history; optionally show only the last N entries |
| `history -r/-w/-a <file>` | Read, write, or append history to a file |
| `jobs` | List all background jobs with their status and job number |
| `complete -C <script> <cmd>` | Register a completer script for a command |
| `complete -p <cmd>` | Print the registered completion specification for a command |
| `complete -r <cmd>` | Remove a registered completion specification |
| `declare [name=value]` | Declare or inspect shell-local variables |

### Tab Completion
| Completion Type | Description |
|-----------------|-------------|
| **Builtin commands** | `ech<TAB>` → `echo ` |
| **PATH executables** | `custom_ex<TAB>` → `custom_exe_1234 ` |
| **Filename completion** | `cat re<TAB>` → `cat readme.txt ` |
| **Nested paths** | `du owl/p<TAB>` → `du owl/pig/` |
| **Multiple matches** | First `<TAB>` rings the bell; second lists all matches |
| **Directory suffixes** | Directories shown with trailing `/` in match lists |
| **Partial completion** | Advances to the longest common prefix before listing |
| **Programmable completion** | Per-command completer scripts invoked via `complete -C` |

### History
| Feature | Description |
|---------|-------------|
| **Listing** | `history` prints all entries with 5-wide right-aligned indices |
| **Limiting** | `history N` prints the last N entries |
| **Persistence** | Reads and writes `$HISTFILE` on startup and exit |
| **Append mode** | Only new commands appended to file on exit (not full rewrite) |
| **Readline integration** | Up/down arrow navigation via GNU Readline's native history |

### Job Control
| Feature | Description |
|---------|-------------|
| **Background execution** | `sleep 10 &` forks without blocking the shell |
| **Job table** | Tracks PID, job ID, command string, and done status |
| **Job markers** | `+` marks the most recent job; `-` marks the second most recent |
| **Reaping** | Done jobs are reported before each prompt (no zombie processes) |
| **Job number recycling** | Finished job numbers are reused; the smallest free number is always assigned |

---

## Project Structure

```
.
├── your_program.sh          # Entry point script (builds and runs the shell)
├── CMakeLists.txt           # Build configuration
└── src/
    ├── main.c               # REPL loop, tab completion engine, completer script runner
    ├── builtin.c            # All built-in command implementations + history + complete
    ├── builtin.h            # Public API for builtins, history, and completion registry
    ├── parse.c              # Word splitter: quotes, escapes, pipe tokens, $VAR expansion
    ├── parse.h
    ├── exec.c               # External command execution: fork/exec, job table management
    ├── exec.h
    ├── pipe.c               # Multi-command pipeline execution using fork and pipe(2)
    ├── pipe.h
    ├── redirect.c           # Redirection operators: extract, apply, restore via dup2
    ├── redirect.h
    ├── path.c               # PATH resolution: walk $PATH and find the first executable match
    └── path.h
```

---

## Architecture

```
User input (readline)
        │
        ▼
   parse_input()          ← word-split, quote handling, pipe token injection
        │
        ├─── expand_args()      ← $VAR / ${VAR} substitution
        │
        ├─── Background? (&)    ← strip trailing & and set background flag
        │
        ├─── Has pipe? (|)
        │       └─── execute_pipeline()   ← fork N children, wire pipe(2) fds
        │
        ├─── extract_redirect() ← scan for >, >>, 2>, etc.
        │
        ├─── exec_builtin()     ← run in parent process (no fork)
        │       └─── echo / exit / type / pwd / cd / history / jobs / complete / declare
        │
        └─── exec_external()    ← fork + execvp, optional background + job table
                └─── alloc_job_id()   ← smallest available job number
```

Tab completion is wired through **GNU Readline**'s `rl_attempted_completion_function` hook. The custom generator `my_completion()` dispatches to:
1. `my_generator()` when completing the first word (commands + PATH executables)
2. The registered completer script (via `run_completer_script()`) when one exists for the current command
3. Readline's built-in filename completion as a fallback for arguments

---

## Building and Running

### Prerequisites

| Dependency | Notes |
|------------|-------|
| `gcc` or `clang` | C99 or later |
| `cmake` ≥ 3.13 | Build system |
| `libreadline-dev` | GNU Readline (for interactive input, history, tab completion) |

On Ubuntu/Debian:
```bash
sudo apt install build-essential cmake libreadline-dev
```

On macOS (uses `libedit` by default via Homebrew):
```bash
brew install cmake readline
```

### Build

```bash
# Configure
cmake -S . -B build

# Compile
cmake --build build

# Run
./your_program.sh
# or directly:
./build/shell
```

### Running via CodeCrafters

```bash
git add .
git commit -m "my changes"
git push origin master
```

The CodeCrafters test runner will automatically build and test your submission.

---

## Usage Examples

```bash
# Basic commands
$ echo hello world
hello world

$ pwd
/home/user

$ cd /tmp && pwd
/tmp

# Quoting
$ echo 'keep   spaces'
keep   spaces

$ echo "expand $HOME here"
/home/user here

# Redirection
$ echo hello > output.txt
$ cat output.txt
hello

$ echo appended >> output.txt
$ ls -la 2> errors.txt

# Pipelines
$ cat /etc/passwd | grep root | cut -d: -f1
root

$ ls -la /tmp | tail -n 5 | head -n 3 | grep "file"
-rw-r--r-- 1 user user 5 file

# Background jobs
$ sleep 60 &
[1] 12345

$ jobs
[1]+  Running                 sleep 60 &

# History
$ history
    1  echo hello world
    2  pwd
    3  history

$ history 2
    2  pwd
    3  history

# Tab completion
$ ech<TAB>
$ echo 

$ cat read<TAB>
$ cat readme.txt 

# Programmable completion
$ complete -C /path/to/my_completer git
$ git <TAB>
$ git clone 

# Shell variables
$ declare MY_VAR=hello
$ echo ${MY_VAR}
hello
```

---

## Implementation Notes & Challenges

### 1. Quote Parsing
The trickiest part of parsing was correctly handling the three quoting modes simultaneously:
- **Single quotes** — everything literal, no escapes
- **Double quotes** — `\`, `"`, `$`, and `` ` `` are escapable; everything else is literal
- **Backslash outside quotes** — escapes the immediately following character

The parser uses a two-flag state machine (`in_single_quotes`, `in_double_quotes`) and processes the input character-by-character, accumulating tokens into a buffer and emitting them on unquoted whitespace or pipe characters.

### 2. Pipeline Architecture
Each pipeline command runs in its own forked child process. The parent creates `N-1` pipes for N commands, wiring each child's `stdin` to the previous pipe's read-end and `stdout` to the current pipe's write-end. The key challenge was ensuring all unused file descriptor copies are closed in both parent and child — a single un-closed write-end causes the next `read()` to block indefinitely.

### 3. Redirection + Builtins
External commands apply redirections inside the forked child (just before `execvp`). Builtins run in the parent process, so they need a different approach: `apply_redirect()` saves the original fd with `dup()`, performs the `dup2()`, runs the builtin, then `restore_redirect()` puts the original fd back. This avoids leaking the redirect into subsequent commands.

### 4. Tab Completion with Readline on macOS vs. Linux
GNU Readline and macOS's `libedit` are largely compatible but diverge in some hooks. Specifically, `rl_completion_display_matches_hook` (used to display matches with directory `/` suffixes) is a GNU Readline extension not present in `libedit`. This was handled with `#ifndef __APPLE__` preprocessor guards, allowing the project to compile cleanly on both macOS (for local development) and Linux (for the CodeCrafters test runner).

### 5. Programmable Completion (completer scripts)
The `complete -C <script> <cmd>` system required forking a child process, setting `COMP_LINE` and `COMP_POINT` environment variables per the Bash completion protocol, and reading the script's stdout through a pipe. The output lines are then sorted and fed back into Readline's match list. Finding the longest common prefix (LCP) of all candidates was necessary to implement the "partial completion then list" behaviour.

### 6. Job Number Recycling
An initial implementation used a simple `next_job_id++` counter. This broke the requirement that job numbers reuse the smallest available slot (so that after job `[2]` exits while `[1]` still runs, the next job gets `[2]` again, not `[3]`). The fix was `alloc_job_id()`, which scans the live job table and returns the first positive integer not currently in use.

### 7. History Persistence
The `$HISTFILE` mechanism required correctly distinguishing between "load from file on startup", "write all history on clean exit", and "append only new commands since last save". A `last_saved_index` counter tracks the position of the last write, so `append_history_to_file()` can skip entries that were already persisted.

---

## What I Learned

- **Systems programming fundamentals** — `fork()`, `execvp()`, `dup2()`, `pipe()`, `waitpid()`, and how they compose to build a real process model
- **File descriptor management** — why closing unused pipe ends is critical, and how `dup()`/`dup2()` enable transient fd swapping for in-process redirects
- **GNU Readline internals** — the completion callback protocol (`rl_attempted_completion_function`, `rl_completion_matches`, `rl_attempted_completion_over`), custom generators, and display hooks
- **Careful C memory management** — every `strdup`, `malloc`, and `realloc` has a matching `free`; pipe buffers are grown dynamically; the job table frees command strings when entries are removed
- **Cross-platform C** — `libedit` vs. GNU Readline differences and using preprocessor guards to keep the code buildable on both macOS and Linux
- **Incremental software development** — building a complex system one feature at a time, with automated regression testing at each step

---

*Built as part of the [CodeCrafters "Build Your Own Shell"](https://app.codecrafters.io/courses/shell/overview) challenge.*
