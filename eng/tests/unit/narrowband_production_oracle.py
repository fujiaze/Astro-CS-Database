#!/usr/bin/env python3
"""Issue #106 生产路径的独立 Oracle（NumPy + SciPy）。

输入 = `narrowband_production_path_test.cpp` 在**真实 Gaia DR3 XP 光谱**上跑出的
逐星产物（/tmp/narrowband_production_path.json）:
  * 每颗匹配星的真实 XP 光谱字节（XPSD 官方解码 F=byte*flux_mul+flux_min）;
  * 生产路径算出的 F_syn（参数化窄带）与 F_syn（measured 等价矩形）;
  * 两条路径的标度与响应身份。

本脚本用 **SciPy 自己的** Akima 插值 + `scipy.integrate.quad` 自适应求积，独立
复算两条路径的 F_syn，并与生产数值对拍；再量化历史 2nm 粗格路径（#43）的误差。

用法:
    python3 eng/tests/unit/narrowband_production_oracle.py \
        --data /tmp/narrowband_production_path.json \
        --work /tmp/acsd_narrowband_prod
"""
from __future__ import annotations

import argparse
import json
import os
import sys

import numpy as np
from scipy.integrate import quad
from scipy.interpolate import Akima1DInterpolator

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
CURVE_DIR = os.path.join(REPO, "config", "response_curves")


def load_curve(path: str):
    with open(path, "r", encoding="utf-8") as fh:
        d = json.load(fh)
    return np.asarray(d["wavelength_nm"], dtype=float), np.asarray(d["value"], dtype=float)


def find_curve_by_name(directory: str, name: str) -> str:
    """按曲线对象的 `name` 字段找文件（库键 = name, **不是**文件名）。"""
    for fn in sorted(os.listdir(directory)):
        if not fn.endswith(".json"):
            continue
        p = os.path.join(directory, fn)
        try:
            with open(p, "r", encoding="utf-8") as fh:
                d = json.load(fh)
        except Exception:
            continue
        if d.get("name") == name:
            return p
    raise FileNotFoundError(f"curve {name!r} not found in {directory}")


