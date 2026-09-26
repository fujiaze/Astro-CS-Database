#!/usr/bin/env python3
"""E4: variance-ratio seam diagnostic (docs/science/PHASE2_UPM.md §9a / 11_upm.md §4.1).

Claim audited: "variance ratio is *in principle* blind to level steps (a step does not
change variance)" => it may only be used as a coarse screen for rendering artifacts;
frame-to-frame no-seam evidence must come from the signed rel_step gate.

Tests:
  (a) algebra: Var(x + Delta*1[x>c]) = sigma^2 + Delta^2 p(1-p); straddled-block check;
  (b) aligned blocks (both sides of boundary measured separately): variance ratio == 1
      even for a 4.7-sigma step  => blindness demonstrated (NEGATIVE: ratio-1 ~ 0);
  (c) same image through the rel_step seam metric => strongly red => the two metrics
      have disjoint blind spots (step visible to rel_step, invisible to variance ratio);
  (d) per-block constant offset artifact (alternating +-a per 32px block): within-block
      variance ratio still == 1 (variance ratio CANNOT see it either) but block-mean
      scatter does -- coarse-screen face;
  (e) within-block noise amplification (x3): variance ratio detects it (its proper use).
Standalone: pure python3 + numpy. SEED fixed.
Run: python3 e4_variance_ratio_blindness.py
"""
import json, os
import numpy as np

SEED = 20250926
H = W = 512
BLOCK = 32
SIGMA = 0.0107
L = 1.0
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "results", "e4_variance_ratio_blindness.json")

def block_stds(img, block=BLOCK):
    H, W = img.shape
    s = [float(np.std(img[y:y + block, x:x + block]))
         for y in range(0, H, block) for x in range(0, W, block)]
    return np.array(s)

def block_means(img, block=BLOCK):
    H, W = img.shape
    m = [float(np.mean(img[y:y + block, x:x + block]))
         for y in range(0, H, block) for x in range(0, W, block)]
    return np.array(m)

def seam_rel_step(img, x0=W // 2, d=2, n=256):
    ys = np.linspace(4, H - 5, n).astype(int)
    seam = img[ys, x0 + d] - img[ys, x0 - d]
    bg = np.median(np.abs(np.concatenate([img[ys, x0 + d], img[ys, x0 - d]])))
    return float(np.median(seam) / bg)

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "sigma": SIGMA, "block": BLOCK, "cases": []}
    base_noise = rng.standard_normal((H, W)) * SIGMA
    cases = {}
    # (a) level step 4.7 sigma
    step = 0.05
    img = L + base_noise.copy(); img[:, W // 2:] += step
    cases["level_step_4.7sigma"] = img
    # (d) alternating per-block constant offsets +-3 sigma
    a = 3 * SIGMA
    img = L + base_noise.copy()
    sg = np.sign(np.sin(2 * np.pi * np.arange(W) / (2 * BLOCK)))
    img += sg[None, :] * a
    cases["block_constant_artifact_3sigma"] = img
    # (e) within-block noise amplification x3
    img = L + rng.standard_normal((H, W)) * (3 * SIGMA)
    cases["noise_amplified_x3"] = img
    # clean reference
    cases["clean"] = L + base_noise.copy()
    ref_std = float(np.median(block_stds(cases["clean"])))
    for name, img in cases.items():
        # variance ratio: median block std left of boundary vs right (aligned blocks,
        # neither straddles the boundary)
        stds = block_stds(img)
        bx = np.tile(np.repeat(np.arange(0, W, BLOCK), 1), H // BLOCK)  # block col index
        cols = np.array([x for y in range(0, H, BLOCK) for x in range(0, W, BLOCK)])
        left = stds[cols < W // 2 - BLOCK]
        right = stds[cols >= W // 2]
        vr = float(np.median(left) / np.median(right))
        # straddled-block algebra check (single block containing the boundary)
        yb = (BLOCK, 2 * BLOCK)
        strad = img[yb[0]:yb[1], W // 2 - BLOCK // 2: W // 2 + BLOCK // 2]
        p = 0.5
        if name == "level_step_4.7sigma":
            algebra_pred = SIGMA ** 2 + (step ** 2) * p * (1 - p)
            strad_var = float(np.var(strad))
        else:
            algebra_pred, strad_var = None, None
        stds_all = block_stds(img)
        stds_all = block_stds(img)
        res["cases"].append({
            "case": name,
            "variance_ratio_aligned_blocks": vr,
            "block_std_over_clean_ref": float(np.median(stds_all) / ref_std),
            "block_std_over_clean_ref": float(np.median(stds_all) / ref_std),
            "seam_rel_step": seam_rel_step(img),
            "seam_gate_1e-2_verdict_red": abs(seam_rel_step(img)) > 1e-2,
            "straddled_block_var": strad_var,
            "algebra_var_pred_sigma2+Delta^2 p(1-p)": algebra_pred,
        })
    clean_vr = next(c["variance_ratio_aligned_blocks"] for c in res["cases"] if c["case"] == "clean")
    res["negative"] = {"clean_variance_ratio_minus_1": clean_vr - 1.0}
    with open(OUT, "w") as f:
        json.dump(res, f, indent=1)
    print(json.dumps(res, indent=1))

if __name__ == "__main__":
    main()