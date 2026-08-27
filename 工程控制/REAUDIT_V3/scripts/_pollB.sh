#!/bin/bash
SSH="ssh -i /home/lighthouse/.ssh/id_ed25519_fatduck -o BatchMode=yes -o ConnectTimeout=20 fujia@100.104.10.71"
for i in $(seq 1 40); do
  sleep 45
  n=$($SSH "powershell -NoProfile -Command \"(Get-ChildItem 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/audit_stage2_32f_B.mosaic.hips/signal' -Recurse -Filter *.fits -ErrorAction SilentlyContinue | Measure-Object).Count\"" 2>&1 | tr -d '[:space:]')
  if [ "$n" = "" ]; then n="?"; fi
  echo "poll $i: B tiles=$n"
  if [ "$n" != "?" ] && [ "$n" -ge 50 ] 2>/dev/null; then echo "DRIZZLE_PROGRESSING"; break; fi
done
echo PW_END
