# lib/phase2/tools/linear_fit_oracle.py — Linear Fit Clipping reference oracle
#
# Reference 语义：Siril 1.4.3（rejection_float.c LINEARFIT 分支，公开算法
# 定义，GPL ORACLE ONLY）：
#   每轮：保留样本按值排序 → 最小二乘 y=a*x+b（x=排序后序号）→
#   sigma=mean|residual| → low: fit-y > sigma*siglow；high:
#   y-fit > sigma*sighigh → 迭代至无新增拒绝或 n<=3。
# 测试数据：Siril tests/rejection_test.c 官方 set1/set2。
# 生产：lib/phase2 rejection.cpp P2_REJECT_LINEAR_FIT（同一定义独立实现）。
import json
import os
import subprocess
import sys

import numpy as np

CLI = r"F:\Astro dev\Astro CS Normalization Database\lib\phase2\build\rejection_cli.exe"
OUT = r"F:\Astro dev\Astro CS Normalization Database\run\phase2\linear_fit_oracle"

SET1 = [145, 125, 190, 135, 220, 130, 210, 3, 165, 165, 150, 350,
        170, 180, 195, 440, 215, 135, 410, 40, 140, 175]
SET2 = [7.7110e-2, 4.7330e-1, 5.7340e-1, 3.3310e-1, 5.3160e-1, 3.6550e-1,
        3.1900e-1, 3.4650e-1, 2.2340e-1, 5.3680e-1, 4.8200e-1, 4.8150e-1,
        2.5420e-1, 7.3770e-1, 6.6930e-1, 3.8980e-1, 5.8780e-1, 6.6680e-1,
        6.9580e-1, 3.6260e-1, 7.1870e-1, 2.6420e-1, 5.2890e-1, 6.1350e-1,
        2.4980e-1, 2.7930e-1, 7.9300e-1, 6.6690e-1, 5.9180e-1, 6.5240e-1,
        8.4440e-2, 8.1500e-1, 3.5880e-1, 3.7450e-1, 5.6660e-1, 2.5050e-1,
        5.6520e-1, 4.6880e-1, 9.7020e-2, 4.9380e-1]


def siril_linear_fit_mask(vals, siglow=-4.0, sighigh=3.0, max_iter=8):
    """按 Siril 公开算法定义独立实现（frozen reference 生成器）。"""
    work = list(map(float, vals))
    orig_idx = list(range(len(vals)))
    for _ in range(max_iter):
        n = len(work)
        if n <= 3:
            break
        order = sorted(range(n), key=lambda k: work[k])   # 值排序
        y = [work[k] for k in order]
        x = list(range(n))
        # 最小二乘
        sx = sum(x)
        sy = sum(y)
        sxx = sum(xi * xi for xi in x)
        sxy = sum(xi * yi for xi, yi in zip(x, y))
        den = n * sxx - sx * sx
        a = (n * sxy - sx * sy) / den if abs(den) > 1e-12 else 0.0
        b = (sy - a * sx) / n
        sigma = sum(abs(yi - (a * xi + b)) for xi, yi in zip(x, y)) / n
        if sigma <= 1e-12:
            break
        reject = [False] * n
        changed = False
        for j in range(n):
            fit = a * j + b
            if fit - y[j] > sigma * abs(siglow):
                reject[j] = True
                changed = True
            elif y[j] - fit > sigma * sighigh:
                reject[j] = True
                changed = True
        if not changed:
            break
        work = [work[k] for k in range(n) if not reject[k]]
        # 维护原始索引映射
        orig_idx = [orig_idx[order[k]] for k in range(n) if not reject[k]]
    keep = set(orig_idx)
    return np.array([1 if i in keep else 0 for i in range(len(vals))],
                    dtype=int)


def run_cpp(vals):
    os.environ["PATH"] = (
        r"C:\msys64\mingw64\bin;" +
        r"F:\Astro dev\Astro CS Normalization Database\lib\astro_image_io;" +
        os.environ.get("PATH", ""))
    txt = "\n".join(repr(float(v)) for v in vals)
    r = subprocess.run(
        [CLI, "4", "-4", "3", "8", "3"], input=txt,
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=60)
    if r.returncode != 0:
        raise RuntimeError("cli failed: " + r.stderr)
    return np.array([int(c) for c in r.stdout.splitlines()[0].split()],
                    dtype=int)


def main():
    os.makedirs(OUT, exist_ok=True)
    rows = []
    ok = True
    cases = [("set1", SET1), ("set2", SET2),
             ("set1_outlier_500", SET1[:10] + [500.0] + SET1[10:]),
             ("set1_outlier_300", SET1[:10] + [300.0] + SET1[10:])]
    for name, vals in cases:
        ref = siril_linear_fit_mask(vals)
        cpp = run_cpp(vals)
        agree = int(np.sum(ref == cpp))
        rej_ref = int(np.sum(ref == 0))
        rej_cpp = int(np.sum(cpp == 0))
        rows.append({
            "case": name,
            "n": len(vals),
            "siril_reference_mask": ref.tolist(),
            "cpp_mask": cpp.tolist(),
            "agree": agree,
            "siril_rejected": rej_ref,
            "cpp_rejected": rej_cpp,
        })
        print(f"[linear-oracle] {name} n={len(vals)} agree={agree}/{len(vals)} "
              f"siril_rej={rej_ref} cpp_rej={rej_cpp}")
        if agree != len(vals):
            ok = False
    ref = {
        "reference": "Siril 1.4.3 rejection_float.c LINEARFIT (public algorithm "
                     "definition; GPL ORACLE ONLY)",
        "test_data": "siril-1.4.3/src/tests/rejection_test.c set1/set2",
        "cases": rows,
    }
    with open(os.path.join(OUT, "linear_fit_reference.json"), "w",
              encoding="utf-8") as f:
        json.dump(ref, f, ensure_ascii=False, indent=2)
    print("LINEAR_FIT_ORACLE=" + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
