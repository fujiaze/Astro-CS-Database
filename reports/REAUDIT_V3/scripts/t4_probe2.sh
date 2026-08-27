#!/usr/bin/env bash
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
P="$ROOT/package/03_testdata"
{
  echo "=== CMD: ssh fatduck pwsh < scripts/t4_check.ps1"
  echo "=== START_UTC: $(date -u +%FT%TZ)"
  timeout 150 ssh -i "$HOME/.ssh/id_ed25519_fatduck" -o BatchMode=yes -o ConnectTimeout=10 fujia@100.104.10.71 "pwsh -NoProfile -Command -" < /home/lighthouse/astrocs_audit_v2/scripts/t4_check.ps1 2>&1
  echo "=== EXIT: $?"
  echo "=== END_UTC: $(date -u +%FT%TZ)"
} | tee "$P/fatduck_t4_direct_check.txt"
