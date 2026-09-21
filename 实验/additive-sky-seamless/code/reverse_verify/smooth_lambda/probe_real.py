#!/usr/bin/env python3
"""Probe real L4 p2_samples.json: geometry, scales, per-control frame scatter.
Read-only. Output: run/reverse_verify/smooth_lambda/real_probe.json
"""
import json, os, sys, math
import numpy as np

ROOT = "/workspace/Astro CS Database"
SRC = os.path.join(ROOT, "run/RELEASE-02/L4-rebuild/mosaic_out/p2_samples.json")
OUT = os.path.join(ROOT, "run/reverse_verify/smooth_lambda/real_probe.json")

d = json.load(open(SRC))
obs = d["observations"]; ctrls = d["controls"]
n_obs = len(obs); n_ctrl = len(ctrls); n_fr = len(d["frame_ids"])
print("n_obs", n_obs, "n_ctrl", n_ctrl, "n_frames", n_fr)

val = np.array([o["value"] for o in obs], float)
unc = np.array([o["uncertainty"] for o in obs], float)
civ = np.array([o["control_ivar"] for o in obs], float)
sup = np.array([o["support"] for o in obs], float)
qf  = np.array([o["quality_flags"] for o in obs], int)
cid = np.array([o["control_id"] for o in obs], np.int64)
fid = np.array([o["frame_id"] for o in obs], np.uint64)

def q(a, ps=(0,1,5,25,50,75,95,99,100)):
    return {str(p): float(np.percentile(a, p)) for p in ps}

res = {
 "n_obs": n_obs, "n_controls": n_ctrl, "n_frames": n_fr,
 "stats": d["stats"],
 "value": q(val), "uncertainty": q(unc), "control_ivar": q(civ),
 "support": q(sup), "quality_flags_unique": sorted(set(qf.tolist()))[:10],
}
# cell coverage count
import collections
cnt = collections.Counter(cid.tolist())
cc = np.array([cnt.get(i,0) for i in range(n_ctrl)], int)
res["frames_per_control_hist"] = {int(k): int(v) for k,v in sorted(collections.Counter(cc.tolist()).items())}

# frame-to-frame scatter per control (>=2 frames)
order = np.lexsort((fid, cid))
c_s = cid[order]; v_s = val[order]; u_s = unc[order]
uniq, starts = np.unique(c_s, return_index=True)
scatter = []; nfr_cell = []
for i, st in enumerate(starts):
    en = starts[i+1] if i+1 < len(starts) else len(c_s)
    vv = v_s[st:en]
    if len(vv) < 2: continue
    scatter.append(float(np.std(vv, ddof=1)))
    nfr_cell.append(len(vv))
scatter = np.array(scatter)
res["frame_scatter_per_control"] = q(scatter)
res["frame_scatter_over_value_median"] = float(np.median(scatter) / max(1e-30, np.median(val)))

# implied sigma_bg from control_variance: cv = k_corr*(pi/2)*sigma_bg^2/N_retained
kc = 1.4
cvar = 1.0/np.maximum(civ, 1e-300)
sig2_over_N = cvar/(kc*(math.pi/2.0))
res["sigma_bg2_over_N"] = q(sig2_over_N)
res["implied_sigma_bg_if_N4096"] = q(np.sqrt(sig2_over_N*4096))
# angular scale: leaf tiles 512 px, tile ~ 512*0.9586 arcsec? grid 8 -> cell 64 px
res["ra_range"] = [float(min(c["ra_deg"] for c in ctrls)), float(max(c["ra_deg"] for c in ctrls))]
res["dec_range"] = [float(min(c["dec_deg"] for c in ctrls)), float(max(c["dec_deg"] for c in ctrls))]
res["n_tiles"] = len(set(c["tile_ipix"] for c in ctrls))
# bright cells: value percentile
res["value_p99"] = float(np.percentile(val, 99))
res["bright_frac_gt_3x_median"] = float(np.mean(val > 3*np.median(val)))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(res, open(OUT, "w"), indent=1)
print(json.dumps(res, indent=1)[:4000])
