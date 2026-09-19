#!/usr/bin/env bash
set -uo pipefail
cd "/workspace/Astro CS Database"
export TMPDIR=/dev/shm/fix-p2b
D=run/RELEASE-02/fix-p2b
g++ -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
  -Ilib/algorithms/integration/v6/include \
  lib/algorithms/integration/v6/oracle/weight_chain_selfcheck.cpp \
  lib/algorithms/integration/v6/src/weight_chain.cpp \
  -o "$D/weight_chain_selfcheck" -lm > "$D/wc_selfcheck_build.log" 2>&1
RC=$?
echo "build_rc=$RC"
if [ $RC -ne 0 ]; then tail -30 "$D/wc_selfcheck_build.log"; exit 1; fi
"$D/weight_chain_selfcheck" > "$D/wc_selfcheck.txt" 2>&1
echo "selfcheck_rc=$?"
tail -3 "$D/wc_selfcheck.txt"
