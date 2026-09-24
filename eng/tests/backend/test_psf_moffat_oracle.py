#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SCI-PSF-001 §11「Python 参考」独立 Oracle — scipy.optimize.curve_fit 复算 Moffat4。

合同依据（docs/science/PSF.md）:
  §11 第 98-99 行：「Python 参考：scipy / NumPy 对同参数 Moffat4 图像块做 curve_fit 复算，
                  位置 ≤0.05px、FWHM ≤1%（合成无噪声谱）」——本文件即该承诺的落地工件。
  §5  连续定义：I(r) = B + A/(1+Q)^4, Q = p1·dx²+2p2·dxdy+p3·dy²（生产二次型写法）。
  §7  不变量：各向同性 FWHM/σ = 2√2·√(2^{1/4}−1) ≈ 1.230310（与 A,B 无关）。
  §13 测试锚：TST-PSF-001 解析一致性 + TST-PSF-INV-* 不变量门。

独立性（关键）:
  * 本 Oracle **不 import 任何本仓 C/C++ 产物**、不链接、不调用 dpsf_* 或任何 acsd 库；
  * 真值生成与拟合模型都不用生产的 p1/p2/p3 二次型，而用**旋转主轴投影**形式
      u =  cosθ·dx + sinθ·dy   (σ = sx)
      v = −sinθ·dx + cosθ·dy   (σ = sy)
      Q = 0.5·(u²/sx² + v²/sy²)
    两侧仅在 PSF.md 冻结的数学定义上等价（与生产参数化不同源）；
  * 求解器不同源：生产用自研分解 LM（dpsf_psf.cpp:98-182），此处用 scipy 的
    Levenberg–Marquardt/TRF（curve_fit）+ 数值 Jacobian。

两种运行形态（缺一不可）:
  A) 独立脚本（PSF.md §11 复算口径 + 极性证据）:
       python3 eng/tests/backend/test_psf_moffat_oracle.py                      -> rc=0, 打印 PSF MOFFAT ORACLE PASS
       python3 eng/tests/backend/test_psf_moffat_oracle.py --negative-injection -> rc=0, 打印 NEGATIVE INJECTION DETECTED
       PSF_ORACLE_INJECT=1 python3 eng/tests/backend/test_psf_moffat_oracle.py  -> 同上
  B) unittest 形态（必须：CHK-UNIT::UT-BACKEND 用
     "python3 -m unittest discover -s eng/tests/backend -t eng/tests/backend" 采集；
     非 TestCase 的纯 main() 脚本不被采集 ⇒ 门形同虚设。
     见 工程控制/PROJECT-GOVERNANCE-01/tasks/SCI-FIX-PSF.md CI-003-SCI-FIX-PSF-4）:
       python3 -m unittest discover -s eng/tests/backend -t eng/tests/backend -p "test_psf_moffat_oracle.py" -v
     采集到 3 个测试：无噪声域 / 噪声面 p95 / 负例检出。

检查内容:
  1) 无噪声解析谱：4 组 (cx,cy,sx,sy,θ)（含各向同性/强各向异性、整像素/子像素中心）
     → 断言 |Δ位置| ≤ 0.05 px、FWHM 相对误差 ≤ 1%；
  2) 噪声面：SNR=100（σ_noise = A/SNR），200 次蒙特卡洛 → p95 |Δ位置| ≤ 0.05 px
     （避免单点恒真：单次无噪声拟合几乎精确，噪声面才检验统计意义下的域）；
  3) 负例注入：合成谱的 β 改为 3.9 而拟合模型仍按 β=4 → 必须被 FWHM>1% 或位置超差检出。

依赖: numpy + scipy（仓内既有依赖，见 eng/tests/unit/v6_p2_upm/oracle/upm_ma_oracle.py 等 3 处）；
      无网络、无编译、无 ctest、不写任何文件、不依赖 build/。
