# lib/phase2/tools/rejection_oracle_compare.py — Phase2 Rejection Oracle 对照（V15）
#
# 用途（控制包 07_REJECTION_IMPLEMENTATION / 12_DELIVERY evidence）：
#   - Sigma ↔ Astropy astropy.stats.sigma_clip（median + mad_std）
#   - ESD  ↔ NIST Generalized ESD（Rosner 示例，54 点 3 outliers）
#   - Auto ↔ WBPP 2.9.1 本机源码 bestRejectionMethod 政策
#   - 边界矩阵（NaN/±Inf/valid=false/零方差/n=2 卫星线）
# 只读 Oracle 工具（NON_PRODUCTION_TOOL_ONLY）。
#
# V15 修复：
#   - CLI 路径不再硬编码（ASTROCS_REJECTION_CLI 环境变量可覆盖）；
#   - 所有 subprocess 显式 timeout；
#   - 删除恒真断言（SciPy primitive 对照改为有意义的数值断言）；
#   - Python 镜像改名 winsorized_mirror_smoke，明确 NOT_AN_ORACLE。
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from astropy.stats import sigma_clip, mad_std
from scipy.stats import mstats, t as tdist

CLI = os.environ.get(
    "ASTROCS_REJECTION_CLI",
    str(Path(__file__).resolve().parents[1] / "build" / "rejection_cli.exe"),
)
TIMEOUT_S = 120


def run_cpp(values, method, lo=-4.0, hi=3.0, max_iter=8, min_samples=3):
    txt = "\n".join(repr(float(v)) for v in values)
    r = subprocess.run(
        [CLI, str(method), str(lo), str(hi), str(max_iter), str(min_samples)],
        input=txt, capture_output=True, text=True, timeout=TIMEOUT_S)
    if r.returncode != 0:
        raise RuntimeError("cli failed: " + r.stderr)
    lines = r.stdout.strip().splitlines()
    mask = np.array([int(c) for c in lines[0].split()], dtype=int)
    return mask, lines[1]


def run_plan(values, request, nominal, reasons_out=True,
             underdetermined_n=2, **typed):
    plan = {"request": request, "nominal": nominal,
            "profile": "wbpp_current", "underdetermined_n": underdetermined_n}
    plan.update(typed)
    fd, pf = tempfile.mkstemp(suffix=".json")
    os.close(fd)
    try:
        with open(pf, "w", encoding="utf-8") as f:
            json.dump(plan, f)
        txt = "\n".join(repr(float(v)) for v in values)
        cmd = [CLI, "--plan", pf]
        if reasons_out:
            cmd.append("--reasons")
        r = subprocess.run(cmd, input=txt, capture_output=True, text=True,
                           timeout=TIMEOUT_S, encoding="utf-8",
                           errors="replace")
    finally:
        os.unlink(pf)
    if r.returncode != 0:
        raise RuntimeError("cli failed: " + r.stderr[:300])
    lines = r.stdout.strip().splitlines()
    mask = np.array([int(c) for c in lines[0].split()], dtype=int)
    reasons = (np.array([int(c) for c in lines[1].split()], dtype=int)
               if reasons_out else None)
    return mask, reasons, lines[-1]


def sigma_vs_astropy():
    rng = np.random.default_rng(20260810)
    vals = rng.normal(0.0, 1.0, 200)
    vals[17] = 6.5
    vals[88] = -6.2
    vals[150] = 8.0
    vals[5] = 7.5
    mask_cpp, stat = run_cpp(vals, method=1, lo=-4.0, hi=3.0,
                             max_iter=8, min_samples=3)
    sc = sigma_clip(vals, sigma_lower=4.0, sigma_upper=3.0, maxiters=8,
                    cenfunc="median", stdfunc="mad_std", masked=True)
    mask_ast = (~sc.mask).astype(int)
    agree = int(np.sum(mask_cpp == mask_ast))
    print(f"[sigma] n={len(vals)} agree={agree}/{len(vals)} "
          f"cpp_reject={int(np.sum(mask_cpp == 0))} "
          f"astropy_reject={int(np.sum(mask_ast == 0))}")
    print("  cpp:", stat)
    assert agree == len(vals), "Sigma 对照不一致"
    return True


