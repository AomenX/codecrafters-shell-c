#!/bin/sh
# Local dev entry point: compile with CMake, then run ./build/shell.
# Remote grading uses .codecrafters/compile.sh and .codecrafters/run.sh instead.
# https://codecrafters.io/program-interface

set -e

# Compile (mirror .codecrafters/compile.sh for local tweaks)
(
  cd "$(dirname "$0")" # Ensure compile steps are run within the repository directory
  cmake -B build -S .
  cmake --build ./build
)

exec "$(dirname "$0")/build/shell" "$@"
