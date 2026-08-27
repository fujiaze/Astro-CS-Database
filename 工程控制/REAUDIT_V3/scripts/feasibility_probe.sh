#!/usr/bin/env bash
# AstroCS audit supplement V2 - feasibility probes (Fatduck SSH, toolchain)
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
LOG="$ROOT/logs"
mkdir -p "$LOG"

# --- Fatduck SSH probe (non-blocking, 12s cap) ---
{
  echo "=== CMD: ssh -o BatchMode=yes -o ConnectTimeout=10 fujia@fatduck 'echo FATDUCK_OK'"
  echo "=== START_UTC: $(date -u +%FT%TZ)"
  timeout 15 ssh -o BatchMode=yes -o ConnectTimeout=10 fujia@fatduck 'echo FATDUCK_OK && ver | head -1' 2>&1
  echo "=== EXIT: $?"
  echo "=== END_UTC: $(date -u +%FT%TZ)"
} > "$ROOT/package/03_testdata/fatduck_ssh_probe.txt" 2>&1
echo "fatduck probe: $(grep EXIT "$ROOT/package/03_testdata/fatduck_ssh_probe.txt" | head -1)"

# --- toolchain inventory ---
{
  echo "collected_utc=$(date -u +%FT%TZ)"
  echo "== gcc =="; gcc --version 2>&1 | head -2 || echo MISSING
  echo "== g++ =="; g++ --version 2>&1 | head -2 || echo MISSING
  echo "== clang =="; clang --version 2>&1 | head -2 || echo MISSING
  echo "== cmake =="; cmake --version 2>&1 | head -2 || echo MISSING
  echo "== make =="; make --version 2>&1 | head -2 || echo MISSING
  echo "== ninja =="; ninja --version 2>&1 || echo MISSING
  echo "== python3 =="; python3 --version 2>&1 || echo MISSING
  echo "== clang-tidy =="; clang-tidy --version 2>&1 | head -2 || echo MISSING
  echo "== cppcheck =="; cppcheck --version 2>&1 || echo MISSING
  echo "== git =="; git --version 2>&1
  echo "== /usr/bin/time =="; /usr/bin/time --version 2>&1 | head -1 || echo MISSING
  echo "== pidstat/mpstat/iostat/perf =="
  for t in pidstat mpstat iostat perf; do command -v $t >/dev/null 2>&1 && echo "$t PRESENT" || echo "$t MISSING"; done
  echo "== nproc/mem =="
  nproc; grep MemTotal /proc/meminfo
} > "$ROOT/package/04_build/toolchain_inventory.txt" 2>&1
echo "toolchain written"
