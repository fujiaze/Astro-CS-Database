#!/bin/bash
sleep 100
ssh -i /home/lighthouse/.ssh/id_ed25519_fatduck -o BatchMode=yes -o ConnectTimeout=20 fujia@100.104.10.71 "cmd /c type C:\\Users\\fujia\\C_extract.log" 2>&1 | tail -8
echo '--- A DIR ---'
ssh -i /home/lighthouse/.ssh/id_ed25519_fatduck -o BatchMode=yes -o ConnectTimeout=20 fujia@100.104.10.71 "cmd /c dir /b \"F:\Astro dev\Astro CS Normalization Database\run\temp\p2_v7\gc\audit_stage2_32f_A.mosaic.hips\"" 2>&1
echo '--- A PROC ---'
ssh -i /home/lighthouse/.ssh/id_ed25519_fatduck -o BatchMode=yes -o ConnectTimeout=20 fujia@100.104.10.71 "tasklist /fi \"imagename eq astrocs-stage2.exe\" /fo csv /nh" 2>&1
echo PROBE_DONE
