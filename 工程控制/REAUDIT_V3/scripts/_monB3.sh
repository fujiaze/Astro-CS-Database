#!/bin/bash
SSH="ssh -i /home/lighthouse/.ssh/id_ed25519_fatduck -o BatchMode=yes -o ConnectTimeout=20 fujia@100.104.10.71"
for i in $(seq 1 40); do
  sleep 60
  out=$($SSH "cmd /c dir /b \"F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_B.mosaic.hips\" 2>&1")
  if echo "$out" | grep -q diagnostics.json; then
    echo "B3 COMPLETE"; echo "$out"; break
  fi
  log=$($SSH "cmd /c type C:\\Users\\fujia\\stage2_32f_B3.log" 2>&1 | tail -1)
  echo "poll $i: $log"
done
echo MON_END
