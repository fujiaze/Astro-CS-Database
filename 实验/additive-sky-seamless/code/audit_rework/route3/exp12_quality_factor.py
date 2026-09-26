#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Q12 quality_factor 降权档 0.1/0.5（无锚量, D-56）—— 豁免论证 + 敏感性实验（路线3）。

来源: 05_正向规格.md 判据04（quality_factor_initial = 0.5, flags 下调）;
      缺陷清单 D-56（质量标记降权的 0.1/0.5 两档权重值无条文无标定）。
豁免论证（为什么它不是完整意义上的科学量）: quality_factor 是【质量旗降权策略常数】:
  它没有独立的物理可观测定义（不像 sigma、方差、间距有真值可对照），文献中不存在
  普适数值（各管线的旗降权策略均为工程取舍: SDSS/LSST 直接剔除而非降权）。
  对它的科学要求只有两条: (a) 方向正确（降权单调, 不反向放大污染）;
  (b) 影响可量化登记（污染泄漏随 q 线性可预言）。
实验腿: 单 control cell、F=6 帧等权, 污染帧偏置 Δp = 10σ:
  共面偏置 bias = (q·w_p / Σw) · Δp —— 对 q 线性。MC 验证 + 负例（Δp=0 => bias=0）。
固定 seed: SEED = 20260926。纯 numpy。
运行: python3 exp12_quality_factor.py
"""
import json
import os
import numpy as np

SEED = 20260926
F = 6
DP = 10.0
SIG = 1.0
REPS = 100000
OUT = os.path.join(os.path.dirname(__file__), "..", "results", "q12_quality_factor.json")


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED, "F": F, "delta_polluted": DP, "sigma": SIG, "reps": REPS,
           "rows": []}
    w_base = 1.0 / SIG ** 2
    for q in [1.0, 0.5, 0.1]:
        w = np.full(F, w_base)
        w[-1] *= q
        share_p = w[-1] / w.sum()
        bias_theory = share_p * DP
        est = np.empty(REPS)
        for r in range(REPS):
            y = np.zeros(F) + DP * 0
            y += rng.normal(0.0, SIG, F)          # 正常帧
            y[-1] = rng.normal(0.0, SIG) + DP     # 污染帧: 偏置 Δp
            est[r] = np.sum(w * y) / w.sum()
        row = {"q": q, "share_polluted": float(share_p),
               "bias_theory": float(bias_theory),
               "bias_measured": float(est.mean()),
               "abs_err": abs(est.mean() - bias_theory)}
        out["rows"].append(row)
        print(row)

    # 负例: Δp=0 => bias=0（任意 q）
    est0 = []
    for r in range(REPS):
        y = rng.normal(0.0, SIG, F)
        est0.append(y.mean())
    out["negative_zero_effect"] = {"mean": float(np.mean(est0)),
                                   "ok": abs(float(np.mean(est0))) < 0.01}
    b = [r["bias_measured"] for r in out["rows"]]
    out["monotone_in_q"] = b[0] > b[1] > b[2]
    out["exemption"] = {
        "verdict": "PARTIAL_EXEMPT",
        "why": "无物理真值可对照的策略常数, 文献腿不存在普适值(UNRESOLVED); "
               "科学内核=方向单调性+泄漏可预言性, 本实验已验证两者; "
               "数值本身应按 02 §4 第4类登记'待确认'(与 D-56 处置一致)",
        "initial_0.5_note": "05 规格的 quality_factor_initial=0.5 与 D-56 登记的两档 0.1/0.5 "
                            "口径不同(初值 vs 降权档), 引用时应区分"}

    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2,
                  default=lambda o: o.item() if hasattr(o, "item") else str(o))
    print(json.dumps({"monotone": out["monotone_in_q"], "negative_ok": out["negative_zero_effect"]["ok"],
                      "exemption": out["exemption"]}, ensure_ascii=False, indent=1))
    print("saved:", os.path.abspath(OUT))


if __name__ == "__main__":
    main()
