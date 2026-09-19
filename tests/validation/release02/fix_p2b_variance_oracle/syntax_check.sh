#!/usr/bin/env bash
set -uo pipefail
cd "/workspace/Astro CS Database"
export TMPDIR=/dev/shm/fix-p2b
DEFINES=$(awk '/module_adapters.cpp.o:/{f=1} f&&/^  DEFINES =/{sub(/^  DEFINES = /,"");print;exit}' build/build.ninja)
INCLUDES=$(awk '/module_adapters.cpp.o:/{f=1} f&&/^  INCLUDES =/{sub(/^  INCLUDES = /,"");print;exit}' build/build.ninja)
INCLUDES="$INCLUDES -I\"/workspace/Astro CS Database/lib/algorithms/calibration/src\" -I\"/workspace/Astro CS Database/lib/algorithms/photometry/cpp/src\""
OUT=/tmp/p2b_syntax.log
eval g++ $DEFINES $INCLUDES -O1 -DNDEBUG -std=gnu++17 -fopenmp -Wall -Wextra -Wpedantic -Wconversion -fsyntax-only lib/infrastructure/scheduler/src/module_adapters.cpp > "$OUT" 2>&1
RC=$?
echo "syntax_rc=$RC"
if [ $RC -ne 0 ]; then head -100 "$OUT"; else echo "SYNTAX_OK"; fi
