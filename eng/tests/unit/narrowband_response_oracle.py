#!/usr/bin/env python3
"""Issue #106 独立数学 Oracle（NumPy + SciPy），与生产 C++ 实现对拍。

**独立性**: 本脚本**不复用**任何 ACSD 生产函数：
  * 插值算子 = `scipy.interpolate.Akima1DInterpolator`（SciPy 自己的实现，
    与项目内 `spectrum_integrator.cpp` 的 Akima 是两套代码）；
  * 求积 = `scipy.integrate.quad`（自适应求积，与生产的分段 4 点
    Gauss–Legendre 是两套算法）；
  * XP 光谱的重构口径按 XPSD 官方解码 `F(λ)=byte*flux_mul+flux_min` 复原。

对拍对象 = 生产 C++ 的两条 F_syn 路径（由 `narrowband_response_test.cpp`
的 `--cases` 驱动模式计算）：
  1. **参数化窄带**（新路径）: 边界精确的分段求积。
  2. **measured 曲线**（历史路径）: Akima 重采样到 XP 2nm 网格 + Simpson。
     本脚本同时给出该路径的**精确参照**，用来量化 #43 的粗格采样误差。

用例定义只存在于本脚本生成的用例文件里（两侧不重复定义）。

用法:
    python3 eng/tests/unit/narrowband_response_oracle.py
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile

import numpy as np
from scipy.integrate import quad
from scipy.interpolate import Akima1DInterpolator

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
CURVE_DIR = os.path.join(REPO, "config", "response_curves")
RUNNER = os.path.join(REPO, "eng", "tests", "unit", "run_narrowband_response_test.py")

# XP 网格（Gaia DR3 BP/RP 的实际采样: 336..1020 nm, 2 nm 步长）
XP_START, XP_STEP, N_XP = 336.0, 2.0, 343


def load_curve(name: str):
    with open(os.path.join(CURVE_DIR, name), "r", encoding="utf-8") as fh:
        d = json.load(fh)
    return np.asarray(d["wavelength_nm"], dtype=float), np.asarray(d["value"], dtype=float)


def synthetic_xp_spectrum() -> np.ndarray:
    """连续谱 toy：两条高斯谱 + 幂律基底, 量化到 uint8（XP 的量化方式）。

    量化步长 1e-8 W·m⁻²·nm⁻¹, flux_min=0 ⇒ byte*1e-8。
    """
    wl = XP_START + XP_STEP * np.arange(N_XP, dtype=float)
    f = (1.0e-7 * np.exp(-0.5 * ((wl - 640.0) / 45.0) ** 2)
         + 2.0e-7 * np.exp(-0.5 * ((wl - 680.0) / 70.0) ** 2)
         + 3.0e-9 * (wl / 1000.0) ** -2.0)
    return np.clip(np.rint(f / 1.0e-8), 0, 255).astype(np.uint8)


def segmented_reference(xp_wl, f_bytes, qe_wl, qe_val, center, fwhm,
                        flux_min, flux_mul) -> float:
    """SciPy 参照: 自适应求积（被积函数 = Akima(F)·Akima(Q)·λ, 乘 top-hat）。"""
    f = f_bytes.astype(float) * flux_mul + flux_min
    fa = Akima1DInterpolator(xp_wl, f)
    if len(qe_wl) >= 2:
        qa = Akima1DInterpolator(qe_wl, qe_val)
        q_lo, q_hi = qe_wl[0], qe_wl[-1]
    else:
        qa = None
        q_lo, q_hi = -np.inf, np.inf
    lo = max(center - fwhm / 2.0, xp_wl[0], q_lo)
    hi = min(center + fwhm / 2.0, xp_wl[-1], q_hi)
    if not (hi > lo):
        return 0.0

    def integrand(lam):
        q = qa(lam) if qa is not None else 1.0
        return float(fa(lam)) * float(q) * lam

    # 断点: XP 节点 ∪ QE 节点（被积函数的节点区间边界）
    pts = [p for p in xp_wl if lo < p < hi]
    if qa is not None:
        pts += [p for p in qe_wl if lo < p < hi]
    pts = sorted(set(pts))
    val, _ = quad(integrand, lo, hi, points=pts, limit=400, epsabs=1e-18, epsrel=1e-12)
    return float(val)


def legacy_reference(xp_wl, f_bytes, filt_wl, filt_val, qe_wl, qe_val,
                     flux_min, flux_mul) -> float:
    """历史 measured 路径的 Python 复刻（Akima 重采样到 XP 网格 + Simpson）。

    用途: 核对 C++ 历史路径的数值（应逐位接近）, 并与 `measured_exact_reference`
    的**精确**积分对比 ⇒ 量化 #43 的粗格采样误差。
    """
    f = f_bytes.astype(float) * flux_mul + flux_min
    fa = Akima1DInterpolator(xp_wl, f)
    t = np.interp(xp_wl, filt_wl, filt_val, left=0.0, right=0.0)  # 仅为量级参考
    if len(qe_wl) >= 2:
        q = np.interp(xp_wl, qe_wl, qe_val, left=0.0, right=0.0)
    else:
        q = np.ones_like(xp_wl)
    y = f * t * q * xp_wl
    # 复合 Simpson（与生产的 simpson_integrate 同式: 奇数区间末 3 段用 3/8）
    n_int = len(xp_wl) - 1
    h = (xp_wl[-1] - xp_wl[0]) / n_int
    if n_int % 2 == 0:
        s = y[0] + y[-1] + np.sum((4.0 if i % 2 else 2.0) * y[i]
                                   for i in range(1, len(y) - 1))
        return float(s * h / 3.0)
    n13 = n_int - 3
    s13 = 0.0
    if n13 > 0:
        s13 = (y[0] + y[n13]
               + sum((4.0 if i % 2 else 2.0) * y[i] for i in range(1, n13))) * h / 3.0
    s38 = (y[n13] + 3.0 * y[n13 + 1] + 3.0 * y[n13 + 2] + y[-1]) * 3.0 * h / 8.0
    return float(s13 + s38)


def measured_exact_reference(xp_wl, f_bytes, filt_wl, filt_val, qe_wl, qe_val,
                             flux_min, flux_mul) -> float:
    """实测/任意曲线的**精确**积分（Akima 重构 + 自适应求积）⇒ 参照真值。"""
    f = f_bytes.astype(float) * flux_mul + flux_min
    fa = Akima1DInterpolator(xp_wl, f)
    ta = Akima1DInterpolator(filt_wl, filt_val)
    if len(qe_wl) >= 2:
        qa = Akima1DInterpolator(qe_wl, qe_val)
        q_lo, q_hi = qe_wl[0], qe_wl[-1]
    else:
        qa = None
        q_lo, q_hi = -np.inf, np.inf
    lo = max(float(filt_wl[0]), float(xp_wl[0]), q_lo)
    hi = min(float(filt_wl[-1]), float(xp_wl[-1]), q_hi)
    if not (hi > lo):
        return 0.0

    def integrand(lam):
        q = qa(lam) if qa is not None else 1.0
        return float(fa(lam)) * float(ta(lam)) * float(q) * lam

    pts = [p for p in xp_wl if lo < p < hi]
    if qa is not None:
        pts += [p for p in qe_wl if lo < p < hi]
    pts = sorted(set(pts))
    val, _ = quad(integrand, lo, hi, points=pts, limit=400, epsabs=1e-18, epsrel=1e-12)
    return float(val)


def write_cases(path: str, cases) -> None:
    with open(path, "w", encoding="utf-8") as fh:
        for c in cases:
            parts = ["CASE", c["id"], repr(c["xp_start"]), repr(c["xp_step"]),
                     str(c["n_xp"]), repr(c["flux_min"]), repr(c["flux_mul"]),
                     repr(c["center"]), repr(c["fwhm"]), str(len(c["qe_wl"]))]
            parts += [repr(v) for v in c["qe_wl"]]
            parts += [repr(v) for v in c["qe_val"]]
            parts += [str(len(c["filt_wl"]))]
            parts += [repr(v) for v in c["filt_wl"]]
            parts += [repr(v) for v in c["filt_val"]]
            parts += [str(int(b)) for b in c["bytes"]]
            fh.write(" ".join(parts) + "\n")


def main() -> int:
    qe_wl, qe_val = load_curve("KAF_16803.json")      # 真实相机 QE（343 点, 2nm）
    filt_wl, filt_val = load_curve("Baader_7nm_H_alpha.json")  # 真实窄带曲线（21 点, 2nm）
    ideal_wl, ideal_val = load_curve("Ideal_QE_curve.json")
    bytes_ = synthetic_xp_spectrum()
    flux_min, flux_mul = 0.0, 1.0e-8

    cases = []
    # (a) 参数化 Hα: 3/5/7 nm, 中心 656.28（**与 XP 2nm 网格错相**）
    for width in (3.0, 5.0, 7.0):
        cases.append(dict(id=f"parametric_ha_{int(width)}nm", xp_start=XP_START,
                          xp_step=XP_STEP, n_xp=N_XP, flux_min=flux_min,
                          flux_mul=flux_mul, center=656.28, fwhm=width,
                          qe_wl=qe_wl.tolist(), qe_val=qe_val.tolist(),
                          filt_wl=[], filt_val=[], bytes=bytes_))
    # (b) 参数化 + 理想 QE（Q≡1）
    cases.append(dict(id="parametric_ha_3nm_ideal_qe", xp_start=XP_START,
                      xp_step=XP_STEP, n_xp=N_XP, flux_min=flux_min,
                      flux_mul=flux_mul, center=656.28, fwhm=3.0,
                      qe_wl=ideal_wl.tolist(), qe_val=ideal_val.tolist(),
                      filt_wl=[], filt_val=[], bytes=bytes_))
    # (c) 等价矩形（与 (a) 同一 3nm 顶帽, 但作为 sampled 曲线）⇒ 与参数化路径
    #     应给出**同一**响应（同一 T 函数）; 旧路径的误差由此量化。
    for width in (3.0, 5.0, 7.0):
        lo, hi = 656.28 - width / 2.0, 656.28 + width / 2.0
        grid = np.arange(640.0, 672.0 + 1e-9, 1.0)
        vals = np.where((grid >= lo) & (grid <= hi), 1.0, 0.0)
        cases.append(dict(id=f"measured_rect_{int(width)}nm", xp_start=XP_START,
                          xp_step=XP_STEP, n_xp=N_XP, flux_min=flux_min,
                          flux_mul=flux_mul, center=656.28, fwhm=width,
                          qe_wl=qe_wl.tolist(), qe_val=qe_val.tolist(),
                          filt_wl=grid.tolist(), filt_val=vals.tolist(),
                          bytes=bytes_))
    # (d) 真实实测窄带曲线（Baader 7nm Hα）经历史 measured 路径
    cases.append(dict(id="measured_baader_7nm_ha", xp_start=XP_START,
                      xp_step=XP_STEP, n_xp=N_XP, flux_min=flux_min,
                      flux_mul=flux_mul, center=656.28, fwhm=3.0,
                      qe_wl=qe_wl.tolist(), qe_val=qe_val.tolist(),
                      filt_wl=filt_wl.tolist(), filt_val=filt_val.tolist(),
                      bytes=bytes_))

    with tempfile.TemporaryDirectory() as tmp:
        case_file = os.path.join(tmp, "cases.txt")
        write_cases(case_file, cases)
        cp = subprocess.run([sys.executable, RUNNER, "--cases", case_file],
                            cwd=REPO, capture_output=True, text=True)
        if cp.returncode != 0:
            print(cp.stdout[-3000:])
            print(cp.stderr[-3000:])
            print("C++ DRIVER FAILED")
            return 1
        cpp = {}
        for line in cp.stdout.splitlines():
            if not line.startswith("CASE "):
                continue
            _, cid, seg, legacy, cov, nseg = line.split()
            cpp[cid] = dict(seg=float(seg), legacy=float(legacy), cov=cov,
                            nseg=int(nseg))

    by_id = {c["id"]: c for c in cases}
    n_fail = 0
    print("=" * 78)
    print("Issue #106 独立 Oracle（NumPy+SciPy） × 生产 C++ 对拍")
    print("=" * 78)

    # ---- 1. 参数化路径: C++ 分段求积 vs SciPy 自适应求积 -------------------
    for width in (3.0, 5.0, 7.0):
        cid = f"parametric_ha_{int(width)}nm"
        c = by_id[cid]
        ref = segmented_reference(
            np.asarray(c["xp_wl"] if "xp_wl" in c else
                       [c["xp_start"] + k * c["xp_step"] for k in range(c["n_xp"])]),
            c["bytes"], np.asarray(c["qe_wl"]), np.asarray(c["qe_val"]),
            c["center"], c["fwhm"], c["flux_min"], c["flux_mul"])
        got = cpp[cid]["seg"]
        rel = abs(got - ref) / abs(ref)
        ok = rel < 1e-9
        n_fail += 0 if ok else 1
        print(f"[{'PASS' if ok else 'FAIL'}] {cid}: C++={got:.10e} scipy={ref:.10e} "
              f"rel={rel:.3e} coverage={cpp[cid]['cov']} nseg={cpp[cid]['nseg']}")
    # 理想 QE
    cid = "parametric_ha_3nm_ideal_qe"
    c = by_id[cid]
    xp = np.asarray([c["xp_start"] + k * c["xp_step"] for k in range(c["n_xp"])])
    ref = segmented_reference(xp, c["bytes"], np.asarray(c["qe_wl"]),
                              np.asarray(c["qe_val"]), c["center"], c["fwhm"],
                              c["flux_min"], c["flux_mul"])
    got = cpp[cid]["seg"]
    rel = abs(got - ref) / abs(ref)
    ok = rel < 1e-9
    n_fail += 0 if ok else 1
    print(f"[{'PASS' if ok else 'FAIL'}] {cid}: C++={got:.10e} scipy={ref:.10e} rel={rel:.3e}")

    # ---- 2. #43 量化: 历史 measured 路径 vs 精确积分 ------------------------
    print("-" * 78)
    for width in (3.0, 5.0, 7.0):
        cid = f"measured_rect_{int(width)}nm"
        c = by_id[cid]
        exact = measured_exact_reference(
            xp, c["bytes"], np.asarray(c["filt_wl"]), np.asarray(c["filt_val"]),
            np.asarray(c["qe_wl"]), np.asarray(c["qe_val"]), c["flux_min"],
            c["flux_mul"])
        legacy = cpp[cid]["legacy"]
        rel = abs(legacy - exact) / abs(exact)
        print(f"[INFO] {cid}: 历史 2nm 路径={legacy:.10e} 精确={exact:.10e} "
              f"相对误差={rel:.3e}")
    cid = "measured_baader_7nm_ha"
    c = by_id[cid]
    exact = measured_exact_reference(
        xp, c["bytes"], np.asarray(c["filt_wl"]), np.asarray(c["filt_val"]),
        np.asarray(c["qe_wl"]), np.asarray(c["qe_val"]), c["flux_min"],
        c["flux_mul"])
    legacy = cpp[cid]["legacy"]
    rel = abs(legacy - exact) / abs(exact)
    print(f"[INFO] {cid}（真实曲线）: 历史 2nm 路径={legacy:.10e} 精确={exact:.10e} "
          f"相对误差={rel:.3e}")
    # 3nm 矩形的粗格误差必须 > 5%（#43 的量化判据）
    cid = "measured_rect_3nm"
    c = by_id[cid]
    exact3 = measured_exact_reference(
        xp, c["bytes"], np.asarray(c["filt_wl"]), np.asarray(c["filt_val"]),
        np.asarray(c["qe_wl"]), np.asarray(c["qe_val"]), c["flux_min"],
        c["flux_mul"])
    rel3 = abs(cpp[cid]["legacy"] - exact3) / abs(exact3)
    ok = rel3 > 0.05
    n_fail += 0 if ok else 1
    print(f"[{'PASS' if ok else 'FAIL'}] #43 判据: 3nm 矩形的 2nm 粗格路径相对误差 "
          f"{rel3:.3e} > 5%")

    print("-" * 78)
    print(f"FAILURES: {n_fail}")
    if n_fail:
        print("ORACLE FAILED")
        return 1
    print("ORACLE PASSED（数学对拍; **不是**真实 FITS/HST 验收）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
