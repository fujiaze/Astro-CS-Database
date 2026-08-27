#!/bin/bash
SSH="ssh -i /home/lighthouse/.ssh/id_ed25519_fatduck -o BatchMode=yes -o ConnectTimeout=20 fujia@100.104.10.71"
# 1. wait for B3 diagnostics.json (up to 50 min)
echo "WAIT_B3 start $(date -u +%H:%M)"
for i in $(seq 1 50); do
  sleep 60
  out=$($SSH "cmd /c dir /b \"F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_B.mosaic.hips\" 2>&1")
  if echo "$out" | grep -q diagnostics.json; then
    echo "B3 COMPLETE at $(date -u +%H:%M)"; echo "$out"; break
  fi
done
# 2. extract B seams
echo "EXTRACT_B start $(date -u +%H:%M)"
$SSH "cmd /c C:\\Users\\fujia\\_run_B_extract.bat" 2>&1 | tail -3
echo "EXTRACT_B done $(date -u +%H:%M)"
# 3. launch C-common
echo "LAUNCH_CCOMMON start $(date -u +%H:%M)"
$SSH "powershell -NoProfile -Command \"Start-Process -FilePath 'cmd.exe' -ArgumentList '/c','C:\\Users\\fujia\\run_fd_Ccommon.cmd' -WindowStyle Hidden\"" 2>&1 | tail -2
echo "CHAIN_DONE $(date -u +%H:%M)"
