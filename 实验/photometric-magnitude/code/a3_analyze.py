#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""A3 通带声明对照实验：天区一致性断言 + 主判据（逐星残差散度）读数。

主判据口径（合同指定）：`docs/engineering/contracts/CONFIG.md` 的「响应曲线库」一节——
**单帧测光一致性残差是通带声明正确性的机器判据**。读数取产物既有字段
`p1_phot.json` 的 `photscale_detail[<帧>].sigma_residual_dex`（= `MAD(r)/0.6744897501960817`，
`r = log10(F_instr/F_syn)`，见 `PHOTOMETRY.md` §3）。本脚本只读、不重算拟合、不改产物。

天区一致性（§13.1）：每帧 CRVAL 从**该帧产品** `p1_wcs.json` 读出；同一臂内 CRVAL 离散
超容差即判失败。

输出：results/a3_curves/{sky_assert.json, a3_readings.json}
"""
from __future__ import annotations

import json
import math
import statistics as st
import sys
from pathlib import Path

UNIT = Path(__file__).resolve().parents[1]
ROOT = UNIT.parents[1]
W = ROOT / "run/PHOT-FIX/a3_curves"
OUT = UNIT / "results/a3_curves"
MAD_SCALE = 0.6744897501960817
CRVAL_TOL_DEG = 0.05          # 天区一致性容差（视场约 1.1°，取 ~5% 视场）

ARMS = [(1, "历史口径", "Baader R", "Ideal QE curve"),
        (2, "只修滤镜", "Astrodon E-series R", "Ideal QE curve"),
        (3, "只修 QE", "Baader R", "KAF-16803"),
        (4, "全修（目标）", "Astrodon E-series R", "KAF-16803")]


def mad(v):
    if len(v) < 2:
        return float("nan")
    m = st.median(v)
    return st.median([abs(x - m) for x in v])


def se_median(v):
    """中位数的渐近标准误 1.2533·std/√n。"""
    if len(v) < 2:
        return float("nan")
    return 1.2533 * st.stdev(v) / math.sqrt(len(v))


def load_arm(a):
    p = W / ("arm%d_pinned" % a) / "p1_phot.json"   # 钉住 build 87701c6f 的四臂
    if not p.exists():
        return None
    return json.loads(p.read_text(encoding="utf-8"))


def frame_scale(d):
    """→ {frame_key: {...}}，取自 photscale_detail（已施加帧的标度证据）。"""
    return d.get("photscale_detail", {}) or {}


def crval_of(a, fk):
    p = W / ("arm%d_pinned" % a) / fk / "p1_wcs.json"
    if not p.exists():
        return None
    w = (json.loads(p.read_text(encoding="utf-8")).get("wcs") or {})
    if w.get("crval1") is None:
        return None
    return (float(w["crval1"]), float(w["crval2"]))


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    arms_meta = json.loads((W / "arms.json").read_text(encoding="utf-8"))
    frames = [Path(f).name for f in arms_meta["frames"]]

    sky, readings = {}, {}
    for a, label, filt, qe in ARMS:
        d = load_arm(a)
        if d is None:
            readings[a] = dict(arm=a, label=label, filter=filt, qe=qe,
                               status="**未产出**（该臂无 p1_phot.json）")
            continue
        psd = frame_scale(d)
        rows = []
        for fk in sorted(psd):
            e = psd[fk]
            rows.append(dict(
                frame=fk,
                sigma_residual_dex=e.get("sigma_residual_dex"),
                k_photo=e.get("k_photo"),
                n_matched=e.get("n_matched"),
                synthetic_colour_median_mag=e.get("synthetic_colour_median_mag"),
                synthetic_colour_valid=e.get("synthetic_colour_valid"),
                synthetic_colour_n_stars=e.get("synthetic_colour_n_stars"),
                crval=crval_of(a, fk),
            ))
        sig = [r["sigma_residual_dex"] for r in rows if r["sigma_residual_dex"] is not None]
        lk = [math.log10(r["k_photo"]) for r in rows if r["k_photo"]]
        col = [r["synthetic_colour_median_mag"] for r in rows
               if r["synthetic_colour_valid"] and r["synthetic_colour_median_mag"] is not None]
        readings[a] = dict(
            arm=a, label=label, filter=filt, qe=qe, n_frames=len(rows), frames=rows,
            sigma_residual_dex=dict(n=len(sig), median=st.median(sig) if sig else None,
                                    mad=mad(sig) if sig else None,
                                    se_median=se_median(sig) if sig else None,
                                    min=min(sig) if sig else None,
                                    max=max(sig) if sig else None),
            log10_k_photo=dict(n=len(lk), median=st.median(lk) if lk else None,
                               sd=st.stdev(lk) if len(lk) > 1 else None,
                               ptp=(max(lk) - min(lk)) if lk else None),
            synthetic_colour_median_mag=dict(n=len(col),
                                             median=st.median(col) if col else None,
                                             sd=st.stdev(col) if len(col) > 1 else None,
                                             ptp=(max(col) - min(col)) if col else None),
        )
        # 天区断言（每臂独立）
        crs = [r["crval"] for r in rows if r["crval"]]
        if crs:
            ra = [c[0] for c in crs]; dec = [c[1] for c in crs]
            span = max(max(ra) - min(ra), max(dec) - min(dec))
            sky[a] = dict(n=len(crs), ra_span=float(max(ra) - min(ra)),
                          dec_span=float(max(dec) - min(dec)), span=float(span),
                          tol=CRVAL_TOL_DEG, pass_=bool(span <= CRVAL_TOL_DEG))

    # ①→④ 变化量
    d14 = None
    if 1 in readings and 4 in readings and "sigma_residual_dex" in readings[1] \
            and readings[1]["sigma_residual_dex"]["median"] is not None \
            and readings[4]["sigma_residual_dex"]["median"] is not None:
        m1 = readings[1]["sigma_residual_dex"]["median"]
        m4 = readings[4]["sigma_residual_dex"]["median"]
        s1 = readings[1]["sigma_residual_dex"]["se_median"]
        s4 = readings[4]["sigma_residual_dex"]["se_median"]
        d14 = dict(median_arm1=m1, median_arm4=m4, delta=m4 - m1,
                   rel=(m4 - m1) / m1 if m1 else None,
                   se_combined=math.hypot(s1, s4) if (s1 and s4) else None,
                   n_sigma=((m4 - m1) / math.hypot(s1, s4)) if (s1 and s4) else None)

    out = dict(frames=frames, sky_assert=sky, readings=readings, arm1_to_arm4=d14,
               criterion="p1_phot.json photscale_detail[].sigma_residual_dex（合同指定判据）",
               note="四臂只变曲线声明；帧集与其余配置逐字不动。")
    (OUT / "sky_assert.json").write_text(
        json.dumps(dict(tol_deg=CRVAL_TOL_DEG, per_arm=sky), indent=2, ensure_ascii=False),
        encoding="utf-8")
    (OUT / "a3_readings.json").write_text(
        json.dumps(out, indent=2, ensure_ascii=False, default=float), encoding="utf-8")

    print("== A3 天区一致性（容差 %.3f°）==" % CRVAL_TOL_DEG)
    for a, v in sorted(sky.items()):
        print("  arm%d  n=%d  RA 跨距=%.5f°  Dec 跨距=%.5f°  %s"
              % (a, v["n"], v["ra_span"], v["dec_span"], "PASS" if v["pass_"] else "**FAIL**"))
    print()
    print("== 主判据：sigma_residual_dex（逐臂）==")
    print("  %-4s %-14s %-22s %-10s %-10s %-10s %s"
          % ("arm", "label", "filter/QE", "median", "MAD", "SE", "n"))
    for a, label, filt, qe in ARMS:
        r = readings.get(a, {})
        s = r.get("sigma_residual_dex")
        if not s or s["median"] is None:
            print("  %-4d %-14s %-22s %s" % (a, label, filt, r.get("status", "无读数")))
            continue
        print("  %-4d %-14s %-22s %-10.6f %-10.6f %-10.6f %d"
              % (a, label, filt + "/" + qe, s["median"], s["mad"], s["se_median"], s["n"]))
    print()
    if d14:
        print("①→④: %.6f → %.6f  Δ=%+.6f (%+.2f%%)  SE=%.6f  %s"
              % (d14["median_arm1"], d14["median_arm4"], d14["delta"],
                 100 * d14["rel"], d14["se_combined"],
                  ("%.1fσ" % d14["n_sigma"]) if d14["n_sigma"] is not None else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
