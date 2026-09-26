#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Q9 弱零锚 zero_anchor_weight=0.001 三腿复算（路线3）。

正本: docs/science/PHASE2_UPM.md §5（求解: Huber IRLS + 弱零锚 zero_anchor_weight=0.001
      + 连通分量独立 gauge）; §7 常量场不变量（常数公共输入 => M=C, C_f=0, 弱零锚微调除外）。
实现锚: lib/algorithms/coverage/src/upm.cpp:562-564
      min Σ_k w_ik (r_ik - C_ik)^2 + λs Σ(C_ik-C_il)^2 + λ0 Σ C_ik^2,  λ0 = 0.001;
      w_ik 为 per-control 归一化的份额权重（upm.cpp:557-559 注释）。
理论腿: 逐坐标岭回归（正交设计）闭式: theta_hat = Σw·y / (Σw + λ0),
      收缩因子 rho = Σw/(Σw+λ0), 偏置 = lambda0/(Σw+lambda0) * theta_true
      （Tikhonov 正则 / Hoerl & Kennard 1970 岭回归收缩的标准结果）。
关键适用域命题（本实验验证）: 锚的"弱"依赖于权重面是【份额式】(Σw≈1)。
      若权重面为绝对 ivar（生产量纲 (ADU sr^-1)^-2, ~1e-24）, 则 lambda0=1e-3 >> Σw
      => rho→0 => 改正场被锚吞没（fail-open 到"不改正"）。锚常数的引用必须连同
      权重口径一起声明（与 D-04 的 sigma_floor 跨标度问题同族）。
负例（真值无效应⇒度量归零）: theta_true = 0 => E[theta_hat] = 0（精确）。
固定 seed: SEED = 20260926。纯 numpy。
运行: python3 exp09_zero_anchor.py
"""
import json
import os
import numpy as np

SEED = 20260926
LAM0 = 1e-3
F = 5
REPS = 2000000
OUT = os.path.join(os.path.dirname(__file__), "..", "results", "q9_zero_anchor.json")


def ridge_solution(y_sum_w, sum_w, lam0):
    """逐坐标闭式: theta_hat = Σw y / (Σw + λ0)。"""
    return sum_w * y_sum_w / (sum_w + lam0)


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED, "lambda0": LAM0, "n_frames": F, "reps": REPS, "rows": []}

    for tag, sum_w in [("share_weight_SigmaW=1", 1.0),
                       ("share_weight_per-obs_SigmaW=0.2", 0.2),
                       ("absolute_ivar_SigmaW=1e-24", 1e-24)]:
        theta_true = 1.0                      # 每帧改正真值 (任意标度)
        y = theta_true + rng.normal(0.0, 1.0, REPS)
        est = ridge_solution(y, sum_w, LAM0)
        rho = sum_w / (sum_w + LAM0)
        row = {"case": tag, "SigmaW": sum_w,
               "rho_theory": rho,
               "mean_est": float(est.mean()),
               "bias_theory": (rho - 1.0) * theta_true,
               "bias_measured": float(est.mean() - theta_true),
               "rel_bias": float((est.mean() - theta_true) / theta_true)}
        out["rows"].append(row)
        print(row)

    # 负例: theta_true = 0 => E[theta_hat] = 0
    est0 = ridge_solution(rng.normal(0.0, 1.0, REPS), 1.0, LAM0)
    out["negative_zero_effect"] = {"mean_est": float(np.mean(est0)),
                                   "ok": abs(float(np.mean(est0))) < 0.005}

    # 常数公共输入不变量 (§7): o_f = 0 => E[c_f] = 0 (期望意义), 且 c_f-c_ref 收缩一致
    yv = rng.normal(0.0, 1.0, (REPS, F))      # 公共输入, 无帧差
    w = np.full(F, 0.2)                       # 份额式
    ests = ridge_solution(yv, w, LAM0)
    out["constant_field_invariant"] = {
        "mean_c_f": ests.mean(0).tolist(),
        "max_abs_mean": float(np.abs(ests.mean(0)).max()),
        "note": "E[c_f]=0（期望归零）; 噪声实现的 c_f 有收缩后残余, 属估计噪声非系统偏置"}

    out["conclusion"] = {
        "share_weight_rel_bias": out["rows"][0]["rel_bias"],
        "absolute_ivar_rel_bias": out["rows"][2]["rel_bias"],
        "claim": "lambda0=1e-3 仅在份额式权重(Σw≈1)下是'弱'锚(偏置 0.1%); "
                 "在绝对 ivar 权重面(Σw~1e-24)下偏置 ~100% => 改正场归零。"
                 "该常数的科学含义必须与权重口径绑定声明。"}
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2,
                  default=lambda o: o.item() if hasattr(o, "item") else str(o))
    print(json.dumps(out["conclusion"], ensure_ascii=False, indent=1))
    print("saved:", os.path.abspath(OUT))


if __name__ == "__main__":
    main()