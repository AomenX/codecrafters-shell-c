#!/bin/sh
# CodeCrafters run step (after compile.sh). Replaces the shell process with the binary.
# https://codecrafters.io/program-interface

set -e

exec $(dirname "$0")/build/shell "$@"
