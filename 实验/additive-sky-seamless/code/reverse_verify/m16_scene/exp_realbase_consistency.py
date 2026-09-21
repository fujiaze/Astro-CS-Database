#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""M16-SCENE 判据 exp_realbase_consistency —— **真实底在合成帧里的传输正确性**。

背景（P9 首次定位的真缺陷，`run/RELEASE-02/paper/modules/P9-real-data.md` §2.6）
-----------------------------------------------------------------------------
`m16_scene.py` 原先把 `load_real_base` 返回的**速率面** [e-/s] 当**电子数**，
再整体除以曝光时间 `t`：

    src_canvas = base_rate.copy()          # 单位 = e/s
    for s in cat: R.stamp(kern, src_canvas, ..., s["flux_e"])   # 注入星单位 = e-
    src_rate = src_canvas / t              # 注释只对"星点"成立，却把真实底也除了 t
    # noise_model.expose 内部：lam_e = t*(src_e_per_s + sky_e_per_s)*m
    # ⇒ 真实底被额外衰减 x t（t = 9600 / 14400 / 16000 s）

实测后果（P9）：真实 vs 合成结构 Pearson r **0.159**、结构 sigma **0.00217 e/s**（真实 **5.37 e/s**）。

判据（**写死，不事后放宽**）
--------------------------
C1 **速率传输恒等式**（确定性，无统计噪声）：
   真实底必须以**速率**身份进入装配。由落盘帧反解：

       implied_base_rate = truth_e / (t * m) - sky - D(T)/m

   其中 m = `Frame.flat`、sky = 场景天光面、D(T) = 暗流率（热像素图默认关闭）。
   该式由 `expose` 的物理链 `lam_e = t*(src+sky)*m + t*D` 精确反解，故是**恒等式**。要求
       max |implied_base_rate / base_rate - 1| <= 1e-9   （在 base_rate 显著非零的像素上）
   **修复前**该量 = 1/t - 1 ≈ -0.99990（t=9600）⇒ 必红。
C2 **结构 sigma 同量级**：3-px 平滑后合成帧结构 sigma 与真实帧结构 sigma 之比 ∈ [0.5, 2.0]。
   （修复前实测 4.0e-4 ⇒ 必红。）
C3 **结构相关**：3-px 平滑后真实 vs 合成的 Pearson r >= 0.30（剔除合成饱和像素后再算一次）。
   （修复前实测 0.159 ⇒ 必红；P9 scratch 修正版 0.530。）

CLI::

    export TMPDIR=/dev/shm/astrocs_m16fix
    python3 实验/additive-sky-seamless/code/reverse_verify/m16_scene/exp_realbase_consistency.py --out run/RELEASE-02/paper/data/M16FIX/regression.json
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve()
RV = HERE.parents[1]                       # reverse_verify/
ROOT = HERE.parents[5]
sys.path.insert(0, str(ROOT / "实验" / "shared" / "synthetic"))

import noise_model as NM      # noqa: E402
import m16_scene as MS        # noqa: E402

SCENES = [("m16_nebula_core", "F657N"), ("m16_starfield", "F502N"),
          ("m16_dark_lowsnr", "F673N")]
