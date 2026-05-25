#!/bin/sh
# CodeCrafters compile step (runs before run.sh). Uses vcpkg toolchain for readline.
# https://codecrafters.io/program-interface

set -e

cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
cmake --build ./build
