#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""A4/E1+E2：重测 `C_abs` 并给出效果预言/验证。

测量口径（正本 `docs/detail/registry/acsd.phase1.photometry.md` :127-:149）：
    `C_abs^P = blockmedian_{16 帧 T2 参考集}( synthetic_colour_median_mag 在通带 P 下 )`
其中 `synthetic_colour_median_mag` = `median_i( magG_i + 2.5·log10 F_syn,i )` = `median(z^R)`
（正本 :129 的 `median(z^R) = C^R + median(G−R)`）。

三段读数：
  reproduce — 历史 T2 基线产物（Baader R，旧键名 `zero_point_mag`）→ 必须逐位复现旧常数；
  baader    — 当前 build、16 帧 T2、`Baader R` + `Ideal QE curve` → 对照（应同旧值）；
  astro     — 当前 build、16 帧 T2、`Astrodon E-series R` + `KAF-16803` → **新常数**。

输出：results/a4_cabs/cabs_remeasure.json
"""
from __future__ import annotations

import json
import math
import statistics as st
from pathlib import Path

UNIT = Path(__file__).resolve().parents[1]
ROOT = UNIT.parents[1]
WORK = ROOT / "run/PHOT-FIX/a4_cabs"
OUT = UNIT / "results/a4_cabs"
OLD = -14.276811281840642          # 旧常数（参考通带 Baader R，正本 :149）
OLD_KEY = "zero_point_mag"         # 旧键名（正本《帧级天区合成颜色项读数键》的「旧名」）


def mad(v):
    m = st.median(v)
    return st.median([abs(x - m) for x in v])


def colour_terms(d):
    """从两段（photscale_fit / photscale_detail）取颜色项，返回 {frame: value}。"""
    out = {}
    for seg in ("photscale_fit", "photscale_detail"):
        for fk, e in (d.get(seg) or {}).items():
            v = e.get("synthetic_colour_median_mag", e.get(OLD_KEY))
            if v is not None:
                out[fk] = float(v)
    return out


def kphotos(d):
    out = {}
    for seg in ("photscale_fit", "photscale_detail"):
        for fk, e in (d.get(seg) or {}).items():
            if e.get("k_photo"):
                out[fk] = float(e["k_photo"])
    return out


def measure(path, label):
    if not Path(path).exists():
        return dict(label=label, status="**未产出**（无 p1_phot.json）")
    d = json.loads(Path(path).read_text(encoding="utf-8"))
    pi = d.get("passband_identity", {})
    c = colour_terms(d)
    k = kphotos(d)
    vals = [c[f] for f in sorted(c)]
    if not vals:
        return dict(label=label, status="**产物无颜色项读数**")
    med = st.median(vals)
    return dict(
        label=label, n_frames=len(vals), c_abs=med,
        mad=mad(vals), sd=(st.stdev(vals) if len(vals) > 1 else None),
        min=min(vals), max=max(vals), ptp=max(vals) - min(vals),
        delta_vs_old=(med - OLD),
        filter_declared=pi.get("declared_filter_passband"),
        filter_resolved=pi.get("resolved_library_key"),
        filter_curve_source=pi.get("curve_source"),
        filter_n_points=pi.get("n_points"),
        qe_resolved=(pi.get("qe") or {}).get("resolved_library_key"),
        qe_curve_source=(pi.get("qe") or {}).get("curve_source"),
        qe_n_points=(pi.get("qe") or {}).get("n_points"),
        per_frame={f: c[f] for f in sorted(c)},
        k_photo={f: k[f] for f in sorted(k) if f in k},
    )


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    res = {
        "definition": {
            "formula": "zp_inst = C_abs − 2.5·log10(k_photo)",
            "c_abs_definition": "C_abs^P = blockmedian_{参考域帧}( synthetic_colour_median_mag 在通带 P 下 )",
            "reference_domain": "16 帧 T2 参考集",
            "reference_passband_old": "Baader R",
            "old_value": OLD,
            "authority": "docs/detail/registry/acsd.phase1.photometry.md :119/:127-129/:136/:143/:147-149/:185",
        },
        "reproduce": measure(ROOT / "run/PHOT-FIX/p1_rerun/t2/p1_phot.json",
                             "历史 T2 基线产物（Baader R，旧键名）"),
        "baader": measure(WORK / "baader/p1_phot.json",
                          "当前 build · 16 帧 T2 · Baader R + Ideal QE curve"),
        "astro": measure(WORK / "astro/p1_phot.json",
                         "当前 build · 16 帧 T2 · Astrodon E-series R + KAF-16803"),
    }
    (OUT / "cabs_remeasure.json").write_text(
        json.dumps(res, indent=2, ensure_ascii=False, default=float), encoding="utf-8")

    print("== C_abs 重测（参考域 = 16 帧 T2 参考集）==")
    print("  旧常数（Baader R）= %.15f" % OLD)
    print()
    for key in ("reproduce", "baader", "astro"):
        r = res[key]
        if r.get("status"):
            print("  %-10s %s" % (key, r["status"])); continue
        print("  %-10s n=%-3d C_abs=%+.15f  MAD=%.3e  峰峰=%.3e  Δ vs 旧=%+.6f"
              % (key, r["n_frames"], r["c_abs"], r["mad"], r["ptp"], r["delta_vs_old"]))
        print("             filter=%s (%s, %s 点)  QE=%s (%s 点)"
              % (r["filter_resolved"], Path(r["filter_curve_source"] or "").name,
                 r["filter_n_points"], r["qe_resolved"], r["qe_n_points"]))
    print()
    if res["astro"].get("c_abs") is not None:
        print("  ⇒ 新常数（Astrodon E-series R）= %.15f" % res["astro"]["c_abs"])
        print("     与旧值差 = %+.9f mag（方向：%s）"
              % (res["astro"]["delta_vs_old"],
                 "更负（绝对刻度锚整体下移）" if res["astro"]["delta_vs_old"] < 0 else "更正"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