SEED = 20260924
SMOOTH_PX = 3.0
C1_TOL = 1e-9
C2_RANGE = (0.5, 2.0)
C3_MIN_R = 0.30
MIN_BASE_RATE_E_S = 1e-3      # 只在该速率以上的像素上做 C1（避免 0/0）


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "run" / "RELEASE-02" / "paper" /
                                         "data" / "M16FIX" / "regression.json"))
    a = ap.parse_args()
    from astropy.io import fits
    from scipy.ndimage import gaussian_filter

    res = {"criterion": "M16 real-base transport consistency",
           "thresholds": {"C1_tol": C1_TOL, "C2_sigma_ratio_range": list(C2_RANGE),
                          "C3_min_pearson_r": C3_MIN_R, "smooth_px": SMOOTH_PX},
           "scenes": {}}
    ok_all = True
    for sid, band in SCENES:
        sc = MS.load_scene(ROOT / "实验" / "shared" / "synthetic" / "scenes" / ("%s.json" % sid))
        frame, truth, valid = MS.render_m16_frame(sc, seed=SEED, frame_index=0)
        base_rate, valid_rb, rb = MS.load_real_base(sc["real_base"], tuple(sc["shape"]))
        assert np.array_equal(valid, valid_rb)
        t = float(truth["exposure_s"])
        det = NM.Detector(**{k: v for k, v in truth["detector"].items()
                             if k in ("gain_e_per_adu", "read_noise_e", "bias_adu",
                                      "full_well_e", "dark_current_e_per_s",
                                      "dark_ref_temp_c", "dark_double_temp_c",
                                      "quantize", "saturate")})
        sky = NM.sky_surface_e_per_s(tuple(sc["shape"]),
                                     **{k: v for k, v in dict(sc.get("sky", {})).items()
                                        if k != "mode"},
                                     level_e_per_s=float(rb["pedestal_e_per_s"]))
        sky = np.where(valid, sky, 0.0)
        m = frame.flat
        dark_rate = det.dark_current_at(float(truth["temp_c"]))     # hot_map 默认 None
        with np.errstate(divide="ignore", invalid="ignore"):
            implied = frame.truth_e / (t * m) - sky - dark_rate / m
        sel = valid & (base_rate > MIN_BASE_RATE_E_S)
        ratio = implied[sel] / base_rate[sel]
        c1 = float(np.max(np.abs(ratio - 1.0))) if sel.any() else float("nan")
        c1_pass = bool(np.isfinite(c1) and c1 <= C1_TOL)
        real = np.asarray(fits.getdata(
            str(MS.MK.ROOT / "testdata" / "HST_M16" / MS.MK.BANDS[band]["file"])), dtype=np.float64)
        y0, x0, h, w = [int(v) for v in rb["crop"]]
        real = real[y0:y0 + h, x0:x0 + w]
        ok = valid & np.isfinite(real)
        real_s = gaussian_filter(np.where(ok, real, np.median(real[ok])), SMOOTH_PX)
        syn_es = (frame.adu - det.bias_adu) * det.gain_e_per_adu / t     # -> e/s
        syn_s = gaussian_filter(np.where(valid, syn_es, 0.0), SMOOTH_PX)
        sat_adu = det.saturation_adu if det.saturate else math.inf
        unsat = valid & ok & (frame.adu < sat_adu)
        r_all = float(np.corrcoef(real_s[ok], syn_s[ok])[0, 1])
        r_unsat = float(np.corrcoef(real_s[unsat], syn_s[unsat])[0, 1])
        sr = float(np.std(real_s[ok])); ss = float(np.std(syn_s[ok]))
        sig_ratio = ss / sr if sr > 0 else float("nan")
        c2_pass = bool(np.isfinite(sig_ratio) and C2_RANGE[0] <= sig_ratio <= C2_RANGE[1])
        c3_pass = bool(r_all >= C3_MIN_R)
        rec = {"band": band, "exposure_s": t, "n_px_c1": int(sel.sum()),
               "C1_max_abs_rel_dev_implied_base_rate": c1,
               "C1_implied_ratio_median": float(np.median(ratio)),
               "C1_expected_if_defect_present": 1.0 / t - 1.0,
               "C1_pass": c1_pass,
               "sigma_structure_real_e_s": sr, "sigma_structure_synth_e_s": ss,
               "C2_sigma_ratio": sig_ratio, "C2_pass": c2_pass,
               "C3_pearson_r": r_all, "C3_pearson_r_excl_saturated": r_unsat,
               "C3_pass": c3_pass,
               "saturated_fraction": float(np.mean(frame.adu >= sat_adu)),
               "saturated_pixels": int(np.count_nonzero(frame.adu >= sat_adu)),
               "adu_median": float(np.median(frame.adu[valid])),
               "sky_e_per_s": float(rb["pedestal_e_per_s"])}
        rec["pass"] = bool(c1_pass and c2_pass and c3_pass)
        ok_all = ok_all and rec["pass"]
        res["scenes"][sid] = rec
        print("[realbase] %-18s C1=%-5s (%.3e)  C2=%-5s sigma %.3f/%.3f = %.4f  "
              "C3=%-5s r=%.3f (unsat %.3f)  sat=%.4f%%"
              % (sid, c1_pass, c1, c2_pass, ss, sr, sig_ratio, c3_pass, r_all, r_unsat,
                 100.0 * rec["saturated_fraction"]), flush=True)
    res["all_pass"] = bool(ok_all)
    p = Path(a.out); p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(res, indent=1, ensure_ascii=False, default=float),
                 encoding="utf-8")
    print("[realbase] all_pass = %s -> %s" % (ok_all, p))
    return 0 if ok_all else 1


if __name__ == "__main__":
    raise SystemExit(main())
