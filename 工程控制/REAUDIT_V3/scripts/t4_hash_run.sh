#!/usr/bin/env bash
# Compute remote-side SHA-256 manifest of T4 Red lights + masters on Fatduck (read-only)
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
P="$ROOT/package/03_testdata"
B64=$(iconv -f utf-8 -t utf-16le /home/lighthouse/astrocs_audit_v2/scripts/t4_hash.ps1 | base64 -w0)
{
  echo "=== CMD: ssh fatduck pwsh -EncodedCommand t4_hash.ps1 (Get-FileHash x35)"
  echo "=== START_UTC: $(date -u +%FT%TZ)"
  timeout 400 ssh -i "$HOME/.ssh/id_ed25519_fatduck" -o BatchMode=yes -o ConnectTimeout=10 fujia@100.104.10.71 "pwsh -NoProfile -EncodedCommand $B64" 2>&1 | grep -Ev "^#< CLIXML|^<Objs " || true
  echo "=== EXIT: ${PIPESTATUS[0]}"
  echo "=== END_UTC: $(date -u +%FT%TZ)"
} | tee "$P/fatduck_t4_hash_run.log"
echo done