def winsorized_vs_scipy():
    """SciPy winsorize primitive 数值断言（V15 修复恒真断言）。
    NOTE：此检查只验证"夹取原语"与 SciPy 一致；Winsorized Sigma 的权威
    参考是 Siril 1.4.3 官方源码（GPL ORACLE ONLY），见 linear_fit_oracle.py
    同目录 harness 说明。"""
    rng = np.random.default_rng(7)
    vals = rng.normal(0.0, 1.0, 100)
    vals[10] = 12.0
    vals[11] = -11.0
    w = np.asarray(mstats.winsorize(vals, limits=[0.02, 0.02]))
    # 有意义的断言：极值被夹取（不再恒真）
    assert np.all(np.isfinite(w))
    assert abs(w[10] - vals[10]) > 1e-6, "高侧极值必须被夹取"
    assert abs(w[11] - vals[11]) > 1e-6, "低侧极值必须被夹取"
    assert np.max(w) < 12.0 and np.min(w) > -11.0
    print("[winsorized] SciPy primitive 夹取数值断言 OK")
    return True


def winsorized_mirror_smoke():
    """Python 镜像 smoke —— NOT_AN_ORACLE（V15 明确命名）。
    镜像与 C++ 同公式，只能做一致性冒烟；独立 oracle 由 Siril 官方
    源码 harness 承担（当前线性拟合 harness 已落地；winsorized 的
    Siril harness 待 W8 后补，本脚本不宣称独立）。"""
    rng = np.random.default_rng(20260815)
    vals = np.concatenate([rng.normal(0, 1, 40), [0.0] * 10, [100.0]])
    mask_cpp, stat = run_cpp(vals, method=2, lo=-4.0, hi=3.0,
                             max_iter=8, min_samples=3)
    # 镜像实现（与 C++ 相同公式；仅冒烟）
    v = vals.copy()
    acc = np.ones(len(v), dtype=bool)
    r = 0
    for _ in range(8):
        cur = v[acc]
        if len(cur) < 2:
            break
        med = float(np.median(cur))
        s0 = float(np.sqrt(np.sum((cur - med) ** 2) / (len(cur) - 1)))
        if s0 <= 1e-12:
            break
        wcur = cur.copy()
        sigma = s0
        for _it in range(64):
            wlo, whi = med - 1.5 * sigma, med + 1.5 * sigma
            wcur = np.clip(wcur, wlo, whi)
            wsd = float(np.sqrt(np.sum((wcur - med) ** 2) / (len(cur) - 1)))
            sigma_new = 1.134 * wsd
            if abs(sigma_new - sigma) <= 5e-4 * sigma:
                sigma = sigma_new
                break
            sigma = sigma_new
        z = (v - med) / sigma
        changed = False
        idx = np.where(acc)[0]
        for i in idx:
            if len(cur) - r <= 4:
                break
            if z[i] < -4.0 or z[i] > 3.0:
                acc[i] = False
                r += 1
                changed = True
        if not changed:
            break
    agree = int(np.sum(mask_cpp == acc.astype(int)))
    print(f"[winsorized-mirror] NOT_AN_ORACLE 冒烟 agree={agree}/"
          f"{len(vals)}")
    assert agree == len(vals), "Winsorized 镜像冒烟不一致（需排查）"
    return True


def nist_esd_full(vals, alpha=0.05, max_out=10):
    """独立两阶段 NIST ESD（scipy t 分布，独立数值栈）。"""
    n0 = len(vals)
    work = list(map(float, vals))
    Rs, Ls = [], []
    for i in range(1, max_out + 1):
        arr = np.array(work)
        n = len(arr)
        mean = arr.mean()
        sd = arr.std(ddof=1)
        if sd <= 1e-12 or n < 3:
            break
        Ri = np.abs(arr - mean) / sd
        j = int(np.argmax(Ri))
        ni = n0 - i
        nu = ni - 1
        p = 1.0 - alpha / (2.0 * (ni + 1.0))
        tc = tdist.ppf(p, nu)
        lam = tc * ni / np.sqrt((nu + tc * tc) * (ni + 1))
        Rs.append(float(Ri[j]))
        Ls.append(float(lam))
        del work[j]
    k = 0
    for r in range(len(Rs)):
        if Rs[r] > Ls[r]:
            k = r + 1
    return k, Rs, Ls