退出码（脚本形态）: 0 = 命中期望；1 = 断言失败/负例未被检出；3 = 依赖缺失。
"""
from __future__ import annotations

import argparse
import math
import os
import sys
import time
import unittest

import numpy as np

try:
    from scipy.optimize import curve_fit
except Exception as exc:  # noqa: BLE001
    print(f"DEPENDENCY_MISSING: scipy 不可用: {exc!r}", file=sys.stderr)
    raise SystemExit(3)

# ---------------------------------------------------------------------------
# 冻结常数（PSF.md §5/§7）
# ---------------------------------------------------------------------------
BETA_FIT = 4.0                                     # 生产/文档冻结的 Moffat β
BETA_INJECT_BAD = 3.9                              # 负例注入用 β（偏离冻结值）
FWHM_FACTOR = 2.0 * math.sqrt(2.0) * math.sqrt(2.0 ** 0.25 - 1.0)   # ≈ 1.2303077
POS_TOL_PX = 0.05                                  # PSF.md:99 位置域
FWHM_REL_TOL = 0.01                                # PSF.md:99 FWHM 域
NOISE_SNR = 100.0                                  # 噪声面 SNR
NOISE_TRIALS = 200                                 # 蒙特卡洛次数
NOISE_P95_TOL_PX = 0.05                            # p95 |Δ位置| 域
TOTAL_TIME_BUDGET_S = 30.0                         # 运行时长硬上限
SEED = 20260101
GRID = 17                                          # 17×17 图像块

# (cx, cy, sx, sy, theta) —— 固定 A/B，至少 3 组不同参数（此处分 4 组）
CASES = [
    ("iso_int_center",     8.00,  8.00, 2.00, 2.00,  0.00),
    ("iso_subpix_center", 10.35,  6.72, 1.60, 1.60,  0.00),
    ("aniso_mid_ratio",    7.90,  9.20, 2.40, 1.50,  0.50),
    ("aniso_large_ratio",  8.60,  7.40, 1.35, 2.90, -1.10),
]
A_TRUE = 1000.0     # ADU
B_TRUE = 120.0      # ADU


def moffat4(coords, B, A, cx, cy, sx, sy, theta, beta=BETA_FIT):
    """旋转主轴投影形式的 Moffat4：B + A/(1+Q)^beta，Q = 0.5·(u²/sx² + v²/sy²)。"""
    x, y = coords
    dx = x - cx
    dy = y - cy
    c = math.cos(theta)
    s = math.sin(theta)
    u = c * dx + s * dy
    v = -s * dx + c * dy
    q = 0.5 * ((u / sx) ** 2 + (v / sy) ** 2)
    return B + A / (1.0 + q) ** beta


def synth(cx, cy, sx, sy, theta, beta=BETA_FIT, noise_sigma=0.0, rng=None):
    """生成合成图像块（独立于生产的解析采样；噪声为加性 Gaussian）。"""
    g = np.arange(GRID, dtype=np.float64)
    xx, yy = np.meshgrid(g, g)
    img = moffat4((xx, yy), B_TRUE, A_TRUE, cx, cy, sx, sy, theta, beta)
    if noise_sigma > 0.0:
        img = img + rng.normal(0.0, noise_sigma, img.shape)
    return xx.ravel(), yy.ravel(), img.ravel()


def fit_block(x, y, img, p0):
    """scipy curve_fit 独立复算 7 参数（与生产 LM 不同源）。"""
    lo = [-np.inf, 0.0, -np.inf, -np.inf, 1e-3, 1e-3, -np.inf]
    hi = [np.inf, np.inf, np.inf, np.inf, np.inf, np.inf, np.inf]
    popt, _pcov = curve_fit(moffat4, (x, y), img, p0=p0, bounds=(lo, hi), maxfev=20000)
    return popt


def p0_for(cx, cy, sx, sy, theta, img):
    """不用真值的初始猜测（峰值像素 + 扰动），避免「p0=真值」式恒真。"""
    b0 = float(np.min(img))
    a0 = max(float(np.max(img)) - b0, 1.0)
    return [b0, a0, cx + 0.45, cy - 0.35, sx * 1.18, sy * 0.85, theta + 0.25]


def fwhm_pair(sx, sy):
    return (FWHM_FACTOR * sx, FWHM_FACTOR * sy)


def rel_fwhm_err(sx_t, sy_t, sx_f, sy_f):
    """主轴退化安全：比较排序后的 (FWHM_a, FWHM_b) 对（θ 与 θ+π/2 标签互换等价）。"""
    t = sorted(fwhm_pair(sx_t, sy_t))
    f = sorted(fwhm_pair(sx_f, sy_f))
    return max(abs(f[i] - t[i]) / t[i] for i in range(2))


def run_cases(beta_inject=BETA_FIT, verbose=True):
    """返回 [(name, dpos_px, fwhm_rel_err, ok, elapsed_s)]。"""
    out = []
    for name, cx, cy, sx, sy, theta in CASES:
        t0 = time.perf_counter()
        x, y, img = synth(cx, cy, sx, sy, theta, beta=beta_inject)
        popt = fit_block(x, y, img, p0_for(cx, cy, sx, sy, theta, img))
        _b, _a, cx_f, cy_f, sx_f, sy_f, _th_f = popt
        dpos = max(abs(cx_f - cx), abs(cy_f - cy))
        drel = rel_fwhm_err(sx, sy, sx_f, sy_f)
        ok = (dpos <= POS_TOL_PX) and (drel <= FWHM_REL_TOL)
        dt = time.perf_counter() - t0
        out.append((name, float(dpos), float(drel), ok, dt))
        if verbose:
            print(f"CASE {name:<18} true(cx={cx:.2f},cy={cy:.2f},sx={sx:.2f},sy={sy:.2f},th={theta:+.2f}) "
                  f"fit(cx={cx_f:.6f},cy={cy_f:.6f},sx={sx_f:.6f},sy={sy_f:.6f}) "
                  f"dpos={dpos:.3e}px fwhm_rel={drel:.3e} "
                  f"({'OK' if ok else 'VIOLATION'}) {dt * 1e3:.1f}ms")
    return out


def run_noise_mc(verbose=True):
    """SNR=100 噪声面：200 次蒙特卡洛，返回 (p95_dpos, 失败拟合数, 耗时)。"""
    rng = np.random.default_rng(SEED)
    sigma = A_TRUE / NOISE_SNR
    errs = []
    nfail = 0
    t0 = time.perf_counter()
    for _ in range(NOISE_TRIALS):
        cx = 8.0 + float(rng.uniform(-0.5, 0.5))
        cy = 8.0 + float(rng.uniform(-0.5, 0.5))
        sx = sy = 2.0
        x, y, img = synth(cx, cy, sx, sy, 0.0, noise_sigma=sigma, rng=rng)
        try:
            popt = fit_block(x, y, img, p0_for(cx, cy, sx, sy, 0.0, img))
        except Exception:  # noqa: BLE001  拟合不收敛计为超差（不掩盖）
            nfail += 1
            errs.append(float("inf"))
            continue
        errs.append(max(abs(popt[2] - cx), abs(popt[3] - cy)))
    dt = time.perf_counter() - t0
    finite = np.array([e for e in errs if np.isfinite(e)])
    p95 = float(np.percentile(finite, 95)) if finite.size else float("inf")
    if verbose:
        print(f"NOISE-MC SNR={NOISE_SNR:.0f} sigma={sigma:.3f}ADU trials={NOISE_TRIALS} "
              f"grid={GRID}x{GRID} nonconv={nfail} "
              f"p50={np.percentile(finite, 50):.3e}px p95={p95:.3e}px max={finite.max():.3e}px "
              f"tol_p95={NOISE_P95_TOL_PX}px ({dt:.2f}s)")
    return p95, nfail, dt


# ---------------------------------------------------------------------------
# unittest 形态（CHK-UNIT::UT-BACKEND discover 采集面）
# ---------------------------------------------------------------------------
class PsfMoffatOracleTest(unittest.TestCase):
    """PSF.md §11 Python 参考承诺的机器可采集形态（3 个测试）。"""

    def test_01_noisefree_position_and_fwhm_domain(self):
        """无噪声：4 组参数，|Δ位置| ≤ 0.05 px 且 FWHM 相对误差 ≤ 1%。"""
        cases = run_cases(BETA_FIT, verbose=False)
        self.assertEqual(len(cases), len(CASES))
        for name, dpos, drel, _ok, _dt in cases:
            self.assertLessEqual(dpos, POS_TOL_PX, f"{name}: |dpos|={dpos:.3e}px 超 PSF.md:99 域")
            self.assertLessEqual(drel, FWHM_REL_TOL, f"{name}: FWHM 相对误差={drel:.3e} 超 PSF.md:99 域")

    def test_02_noise_mc_p95_position(self):
        """噪声面 SNR=100 / 200 次蒙特卡洛：p95 |Δ位置| ≤ 0.05 px（拒绝单点恒真）。"""
        p95, nfail, _dt = run_noise_mc(verbose=False)
        self.assertEqual(nfail, 0, f"{nfail}/{NOISE_TRIALS} 次拟合不收敛")
        self.assertLessEqual(p95, NOISE_P95_TOL_PX,
                             f"SNR={NOISE_SNR:.0f} p95 |dpos|={p95:.3e}px 超域")

    def test_03_negative_injection_detected(self):
        """负例：合成谱 β=3.9 而模型按 β=4 拟合 → 必须被 FWHM/位置域检出。"""
        cases = run_cases(BETA_INJECT_BAD, verbose=False)
        viol = [c for c in cases if not c[3]]
        self.assertTrue(viol, "β=3.9 注入未被任何用例检出（门无鉴别力）")
        self.assertGreater(max(c[2] for c in cases), FWHM_REL_TOL,
                           "β=3.9 注入的 FWHM 相对误差未超 1%")


# ---------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser(description="PSF Moffat4 scipy 独立 Oracle (SCI-PSF-001 §11)")
    ap.add_argument("--negative-injection", action="store_true",
                    help="把合成谱的 beta 改为 3.9（模型仍按 4 拟合），必须被检出")
    args = ap.parse_args()
    inject = bool(args.negative_injection) or os.environ.get("PSF_ORACLE_INJECT") == "1"
    beta_inject = BETA_INJECT_BAD if inject else BETA_FIT

    print(f"PSF ORACLE mode={'NEGATIVE-INJECTION' if inject else 'NORMAL'} "
          f"beta_inject={beta_inject} beta_fit={BETA_FIT} "
          f"FWHM_FACTOR={FWHM_FACTOR:.6f} grid={GRID}x{GRID} A={A_TRUE} B={B_TRUE}")
    t_start = time.perf_counter()

    cases = run_cases(beta_inject)
    viol = [c for c in cases if not c[3]]
    dpos_max = max(c[1] for c in cases)
    drel_max = max(c[2] for c in cases)

    if inject:
        elapsed = time.perf_counter() - t_start
        if viol:
            print(f"NEGATIVE INJECTION DETECTED cases_violating={len(viol)}/{len(cases)} "
                  f"max_dpos={dpos_max:.3e}px max_fwhm_rel={drel_max:.3e} "
                  f"(tol pos<={POS_TOL_PX}px fwhm<={FWHM_REL_TOL:.0%}) elapsed={elapsed:.2f}s")
            return 0
        print(f"NEGATIVE INJECTION NOT DETECTED: beta={beta_inject} 偏离未被检出 "
              f"max_dpos={dpos_max:.3e}px max_fwhm_rel={drel_max:.3e} elapsed={elapsed:.2f}s")
        return 1

    p95, nfail, _mc_s = run_noise_mc()
    elapsed = time.perf_counter() - t_start
    checks = [
        ("noise-free dpos <= 0.05px", dpos_max <= POS_TOL_PX, f"{dpos_max:.3e}px"),
        ("noise-free fwhm_rel <= 1%", drel_max <= FWHM_REL_TOL, f"{drel_max:.3e}"),
        ("SNR100 p95 dpos <= 0.05px", p95 <= NOISE_P95_TOL_PX, f"{p95:.3e}px q={nfail}nonconv"),
        ("runtime < 30s", elapsed < TOTAL_TIME_BUDGET_S, f"{elapsed:.2f}s"),
    ]
    bad = [n for n, ok, _v in checks if not ok]
    for n, ok, v in checks:
        print(f"  CHECK {'PASS' if ok else 'FAIL'} {n} -> {v}")
    if bad:
        print(f"PSF MOFFAT ORACLE FAIL: {', '.join(bad)} (elapsed {elapsed:.2f}s)")
        return 1
    print(f"PSF MOFFAT ORACLE PASS ({len(cases)} cases + SNR{NOISE_SNR:.0f} MC {NOISE_TRIALS} trials, "
          f"elapsed {elapsed:.2f}s, dpos_max={dpos_max:.2e}px, fwhm_rel_max={drel_max:.2e}, p95={p95:.2e}px)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print(f"TOOLING_FAILURE: 未捕获异常 {exc!r}", file=sys.stderr)
        raise SystemExit(3)
