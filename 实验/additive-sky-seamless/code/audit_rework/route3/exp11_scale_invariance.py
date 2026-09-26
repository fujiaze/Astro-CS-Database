#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Q11 跨标度判据失效: sigma_floor=1e-3 恒不生效 / 绝对容差 1e-6 原理不可达（路线3）。

正本: 缺陷清单 D-04（upm.cpp:292 sigma_floor=1e-3 与消费侧面亮度标度差 13 个量级 => 地板
      恒不生效; uncertainty=+inf => z=0 => huber_w=1 满权）; D-54（绝对容差在生产尺度
      scale_obs=5.262929e13 下 = 8.557e-5 x ULP, 原理不可达）;
      docs/science/PHASE2_UPM.md §5（生产必须 tolerance_relative=1）。
理论腿: 双精度浮点的标度律 —— 量 x 的 ULP = 2^(floor(log2 x) - 52);
      绝对阈值 t 在标度 S 下可达 <=> t >= ULP(S); 相对阈值恒可达（尺度不变）。
实验腿（解析算术复算 + 数值演示, 无随机量; seed 仍写死合规）:
  (a) sigma_floor 三种 regime 的 z 与 huber_w;
  (b) 生产尺度的 ULP 与绝对容差比; 模拟收敛序列演示绝对门永不触发、相对门触发;
  (c) 标度≈1 的合成数据: 绝对 1e-6 可达（等价域登记）。
负例（真值无效应⇒度量归零）: 改正量真值=0 => max_dM=0 同时满足两种判据（归零一致）。
固定 seed: SEED = 20260926。纯 numpy。
运行: python3 exp11_scale_invariance.py
"""
import json
import math
import os
import numpy as np

SEED = 20260926
SCALE_OBS = 5.262929e13      # PHASE2_UPM §5: M42 天空 1210 ADU/px / Ω_px 2.2991e-11 sr
SIGMA_FLOOR = 1e-3           # upm.cpp:292
ABS_TOL = 1e-6
OUT = os.path.join(os.path.dirname(__file__), "..", "results", "q11_scale_invariance.json")

DELTA = 1.345


def huber_w(z):
    az = abs(z)
    return 1.0 if az <= DELTA else DELTA / az


def main():
    out = {"seed": SEED, "scale_obs": SCALE_OBS}

    # ---- (a) sigma_floor 的三种 regime ----
    rows = []
    for tag, unc, resid in [
            ("production_surface_brightness", 4.3e11, 1.0e11),   # M42 量纲 (ADU sr^-1)
            ("uncertainty_inf", math.inf, 1.0e11),
            ("tiny_alpha2_scale", 1e-30, 1e-29)]:
        sigma_eff = max(abs(unc), SIGMA_FLOOR) if math.isfinite(unc) else abs(unc)
        z = resid / sigma_eff if math.isfinite(sigma_eff) else 0.0
        rows.append({"case": tag, "uncertainty": unc, "residual": resid,
                     "sigma_eff": sigma_eff, "z": z, "huber_w": huber_w(z),
                     "floor_active": math.isfinite(unc) and abs(unc) < SIGMA_FLOOR})
    out["sigma_floor_regimes"] = rows
    out["floor_dead_production"] = rows[0]["floor_active"] is False and rows[0]["huber_w"] == 1.0
    out["inf_gains_full_weight"] = rows[1]["huber_w"] == 1.0
    out["floor_dead_tiny_scale"] = rows[2]["huber_w"] == 1.0

    # 相对地板: floor_rel = 1e-3 * scale_obs
    floor_rel = 1e-3 * SCALE_OBS
    z_rel = 1.0e11 / floor_rel
    out["relative_floor_active"] = {"floor_rel": floor_rel, "z_example": z_rel,
                                    "huber_w": huber_w(z_rel),
                                    "in_delta_region": abs(z_rel) <= DELTA}

    # ---- (b) 绝对容差不可达 ----
    x = SCALE_OBS
    ulp = np.nextafter(x, math.inf) - x
    out["absolute_tolerance"] = {
        "ulp_at_scale": float(ulp),
        "abs_tol_over_ulp": ABS_TOL / float(ulp),
        "unreachable": ABS_TOL < float(ulp),
        "d54_recorded_ratio": 8.557e-5}
    # 收敛序列演示: 相对改善 1e-9/步, 判 max_dM
    dM_rel = 1e-9
    max_dM = SCALE_OBS * dM_rel
    out["convergence_demo"] = {
        "max_dM_at_rel_improve_1e-9": float(max_dM),
        "abs_gate_fires": max_dM < ABS_TOL,
        "rel_gate_fires": max_dM / max(SCALE_OBS, 1.0) < ABS_TOL}

    # ---- (c) 标度≈1 的等价域 ----
    out["unit_scale_equivalence"] = {
        "scale": 1.0, "ulp": float(np.nextafter(1.0, math.inf) - 1.0),
        "abs_tol_1e-6_reachable": ABS_TOL > float(np.nextafter(1.0, math.inf) - 1.0),
        "claim": "绝对 1e-6 只在观测尺度 ≈1 时与相对判据等价（PHASE2_UPM §5 已声明）"}

    # ---- (d) 负例 ----
    out["negative_zero_correction"] = {"max_dM": 0.0, "abs_gate": 0.0 < ABS_TOL,
                                       "rel_gate": 0.0 < ABS_TOL,
                                       "note": "真值无效应 => max_dM=0, 两判据一致归零"}

    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2,
                  default=lambda o: o.item() if hasattr(o, "item") else str(o))
    print(json.dumps(out, ensure_ascii=False, indent=1)[:2200])
    print("saved:", os.path.abspath(OUT))


if __name__ == "__main__":
    main()
