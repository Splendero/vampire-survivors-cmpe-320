#!/usr/bin/env bash
# Linux build. The .bat files in this folder are for Windows + Premake.
# Point RAYLIB_INCLUDE and RAYLIB_LIB at a raylib build if it is not installed system-wide.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p bin

cxx=(g++ -std=c++17 -O2 -Wall -Wextra -Isrc)
if [[ -n "${RAYLIB_INCLUDE:-}" ]]; then
    cxx+=("-I${RAYLIB_INCLUDE}")
fi
libs=(-lraylib -lGL -lm -lpthread -ldl -lrt -lX11)
if [[ -n "${RAYLIB_LIB:-}" ]]; then
    libs=("-L${RAYLIB_LIB}" -Wl,-rpath,"${RAYLIB_LIB}" "${libs[@]}")
fi

"${cxx[@]}" -o bin/western src/main.cpp "${libs[@]}"
"${cxx[@]}" -Isrc -o bin/auto_shooter_test tests/auto_shooter_test.cpp "${libs[@]}"
echo "built bin/western and bin/auto_shooter_test"
