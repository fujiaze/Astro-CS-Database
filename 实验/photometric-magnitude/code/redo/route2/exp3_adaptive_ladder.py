#!/usr/bin/env python3
"""exp3_adaptive_ladder.py -- P1 route2 experiment 3: adaptive magnitude ladder & Gaia counts.

Items covered:
  S6  ladder {12,13,14,15,16}, early stop n_gaia >= 2000, loop cap 5  (05 A-5 / 01 C5)
      - counts model anchored to Gaia DR3 totals (Gaia Collaboration 2023, A&A 674, A1:
        ~1.812e9 sources) and a standard 0.36 dex/mag differential-count slope
      - which rung first reaches 2000 as a function of FOV radius and crowding
      - dead-rung negative: adding a 6th rung is silently ignored by i<5/i==4 (01/C5)
      - the "upper bound 10000" comment is never enforced (n_gaia can exceed it)

Seed fixed. Run: python3 exp3_adaptive_ladder.py
"""
import json
import math
import os

import numpy as np

SEED = 20260926

# ---- literature-anchored counts model (sky-averaged) -------------------------
# Gaia DR3: 1.812e9 sources over the whole sky (41252.96 deg^2)
N_TOTAL = 1.812e9
SKY_DEG2 = 41252.96
N_CUM_DEG2_AT_21 = N_TOTAL / SKY_DEG2     # mean cumulative density at G<=~21
ALPHA = 0.36                              # dex per mag, standard differential-count slope

def n_cum_per_deg2(g):
    """Sky-averaged cumulative sources per deg^2 brighter than G=g (model)."""
    return N_CUM_DEG2_AT_21 * 10.0 ** (ALPHA * (g - 21.0))

def n_gaia(fov_radius_deg, mag_max, crowding=1.0):
    return math.pi * fov_radius_deg ** 2 * n_cum_per_deg2(mag_max) * crowding

# ---- ladder simulation --------------------------------------------------------
LADDER = [12.0, 13.0, 14.0, 15.0, 16.0]
EARLY_STOP = 2000

def ladder_query(fov_radius_deg, mag_min=6.0, crowding=1.0, n_rungs=5, early_stop=EARLY_STOP):
    """Emulate pc_api.cpp:977-1008 loop. Returns (n_gaia, mag_used, n_queries)."""
    for i in range(min(n_rungs, 5)):          # i < 5 hard cap (loop literal)
        g = LADDER[i] if i < len(LADDER) else LADDER[-1] + (i - 4)
        n = n_gaia(fov_radius_deg, g, crowding)
        if n >= early_stop or i == 4:
            return n, g, i + 1
    return n, g, n_rungs

table = {}
for fov in [0.5, 1.0, 2.0, 3.0, 5.0, 10.0]:
    for crowd in [1.0, 10.0, 100.0]:
        n, g, q = ladder_query(fov, crowding=crowd)
        table["fov=%.1f_crowd=%.0f" % (fov, crowd)] = {
            "n_gaia_at_stop": n, "mag_max_used": g, "queries": q,
            "n_gaia_at_rung5_full": n_gaia(fov, 16.0, crowd),
        }
    print("fov %.1f done" % fov, flush=True)

# queries saved vs always-full-depth
saved = {k: 5 - v["queries"] for k, v in table.items()}

# dead-rung negative: 6-rung ladder is silently truncated by the loop cap
n6, g6, q6 = ladder_query(0.5, crowding=0.0)  # with zero stars: no early stop possible
# emulate 6 rungs where only rung 6 (G=17) would satisfy the threshold:
def ladder_query_with_6th(fov, crowding, threshold=EARLY_STOP):
    for i in range(6):                        # intent: 6 rungs {12..17}
        g = LADDER[i] if i < 5 else 17.0
        n = n_gaia(fov, g, crowding)
        if n >= threshold or i == 4:          # production-style i==4 sentinel
            return n, g, i + 1
    return n, g, 6

n_dead, g_dead, q_dead = ladder_query_with_6th(0.2, crowding=1.0)
dead_rung = {
    "fov_deg": 0.2,
    "n_at_G16_rung5": n_gaia(0.2, 16.0, 1.0),
    "n_at_G17_would_be": n_gaia(0.2, 17.0, 1.0),
    "mag_used_with_dead_6th_rung": g_dead,
    "reachability_of_G17_rung_metric": 0 if g_dead <= 16.0 else 1,
}

# "upper bound 10000" claim check: max n_gaia in a crowded 10 deg FOV
n_max = n_gaia(10.0, 16.0, 100.0)

# no-effect negative: threshold met at rung 0 => no further queries, wasted=0
n0 = n_gaia(10.0, 12.0, 1.0)
neg = {
    "n_at_rung0_fov10": n0,
    "queries_used": ladder_query(10.0)[2],
    "wasted_queries_beyond_first_stop_metric": 0,
}

out = {
    "seed": SEED,
    "model": {"N_TOTAL": N_TOTAL, "SKY_DEG2": SKY_DEG2,
              "N_CUM_PER_DEG2_AT_G21": N_CUM_DEG2_AT_21, "alpha_dex_per_mag": ALPHA,
              "anchor": "Gaia Collaboration 2023 (A&A 674, A1) source total + 0.36 dex/mag slope"},
    "n_cum_per_deg2_by_mag": {str(int(g)): n_cum_per_deg2(g) for g in range(8, 22)},
    "S6_ladder_table": table,
    "S6_queries_saved_vs_full_depth_median": float(np.median(list(saved.values()))),
    "S6_dead_rung_negative": dead_rung,
    "S6_upper_bound_10000_claim": {
        "max_n_gaia_crowded_fov10": n_max,
        "claimed_bound": 10000,
        "bound_violated_by_factor": n_max / 10000.0,
    },
    "S6_no_effect_negative": neg,
}
here = os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(here, "..", "results"), exist_ok=True)
path = os.path.join(here, "..", "results", "exp3_adaptive_ladder.json")
with open(path, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=2, ensure_ascii=False)
print("WROTE", path, flush=True)

