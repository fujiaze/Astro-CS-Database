#!/usr/bin/env bash
# P5 route-1 experiment runner. Pure Python+numpy, fixed seeds, no repo imports.
set -u
cd "$(dirname "$0")"
for s in c1_median_variance c2_kcorr_drizzle c3_seam_gate c4_variance_ratio \
         c5_huber_efficiency c6_identifiability_dof c7_share_vs_abs_weight \
         c8_scale_tolerance c9_representation_boundary c10_anchor_forensics; do
    echo "== $s =="
    python3 "${s}.py" || { echo "FAILED: ${s}"; exit 1; }
done
echo "all 10 experiments OK"
