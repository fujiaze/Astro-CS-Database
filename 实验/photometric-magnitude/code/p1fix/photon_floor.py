#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-FIX 诊断 P-A(3)：把「空间场后残差」拆成 逐星测光噪声 vs 空间场不足。

动机（见 docs/p1fix_process.md §9.2）：
  产物 `noise_floor_dex` = S·sqrt(mean_pix(B̃ᵀ H⁻¹ B̃))，**只含空间场系数估计**的统计误差，
  **不含逐星测光误差**。而 `sigma_spatial_residual_dex` 是全部逐星残差的 MAD。
  因此 sigma_post/noise_floor 天然 > 1，**倍数本身不能证明"空间场不够"**。

本脚本给出独立参照：用 p1_flux.json 的 flux/flux_error 算**逐星光子噪声**在 dex 域的预测
  sigma_photon,i = 0.4342944819 · (flux_error_i / flux_i)
取其中位数，与 sigma_post 比较。

判据（本脚本只报读数，不判红）：
  · 若 median(sigma_photon) ≈ sigma_post ⇒ 残差已到逐星噪声底，**空间场不再有可去除结构**；
  · 若 median(sigma_photon) ≪ sigma_post ⇒ 残差中仍有超出逐星噪声的成分，才谈得上改模型。

只读 run/PHOT-FIX/**；输出 results/p1fix/photon_floor.json。
"""
from __future__ import annotations
import json, math, sys
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[4]
SRC = ROOT / "run/PHOT-FIX/p1_rerun"
OUT = ROOT / "实验/photometric-magnitude/results/p1fix"
DEX = 0.4342944819032518
MAD_SCALE = 0.6744897501960817


def main():
    out = []
    for tag in ("t2", "t3"):
        phot = json.loads((SRC / tag / "p1_phot.json").read_text())
        flux = json.loads((SRC / tag / "p1_flux.json").read_text())
        # 逐帧 file -> results
        fmap = {}
        for fr in flux["frames"]:
            fmap[fr["file"]] = fr["results"]
        for fk in sorted(phot["spatial_gain"]):
            sg = phot["spatial_gain"][fk]
            s = sg["samples"]
            if not s.get("x"):
                continue
            sx = np.array(s["x"]); sy = np.array(s["y"])
            used = np.array(s["used"], bool)
            # 找该帧的 flux results（file 名含帧名）
            res = None
            for k, v in fmap.items():
                if fk in k or k.replace("cleaned_", "").split("@")[0] in fk:
                    res = v
                    break
            if res is None:
                out.append({"tag": tag, "frame": fk, "error": "flux frame not matched"})
                continue
            fx = np.array([r["x"] for r in res]); fy = np.array([r["y"] for r in res])
            fl = np.array([r.get("flux") or np.nan for r in res])
            fe = np.array([r.get("flux_error") or np.nan for r in res])
            # 最近邻匹配（样本 -> flux 源）
            sig = []
            for i in range(len(sx)):
                if not used[i]:
                    continue
                d2 = (fx - sx[i]) ** 2 + (fy - sy[i]) ** 2
                j = int(np.argmin(d2))
                if d2[j] > 25.0:      # 5 px 容差
                    continue
                if not (np.isfinite(fl[j]) and np.isfinite(fe[j]) and fl[j] > 0 and fe[j] > 0):
                    continue
                sig.append(DEX * fe[j] / fl[j])
            if not sig:
                out.append({"tag": tag, "frame": fk, "error": "no matched flux error"})
                continue
            sig = np.array(sig)
            sig_med = float(np.median(sig))
            sig_lo = float(np.percentile(sig, 25))
            sig_hi = float(np.percentile(sig, 75))
            post = sg.get("sigma_spatial_residual_dex")
            raw = None
            for r_ in phot.get("frames", []):
                pass
            out.append({
                "tag": tag, "frame": fk,
                "n_used": int(used.sum()), "n_flux_matched": int(len(sig)),
                "sigma_photon_med_dex": sig_med,
                "sigma_photon_p25_dex": sig_lo,
                "sigma_photon_p75_dex": sig_hi,
                "sigma_post_dex": post,
                "noise_floor_dex": sg.get("noise_floor_dex"),
                "ratio_post_over_photon": (post / sig_med) if (post and sig_med) else None,
                "ratio_photon_over_noisefloor": (sig_med / sg["noise_floor_dex"]) if sg.get("noise_floor_dex") else None,
            })
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "photon_floor.json").write_text(json.dumps(out, indent=1, ensure_ascii=False))
    ok = [r for r in out if "error" not in r]
    print(f"frames={len(out)}  ok={len(ok)}")
    if ok:
        r1 = [r["ratio_post_over_photon"] for r in ok if r["ratio_post_over_photon"]]
        r2 = [r["ratio_photon_over_noisefloor"] for r in ok if r["ratio_photon_over_noisefloor"]]
        print("sigma_post / sigma_photon      : min=%.2f median=%.2f max=%.2f" % (min(r1), sorted(r1)[len(r1)//2], max(r1)))
        print("sigma_photon / noise_floor     : min=%.2f median=%.2f max=%.2f" % (min(r2), sorted(r2)[len(r2)//2], max(r2)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
