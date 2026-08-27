#!/usr/bin/env bash
# Fatduck T4 location probe (read-only)
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
P="$ROOT/package/03_testdata"
{
  echo "=== CMD: ssh -i ~/.ssh/id_ed25519_fatduck fujia@100.104.10.71 'pwsh -NoProfile -Command -' < scripts/t4_locate.ps1"
  echo "=== START_UTC: $(date -u +%FT%TZ)"
  timeout 150 ssh -i "$HOME/.ssh/id_ed25519_fatduck" -o BatchMode=yes -o ConnectTimeout=10 fujia@100.104.10.71 "pwsh -NoProfile -Command -" < /home/lighthouse/astrocs_audit_v2/scripts/t4_locate.ps1 2>&1
  echo "=== EXIT: $?"
  echo "=== END_UTC: $(date -u +%FT%TZ)"
} | tee "$P/fatduck_t4_location_probe.txt"
echo "probe written"