def band_integral(xp_wl, f, qe_wl, qe_val, lo, hi, resp_wl=None, resp_val=None):
    """∫ F(λ)·T(λ)·Q(λ)·λ dλ（SciPy 参照）.

    F/Q 用 Akima 重构（SciPy 自己的实现）; T = 矩形带（resp_* 为 None 时）
    或一条采样曲线（resp_* 给出时, 同样用 Akima 重构）。
    """
    fa = Akima1DInterpolator(xp_wl, f)
    qa = Akima1DInterpolator(qe_wl, qe_val) if qe_wl is not None and len(qe_wl) >= 2 else None
    ta = Akima1DInterpolator(resp_wl, resp_val) if resp_wl is not None else None
    q_lo = qe_wl[0] if qa is not None else -np.inf
    q_hi = qe_wl[-1] if qa is not None else np.inf
    t_lo = resp_wl[0] if ta is not None else lo
    t_hi = resp_wl[-1] if ta is not None else hi
    a = max(lo, float(xp_wl[0]), q_lo, t_lo)
    b = min(hi, float(xp_wl[-1]), q_hi, t_hi)
    if not (b > a):
        return 0.0

    def integrand(lam):
        q = float(qa(lam)) if qa is not None else 1.0
        t = float(ta(lam)) if ta is not None else 1.0
        return float(fa(lam)) * t * q * lam

    pts = [p for p in xp_wl if a < p < b]
    if qa is not None:
        pts += [p for p in qe_wl if a < p < b]
    if ta is not None:
        pts += [p for p in resp_wl if a < p < b]
    pts = sorted(set(pts))
    val, _ = quad(integrand, a, b, points=pts, limit=400, epsabs=1e-20, epsrel=1e-12)
    return float(val)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default="/tmp/narrowband_production_path.json")
    ap.add_argument("--work", default="/tmp/acsd_narrowband_prod")
    a = ap.parse_args()

    with open(a.data, "r", encoding="utf-8") as fh:
        d = json.load(fh)
    qe_wl, qe_val = load_curve(find_curve_by_name(CURVE_DIR, d["qe_name"]))
    rect_dir = os.path.join(a.work, "config", "response_curves")
    rect_path = find_curve_by_name(rect_dir, d["rect_name"])
    rect_wl, rect_val = load_curve(rect_path)

    xp_start = float(d["xp_wl_start"])
    xp_step = float(d["xp_wl_step"])
    n_xp = int(d["xp_wl_count"])
    xp_wl = xp_start + xp_step * np.arange(n_xp, dtype=float)
    center = float(d["center_nm"])
    fwhm = float(d["fwhm_nm"])
    lo, hi = center - fwhm / 2.0, center + fwhm / 2.0

    print("=" * 78)
    print("Issue #106 生产路径独立 Oracle（SciPy） × 生产 C++ 对拍")
    print(f"XP 网格: {xp_wl[0]:.0f}..{xp_wl[-1]:.0f} nm / {xp_step:.0f} nm ({n_xp} 点)")
    print(f"QE: {d['qe_name']}  响应: parametric {center}/{fwhm} nm  "
          f"measured 等价矩形: {d['rect_name']}")
    print("=" * 78)

    n_fail = 0
    rel_p, rel_m, ratio_meas = [], [], []
    for row in d["stars"]:
        b = np.asarray(row["bytes"], dtype=np.uint8)
        f = b.astype(float) * float(row["flux_mul"]) + float(row["flux_min"])
        ref_p = band_integral(xp_wl, f, qe_wl, qe_val, lo, hi)
        ref_m = band_integral(xp_wl, f, qe_wl, qe_val, lo, hi,
                              resp_wl=rect_wl, resp_val=rect_val)
        gp = float(row["fsyn_parametric"])
        gm = float(row["fsyn_measured"])
        if ref_p > 0:
            rel_p.append(abs(gp - ref_p) / ref_p)
        if ref_m > 0:
            rel_m.append(abs(gm - ref_m) / ref_m)
        if ref_p > 0 and ref_m > 0:
            ratio_meas.append(gm / ref_p)

    def stat(v):
        return (float(np.max(v)), float(np.median(v))) if v else (float("nan"),) * 2

    mp, dp = stat(rel_p)
    mm, dm = stat(rel_m)
    rm, dmed = stat(ratio_meas)
    print(f"[参数化窄带] 生产 vs SciPy 相对误差: max={mp:.3e} median={dp:.3e}  "
          f"(n={len(rel_p)})")
    ok = mp < 1e-9
    n_fail += 0 if ok else 1
    print(f"[{'PASS' if ok else 'FAIL'}] 参数化路径的生产 F_syn 与独立 SciPy 复算一致 "
          f"(max rel < 1e-9)")
    print(f"[measured 矩形] 生产 vs SciPy 相对误差: max={mm:.3e} median={dm:.3e}")
    print(f"[#43 量化] measured(2nm 粗格) / 参数化(精确) 的 F_syn 比: "
          f"median={dmed:.4f} min={rm:.4f}")
    # 措辞（P2-3 审核订正）: 分母必须写清。
    #   dmed = F_meas/F_param（历史 2nm 粗格路径 / 精确值）。
    #   「相对精确值的**低估**」= 1 − dmed（实测值比精确值低多少）;
    #   1/dmed − 1 是**相对实测值**需要提高的百分比（分母不同, 数值更大）。
    print(f"           ⇒ 历史 2nm 路径的 F_syn 为精确值的 {dmed:.4f} 倍，"
          f"即相对精确值**低估 {(1.0 - dmed) * 100.0:.1f}%**"
          f"（等价表述：相对该实测值需提高 {(1.0 / dmed - 1.0) * 100.0:.1f}%）")

    # 标度自洽: 两条路径的标度比应与逐星 F_syn 比的中位数一致（同一常数因子被
    # 标度吸收 ⇒ 残差仅差一个常数）。
    kp = float(d["k_photo_parametric"])
    km = float(d["k_photo_measured"])
    print(f"[标度] 参数化 k_photo={kp:.6e}  measured k_photo={km:.6e}  "
          f"比={kp / km:.4f}（1/median(F_syn_meas/F_syn_param)={1.0 / dmed:.4f}）")

    print("-" * 78)
    print(f"FAILURES: {n_fail}")
    if n_fail:
        print("PRODUCTION ORACLE FAILED")
        return 1
    print("PRODUCTION ORACLE PASSED（真实 Gaia XP 光谱 + 真实 QE 曲线; "
          "**不是**真实 FITS/HST 验收）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
