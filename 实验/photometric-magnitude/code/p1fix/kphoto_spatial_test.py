#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1 ③ 判别：帧级标度是否把帧内空间项混进了帧级标量。

口径（先读，照抄正本）：
  · docs/detail/registry/acsd.phase1.photometry.md：`k_photo` = 帧级线性标度
    （`scale = 10^(−location)`）；`m(x,y)` = 与其并列的低阶乘性空间场。
  · docs/science/photometry/PHOTOMETRY.md：规范 = 拟合星集上 `log10 m` 的
    （Tukey 权重）加权均值为 0；`k_photo` 仍承担全局项；order=0（m≡1）与标量逐位一致。
  · docs/detail/normalize/pipeline.md：切实现在阶段一调度适配层；阶数 3 被夹到 2。

两种口径（同一批帧、同一批星样本、不调参、不换判据）：
  (a) 现状口径（含空间场）= 产物口径：location_a = −log10(k_photo)（直接取自产物）
  (b) 去掉空间场后仅用帧级线性标度：location_b = 帧内 `r` 的 Tukey-IRLS 稳健位置
      （r = log10(F_instr/F_syn)，只用该帧自身星点）

判读：
  · 规范把 m 约束成零均值 ⇒ 若 (b) 与 (a) 逐帧几乎相同，则空间项**无法**进入帧级标量，
    ③ 的机制不成立，帧间散度另有来源；
  · 分别给两者的帧间散度、逐帧标准误（自助法）、散度/标准误倍数，以及尾部贡献分布。

只读 run/PHOT-FIX/**；输出 results/p1fix/kphoto_spatial_test.json
"""
from __future__ import annotations
import json, math, re
from pathlib import Path
from datetime import datetime
import numpy as np

ROOT = Path(__file__).resolve().parents[4]
SRC = ROOT / "run/PHOT-FIX/p1_rerun"
OUT = ROOT / "实验/photometric-magnitude/results/p1fix"
MAD = 0.6744897501960817
C_TUKEY = 4.685
NBOOT = 200


def tukey_location(r, iters=50, tol=1e-6):
    loc = float(np.median(r))
    for _ in range(iters):
        S = float(np.median(np.abs(r - loc)) / MAD)
        if S <= 0:
            break
        u = (r - loc) / (C_TUKEY * S)
        w = np.where(np.abs(u) < 1, (1 - u * u) ** 2, 0.0)
        if w.sum() <= 0:
            break
        new = float((w * r).sum() / w.sum())
        if abs(new - loc) < tol:
            return new
        loc = new
    return loc


def parse_dt(fk):
    m = re.search(r"-(\d{8})_(\d{6})-", fk)
    if not m:
        return None
    d, t = m.group(1), m.group(2)
    return datetime(int(d[:4]), int(d[4:6]), int(d[6:8]), int(t[:2]), int(t[2:4]), int(t[4:6]))


def robust_sigma(v):
    return float(np.median(np.abs(v - np.median(v))) / MAD)


def main():
    rows = []
    for tag in ("t2", "t3"):
        d = json.loads((SRC / tag / "p1_phot.json").read_text())
        for fk in sorted(d["spatial_gain"]):
            sg = d["spatial_gain"][fk]; pd = d["photscale_detail"][fk]
            s = sg.get("samples", {})
            if not s.get("x"):
                continue
            r = np.array(s["r_log10_finstr_over_fsyn"]); used = np.array(s["used"], bool)
            kp = pd.get("k_photo")
            if not (kp and kp > 0):
                continue
            rv = r[used]
            loc_a = -math.log10(kp)                      # (a) 含空间场的产物口径
            loc_b = tukey_location(rv)                   # (b) 去掉空间场，仅帧级标度
            rng = np.random.default_rng(20260101)
            boots = np.empty(NBOOT)
            for b in range(NBOOT):
                boots[b] = tukey_location(rv[rng.integers(0, len(rv), len(rv))])
            rows.append({
                "tag": tag, "frame": fk,
                "k_photo": kp, "loc_a_product": loc_a, "loc_b_no_spatial": loc_b,
                "delta_b_minus_a": loc_b - loc_a,
                "field_ptp_dex": sg.get("field_ptp_dex"),
                "field_rms_dex": sg.get("field_rms_dex"),
                "n_used": int(used.sum()),
                "se_boot_b": float(np.std(boots, ddof=1)),
                "se_analytic_b": float(robust_sigma(rv) / math.sqrt(len(rv))),
                "dt": parse_dt(fk).isoformat() if parse_dt(fk) else None,
                "date": parse_dt(fk).strftime("%Y%m%d") if parse_dt(fk) else None,
            })
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "kphoto_spatial_test.json").write_text(json.dumps(rows, indent=1, ensure_ascii=False))

    a = np.array([q["loc_a_product"] for q in rows])
    b = np.array([q["loc_b_no_spatial"] for q in rows])
    dl = b - a
    se = np.array([q["se_boot_b"] for q in rows])
    print(f"frames = {len(rows)}")
    print()
    print("=== (a) 含空间场（产物口径） vs (b) 去掉空间场（仅帧级标度）===")
    print("  逐帧差 Δ=(b−a): 中位 %+.3e  min %+.3e  max %+.3e dex" %
          (np.median(dl), dl.min(), dl.max()))
    print("  |Δ| 中位 %.3e dex ; 而帧间散度为 %.4f dex ⇒ 比值 %.1f" %
          (np.median(np.abs(dl)), robust_sigma(b), robust_sigma(b) / max(np.median(np.abs(dl)), 1e-12)))
    print()
    for name, v in (("(a) 含空间场", a), ("(b) 去空间场", b)):
        sd = float(np.std(v, ddof=1)); rs = robust_sigma(v)
        print("  %-12s SD=%.6f dex  MAD/0.6745=%.6f dex  峰峰=%.6f" % (name, sd, rs, v.max() - v.min()))
    print()
    print("  (b) 的散度 / 自助法标准误：")
    print("    典型标准误(中位) = %.6f dex ; 散度(SD)=%.6f ⇒ %.1f 倍 ; 散度(MAD)=%.6f ⇒ %.1f 倍" %
          (np.median(se), float(np.std(b, ddof=1)), float(np.std(b, ddof=1)) / np.median(se),
           robust_sigma(b), robust_sigma(b) / np.median(se)))
    chi2 = float(np.sum(((b - b.mean()) / se) ** 2) / (len(b) - 1))
    print("    χ²_red(b) = %.1f (dof=%d)" % (chi2, len(b) - 1))
    print()
    print("=== 尾部贡献分布（|loc − 中位| 排序，前 8）===")
    med = np.median(b)
    order = np.argsort(-np.abs(b - med))
    for i in order[:8]:
        q = rows[i]
        print("  %-46s date=%s loc_b=%+.5f  dev=%+.5f dex  n_used=%4d  field_ptp=%.3f" %
              (q["frame"][:46], q["date"], b[i], b[i] - med, q["n_used"], q["field_ptp_dex"] or 0))
    print()
    print("=== 空间场幅度 vs 帧级标度（相关）===")
    fp = np.array([q["field_ptp_dex"] or 0.0 for q in rows])
    from scipy import stats
    r_, p_ = stats.pearsonr(fp, b)
    print("  corr(field_ptp_dex, loc_b) = %+.3f (p=%.3g, n=%d)" % (r_, p_, len(b)))
    print("  field_ptp_dex: 中位 %.4f  min %.4f  max %.4f dex" % (np.median(fp), fp.min(), fp.max()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
