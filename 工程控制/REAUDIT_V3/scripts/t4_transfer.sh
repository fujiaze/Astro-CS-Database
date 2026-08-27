#!/usr/bin/env bash
# Pack T4 Red+masters on Fatduck, pull via scp, extract into audit root testdata/
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
P="$ROOT/package/03_testdata"
LOG="$ROOT/logs"
B64=$(iconv -f utf-8 -t utf-16le /home/lighthouse/astrocs_audit_v2/scripts/t4_pack.ps1 | base64 -w0)
{
  echo "=== STEP1 remote pack START_UTC: $(date -u +%FT%TZ)"
  timeout 300 ssh -i "$HOME/.ssh/id_ed25519_fatduck" -o BatchMode=yes -o ConnectTimeout=10 fujia@100.104.10.71 "pwsh -NoProfile -EncodedCommand $B64" 2>&1 | grep -Ev "^#< CLIXML|^<Objs " || true
  echo "=== STEP1 EXIT: ${PIPESTATUS[0]}"
} | tee "$LOG/t4_transfer_step1_pack.log"

{
  echo "=== STEP2 scp START_UTC: $(date -u +%FT%TZ)"
  timeout 3600 scp -i "$HOME/.ssh/id_ed25519_fatduck" -o BatchMode=yes "fujia@100.104.10.71:C:/Users/fujia/AppData/Local/Temp/t4_audit/t4_red_plus_masters.tar" "$ROOT/testdata/t4_red_plus_masters.tar"
  echo "=== STEP2 EXIT: $?"
  ls -la "$ROOT/testdata/"
  echo "=== STEP2 END_UTC: $(date -u +%FT%TZ)"
} > "$LOG/t4_transfer_step2_scp.log" 2>&1

{
  echo "=== STEP3 extract+verify START_UTC: $(date -u +%FT%TZ)"
  cd "$ROOT"
  timeout 300 tar -xf "$ROOT/testdata/t4_red_plus_masters.tar"
  echo "=== TAR_EXIT: $?"
  python3 /home/lighthouse/astrocs_audit_v2/scripts/t4_verify.py 2>&1
  echo "=== VERIFY_EXIT: $?"
  echo "=== END_UTC: $(date -u +%FT%TZ)"
} > "$LOG/t4_transfer_step3_verify.log" 2>&1
echo TRANSFER_DONE
