#!/bin/bash
sleep 250
SSH="ssh -i /home/lighthouse/.ssh/id_ed25519_fatduck -o BatchMode=yes -o ConnectTimeout=20 fujia@100.104.10.71"
echo "--- time $(date -u +%H:%M) ---"
echo "B tiles=$($SSH "powershell -NoProfile -Command \"(Get-ChildItem 'F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/audit_stage2_32f_B.mosaic.hips/signal' -Recurse -Filter *.fits -ErrorAction SilentlyContinue | Measure-Object).Count\"" 2>&1)"
$SSH "powershell -NoProfile -ExecutionPolicy Bypass -File C:\\Users\\fujia\\_checkB.ps1" 2>&1
$SSH "cmd /c type C:\\Users\\fujia\\stage2_32f_B3.log" 2>&1 | tail -2
$SSH "cmd /c dir /b \"F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_B.mosaic.hips\"" 2>&1
echo CHK_END
