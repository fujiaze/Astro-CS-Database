#!/usr/bin/env bash
set -uo pipefail
cd "/workspace/Astro CS Database"
export TMPDIR="${TMPDIR:-/var/tmp/astrocs}"
D=run/RELEASE-02/fix-p2b
g++ -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
  -Ilib/algorithms/integration/v6/include \
  "$D/variance_oracle.cpp" \
  lib/algorithms/integration/v6/src/variance_propagation.cpp \
  lib/algorithms/integration/v6/src/weight_chain.cpp \
  -o "$D/variance_oracle" -lm > "$D/oracle_build.log" 2>&1
RC=$?
echo "build_rc=$RC"
if [ $RC -ne 0 ]; then cat "$D/oracle_build.log"; exit 1; fi
echo "--- GREEN run (expect rc=0) ---"
"$D/variance_oracle" > "$D/oracle_output.txt" 2>&1
GREEN=$?
tail -1 "$D/oracle_output.txt"
echo "green_rc=$GREEN"
echo "--- RED run P2B_ORACLE_FAULT=naive (expect rc=1) ---"
P2B_ORACLE_FAULT=naive "$D/variance_oracle" > "$D/oracle_output_red.txt" 2>&1
RED=$?
grep -c "\[FAIL\]" "$D/oracle_output_red.txt" | sed 's/^/red_fail_count=/'
tail -1 "$D/oracle_output_red.txt"
echo "red_rc=$RED"
if [ $GREEN -eq 0 ] && [ $RED -ne 0 ]; then echo "ORACLE_REDGREEN_OK"; else echo "ORACLE_REDGREEN_BAD"; exit 1; fi
