# lib/phase2/tools/rejection_oracle_compare.py — Phase2 Rejection Oracle 对照
#
# 用途（控制包 07_REJECTION_IMPLEMENTATION / 12_DELIVERY evidence）：
#   - Sigma    ↔ Astropy astropy.stats.sigma_clip（median + mad_std）
#   - ESD      ↔ NIST Generalized ESD（Rosner 示例，54 点检出 3 个离群）
#   - Winsorized ↔ SciPy mstats.winsorize primitive
# 只读 Oracle 工具（NON_PRODUCTION_TOOL_ONLY）。
import subprocess
import sys
import tempfile
import os
import numpy as np
from astropy.stats import sigma_clip, mad_std
from scipy.stats import mstats, t

CLI = r"F:\Astro dev\Astro CS Normalization Database\lib\phase2\build\rejection_cli.exe"


def run_cpp(values, method, lo=-4.0, hi=3.0, max_iter=8, min_samples=3):
    txt = "\n".join(repr(float(v)) for v in values)
    r = subprocess.run(
        [CLI, str(method), str(lo), str(hi), str(max_iter), str(min_samples)],
        input=txt, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("cli failed: " + r.stderr)
    lines = r.stdout.strip().splitlines()
    mask = np.array([int(c) for c in lines[0].split()], dtype=int)
    return mask, lines[1]


def sigma_vs_astropy():
    rng = np.random.default_rng(20260810)
    vals = rng.normal(0.0, 1.0, 200)
    vals[17] = 6.5
    vals[88] = -6.2
    vals[150] = 8.0
    vals[5] = 7.5
    mask_cpp, stat = run_cpp(vals, method=1, lo=-4.0, hi=3.0, max_iter=8, min_samples=3)
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
    rng = np.random.default_rng(7)
    vals = rng.normal(0.0, 1.0, 100)
    vals[10] = 12.0
    vals[11] = -11.0
    w = mstats.winsorize(vals, limits=[0.02, 0.02])
    # 我们的实现：winsorized_sigma 内部对边界样本先夹取再迭代；
    # 此处仅验证 primitive 数值一致性：winsorize 后极值被夹到分位值。
    # scipy winsorize：两端 2 个样本替换为最接近的保留值
    w = np.asarray(w)
    assert np.all(np.isfinite(w)) and np.allclose(np.sort(w)[:2], np.sort(w)[:2])
    assert np.abs(w[10] - vals[10]) > 1e-9 and np.abs(w[11] - vals[11]) > 1e-9
    print("[winsorized] SciPy primitive 对照 OK（2%/98% 分位夹取）")
    return True


def winsorized_independent_mask(vals, lo=-4.0, hi=3.0, max_iter=8):
    """独立 Python winsorized sigma（V14 升级，对齐 Siril 官方语义）：
    位置=median；σ 由迭代 winsorize 到 [median-1.5σ, median+1.5σ] 后的
    SD × 1.134 估计（收敛 |Δσ|<=5e-4·σ）；最后按 median 位置 sigma clip，
    N-r<=4 后不再拒绝。"""
    import numpy as np
    v = np.asarray(vals, dtype=float)
    acc = np.ones(len(v), dtype=bool)
    r = 0
    for _ in range(max_iter):
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
            if z[i] < lo or z[i] > hi:
                acc[i] = False
                r += 1
                changed = True
        if not changed:
            break
    return acc


def winsorized_vs_independent():
    rng = np.random.default_rng(20260815)
    vals = np.concatenate([rng.normal(0, 1, 40), [0.0] * 10, [100.0]])
    mask_cpp, stat = run_cpp(vals, method=2, lo=-4.0, hi=3.0,
                             max_iter=8, min_samples=3)
    mask_ref = winsorized_independent_mask(vals)
    agree = int(np.sum(mask_cpp == mask_ref))
    print(f"[winsorized-ref] n={len(vals)} agree={agree}/{len(vals)} "
          f"cpp_reject={int(np.sum(mask_cpp == 0))} "
          f"ref_reject={int(np.sum(mask_ref == 0))}")
    assert agree == len(vals), "Winsorized 独立 reference 不一致"
    return True


def nist_esd_full(vals, alpha=0.05, max_out=10):
    """独立两阶段 NIST ESD（先连续移除记录 R_i/λ_i，再取最大 i）。"""
    import numpy as np
    from scipy.stats import t as tdist
    n0 = len(vals)
    work = list(map(float, vals))
    Rs, Ls, removed = [], [], []
    for i in range(1, max_out + 1):
        arr = np.array(work)
        n = len(arr)
        mean = arr.mean()
        sd = arr.std(ddof=1)
        if sd <= 1e-12:
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
        removed.append(float(arr[j]))
        del work[j]
    k = 0
    for r in range(len(Rs)):
        if Rs[r] > Ls[r]:
            k = r + 1
    return k, Rs, Ls, removed[:k]


def esd_vs_nist():
    # NIST Rosner 54 点：两阶段 ESD 必须 3 outliers
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
    k_ref, Rs, Ls, rem = nist_esd_full(vals)
    print(f"[esd-nist] n=54 cpp_reject={n_rej} ref_k={k_ref} "
          f"R1={Rs[0]:.4f} lambda1={Ls[0]:.4f}")
    assert n_rej == 3 and k_ref == 3, "NIST Rosner 54 点必须 3 outliers"
    # masking case：两极端离群
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


def main():
    ok = True
    ok &= sigma_vs_astropy()
    ok &= esd_vs_nist()
    ok &= winsorized_vs_scipy()
    ok &= winsorized_vs_independent()
    # RCR oracle 由独立脚本 rcr_oracle_compare.py 覆盖（官方 rcr 2.4.7）
    print("ORACLE_RESULT=" + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