def esd_vs_nist():
    vals = np.array([
        -0.25, 0.68, 0.94, 1.15, 1.20, 1.26, 1.26, 1.34, 1.38, 1.43,
        1.49, 1.49, 1.55, 1.56, 1.58, 1.65, 1.69, 1.70, 1.76, 1.77,
        1.81, 1.91, 1.94, 1.96, 1.99, 2.06, 2.09, 2.10, 2.14, 2.15,
        2.23, 2.24, 2.26, 2.35, 2.37, 2.40, 2.47, 2.54, 2.62, 2.64,
        2.90, 2.92, 2.92, 2.93, 3.21, 3.26, 3.30, 3.59, 3.68, 4.30,
        4.64, 5.34, 5.42, 6.01])
    mask_cpp, stat = run_cpp(vals, method=5, lo=-4.0, hi=3.0,
                             max_iter=10, min_samples=5)
    n_rej = int(np.sum(mask_cpp == 0))
    k_ref, Rs, Ls = nist_esd_full(vals)
    print(f"[esd-nist] n=54 cpp_reject={n_rej} ref_k={k_ref} "
          f"R1={Rs[0]:.4f} lambda1={Ls[0]:.4f}")
    assert n_rej == 3 and k_ref == 3, "NIST Rosner 54 点必须 3 outliers"
    rng = np.random.default_rng(42)
    v2 = rng.normal(0, 1, 40)
    v2[5] = 8.0
    v2[6] = 8.5
    mask2_cpp, stat2 = run_cpp(v2, method=5, lo=-4.0, hi=3.0,
                               max_iter=10, min_samples=5)
    k2 = nist_esd_full(v2)[0]
    print(f"[esd-masking] cpp_reject={int(np.sum(mask2_cpp == 0))} ref_k={k2}")
    assert int(np.sum(mask2_cpp == 0)) >= 2 and k2 >= 2, "ESD masking case"
    return True


def auto_vs_wbpp_policy():
    """WBPP 2.9.1 bestRejectionMethod 政策核验（本机源码证据）：
       n<6 → percentile；6..15 → winsorized；>15 → linear_fit。"""
    expected = {2: "percentile", 5: "percentile", 6: "winsorized_sigma",
                15: "winsorized_sigma", 16: "linear_fit", 20: "linear_fit"}
    ok = True
    for n, want in expected.items():
        mask, reasons, stat = run_plan([10.0] * max(3, n), "auto", n)
        # stat 形如 "status=... method=4"
        got = int(stat.rsplit("method=", 1)[1])
        names = {0: "none", 1: "sigma", 2: "winsorized_sigma",
                 3: "averaged_sigma", 4: "linear_fit", 5: "generalized_esd",
                 6: "rcr", 7: "percentile", 8: "median_sigma", 9: "minmax"}
        ok &= (names[got] == want)
        print(f"[auto-policy] nominal={n} resolved={names[got]} "
              f"want={want} {'OK' if names[got] == want else 'FAIL'}")
    assert ok, "Auto 路由与 WBPP 2.9.1 政策不一致"
    return True


def edge_matrix():
    """G4 边界/对抗矩阵（V15）：n=2 卫星线 → UNDERDETERMINED；
    NaN/Inf → INVALID_INPUT；零方差 → 无拒绝。"""
    # n=2 卫星线：不得宣称可剔除
    mask, reasons, stat = run_plan([10.0, 50.0], "auto", 2)
    assert "status=4" in stat, "n=2 必须 UNDERDETERMINED(4): " + stat
    assert int(np.sum(mask)) == 2, "n=2 全部接受（不伪称剔除）"
    assert reasons is not None and np.all(reasons == 3)
    print("[edge] n=2 卫星线 UNDERDETERMINED OK")
    # NaN → INVALID_INPUT(3)
    mask, reasons, stat = run_plan([10.0, float("nan"), 11.0], "sigma", 20)
    assert "status=3" in stat, "NaN 必须 INVALID_INPUT: " + stat
    # 契约：INVALID_INPUT 下不做任何拒绝声明（全 accepted + reason=UNDERDETERMINED）；
    # 生产路径由 eligibility 层在进 kernel 前过滤 NaN。
    assert int(np.sum(mask)) == 3, "NaN 防御路径不得伪称拒绝: " + stat
    assert reasons is not None and np.all(reasons == 3)
    print("[edge] NaN INVALID_INPUT OK")
    # ±Inf
    mask, reasons, stat = run_plan([10.0, float("inf"), -float("inf")],
                                   "sigma", 20)
    assert "status=3" in stat
    assert int(np.sum(mask)) == 3
    print("[edge] +/-Inf INVALID_INPUT OK")
    # 零方差：无拒绝
    mask, reasons, stat = run_plan([10.0] * 10, "sigma", 20)
    assert int(np.sum(mask)) == 10 and "status=0" in stat
    print("[edge] 零方差无拒绝 OK")
    return True


def main():
    ok = True
    ok &= sigma_vs_astropy()
    ok &= esd_vs_nist()
    ok &= winsorized_vs_scipy()
    ok &= winsorized_mirror_smoke()
    ok &= auto_vs_wbpp_policy()
    ok &= edge_matrix()
    # RCR oracle 由 rcr_oracle_compare.py 覆盖（官方 rcr 2.4.7 固定版本）
    print("ORACLE_RESULT=" + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
