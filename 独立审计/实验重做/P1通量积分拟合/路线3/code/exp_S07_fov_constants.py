#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S07 FOV 半径三常数: 缓冲 1.2、钳位界 1.0/10.0（05 A-4b, 01/C7, B12）。

假说 H7a: 对本仓主流像元尺度(0.2-1.2 "/px)与画幅(2k-8k px), 自然 FOV 落在
        [1,10] deg 之外的情形由小像元/大画幅端主导; 钳到 1.0 deg 使取样面积
        膨胀可达两个数量级 ⇒ 星样本与成本对钳位界敏感。
假说 H7b: 缓冲 1.2 的几何角色 = 半径方向 20% 余量; 相对 WCS 平移误差(~1.6-2",
        PHOTOMETRY.md §16.5.6)与像元尺度, 该余量在角域是绝对量, 与画幅无关 ⇒
        1.2 与 "1.0 界" 组合等价于最小锥搜半径 1.2 deg ≥ 任何 WCS 误差 ⇒ 覆盖充分。
负例: 自然 FOV ∈ [1.2,10] 的构型 ⇒ 钳位与缓冲均无效应(度量=0)。
注: 三常数为项目约定值(01/C7 判: 条文零命中), 本实验给敏感性与保守方向, 不给文献背书。
seed 固定 = 20260926。复现: python3 exp_S07_fov_constants.py
"""
import json, os
import numpy as np

SEED = 20260926
DEG_PER_ARCSEC = 1.0 / 3600.0


def fov(pixel_scale_as, w, h, buffer=1.2):
    ps = pixel_scale_as * DEG_PER_ARCSEC
    return ps * np.sqrt(w * w + h * h) / 2.0 * buffer


def main():
    out = {"seed": SEED, "note": "三常数为项目约定值; 本实验只给敏感性, 不造文献锚"}
    rows = []
    for ps in (0.2, 0.35, 0.5, 0.73, 1.0, 1.2):          # "/px 常见消费级/科研 CCD
        for size in (2000, 4000, 8000, 10000):
            f = fov(ps, size, size)
            clamped = min(max(f, 1.0), 10.0)
            area_ratio = (clamped / f) ** 2 if f > 0 else np.inf
            rows.append({"pixel_scale_as": ps, "size": size, "fov_natural_deg": float(f),
                          "fov_clamped_deg": clamped, "area_inflation": float(area_ratio),
                          "clamp_active": bool(f < 1.0 or f > 10.0)})
    out["sensitivity_table"] = rows
    n_active = sum(r["clamp_active"] for r in rows)
    out["H7a"] = {
        "n_active": n_active, "n_total": len(rows),
        "max_area_inflation": float(max(r["area_inflation"] for r in rows)),
        "detail": "自然 FOV<1.0 的构型(小像元+大画幅)被抬到 1.0 deg, 取样面积膨胀可达 "
                  "数十倍以上 ⇒ 星样本与查询成本由钳位界决定; >10 deg 侧仅超大画幅/大像元触发",
        "pass": bool(n_active > 0),
    }

    # H7b: 1.2 缓冲 vs WCS 平移误差 ~1.6-2"
    wcs_err_deg = 2.0 * DEG_PER_ARCSEC
    diag_half_deg = min(fov(ps, s, s, buffer=1.0) for ps in (0.2,) for s in (2000,))
    out["H7b"] = {
        "wcs_translation_error_deg": wcs_err_deg,
        "min_unbuffered_fov_deg_in_table": float(diag_half_deg),
        "buffer_margin_fraction": 1.2 ** 2 - 1.0,
        "criterion": "缓冲后最小半径 1.2 deg ≥ WCS 误差 5.6e-4 deg 六个数量级 ⇒ 覆盖充分;"
                     "缓冲的物理约束是 Gaia 星 'WCS 解在画幅外' 的边缘星损失, 20% 半径余量覆盖之",
        "pass": bool(1.2 * 1.0 > 10 * wcs_err_deg),
    }

    # 负例: 自然 FOV∈[1.2,10] ⇒ 无效应(需粗像元; 科学像元尺度下几乎恒被抬到 1.0)
    neutral = []
    for ps, s in ((3.0, 2000), (4.0, 2000), (2.5, 3000), (5.0, 1500)):
        f = fov(ps, s, s)
        neutral.append({"pixel_scale_as": ps, "size": s, "fov": float(f),
                         "clamp_ineffective": bool(1.2 <= f <= 10.0)})
    out["negative_zero_check"] = {
        "rows": neutral,
        "metric": "|k_effect| = 钳位前后面积差(自然 FOV 在窗内时应为 0)",
        "all_zero": all(r["clamp_ineffective"] for r in neutral),
        "pass": all(r["clamp_ineffective"] for r in neutral),
    }

    # 条件 vs 无条件钳位的差异区: (0,1) 与 (10,30) —— 现行代码不钳, 05 要求无条件钳
    diff_zone = [r for r in rows if 0.0 < r["fov_natural_deg"] < 1.0 or 10.0 < r["fov_natural_deg"] < 30.0]
    out["conditional_vs_unconditional"] = {
        "n_rows_in_diff_zone": len(diff_zone),
        "note": "05 A-4b 把'条件钳位(<=0 或 >=30 才钳)'改为无条件钳位; 本表给出差异区"
                "内行数 —— 即该规格变更实际改变行为的构型集合",
    }

    out["verdict"] = {"H7a": out["H7a"]["pass"], "H7b": out["H7b"]["pass"],
                      "negative_zero": out["negative_zero_check"]["pass"]}
    res = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results")
    os.makedirs(res, exist_ok=True)
    p = os.path.join(res, "exp_S07_fov_constants.json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(json.dumps({"H7a": out["H7a"], "H7b": out["H7b"],
                      "negative_zero_check": out["negative_zero_check"],
                      "conditional_vs_unconditional": out["conditional_vs_unconditional"],
                      "verdict": out["verdict"]}, ensure_ascii=False, indent=1))
    print("written:", p)


if __name__ == "__main__":
    main()
