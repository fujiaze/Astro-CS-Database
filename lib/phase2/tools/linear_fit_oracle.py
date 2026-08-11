# lib/phase2/tools/linear_fit_oracle.py — Linear Fit 真外部 frozen reference
#
# Reference：实际运行未修改的 Siril 1.4.3 官方源码（V4 R5 方案 B）：
#   - src/stacking/rejection_float.c（未修改，#include 进驱动，
#     与官方 src/tests/rejection_test.c 相同做法）→ line_clipping；
#   - src/stacking/siril_fit_linear.c（未修改）；
#   - src/algos/sorting.c quicksort_f（88-127 行逐字，驱动内镜像同交换序）；
#   - 主循环与官方 rejection_test.c `linearfit_test` 逐行一致（仅增加
#     原索引跟踪以输出 mask）。
# 驱动/编译命令记录在 evidence/oracle/linear_fit_provenance.json。
# 生产：lib/phase2 rejection.cpp P2_REJECT_LINEAR_FIT。
# 比较：production mask 与 Siril frozen mask 逐元素一致。
import json
import os
import subprocess
import sys

import numpy as np

CLI = r"F:\Astro dev\Astro CS Normalization Database\lib\phase2\build\rejection_cli.exe"
HARNESS = (r"F:\Astro dev\Astro CS Normalization Database\run\temp"
           r"\p2_v4_evidence\siril_harness\siril_linearfit_oracle.exe")
OUT = r"F:\Astro dev\Astro CS Normalization Database\run\phase2\linear_fit_oracle"
PROVENANCE = {
    "reference": ("Siril 1.4.3 官方源码实际运行（GPL ORACLE ONLY，不进入生产；"
                  "审核包不打包第三方源码）"),
    "version": "1.4.3",
    "sources_unmodified": [
        "siril-1.4.3/src/stacking/rejection_float.c (line_clipping)",
        "siril-1.4.3/src/stacking/siril_fit_linear.c (siril_fit_linear)",
        "siril-1.4.3/src/algos/sorting.c:88-127 (quicksort_f/insertionSort_f, "
        "逐字提取；驱动内 quicksort_track 为同交换序列 + 原索引镜像)",
    ],
    "harness_loop": ("官方 src/tests/rejection_test.c linearfit_test 逐行一致"
                     "（xf=1/(j+1) 加权、m_x=(n-1)/2、mean|residual| sigma、"
                     "N-r<=4 停止拒绝、n<=3 退出），仅附加原索引跟踪"),
    "build_command": (
        "gcc -O2 -Wno-implicit-function-declaration -ffunction-sections "
        "-fdata-sections -I run/temp/p2_v4_evidence/siril_harness/include "
        "-I siril-1.4.3/src run/temp/p2_v4_evidence/siril_harness/"
        "siril_linearfit_oracle.c run/temp/p2_v4_evidence/siril_harness/"
        "oracle_stubs.c siril-1.4.3/src/stacking/siril_fit_linear.c "
        "-Wl,--gc-sections -o siril_linearfit_oracle.exe"),
    "verify": ("official rejection_test.c 期望值 set2 sig=2.5/2.5: "
               "rej[0]=3 rej[1]=2 mean=0.476394 —— harness 输出 "
               "rejected_lo=3 rejected_hi=2 accepted=35"),
}

SET1 = [145, 125, 190, 135, 220, 130, 210, 3, 165, 165, 150, 350,
        170, 180, 195, 440, 215, 135, 410, 40, 140, 175]
SET2 = [7.7110e-2, 4.7330e-1, 5.7340e-1, 3.3310e-1, 5.3160e-1, 3.6550e-1,
        3.1900e-1, 3.4650e-1, 2.2340e-1, 5.3680e-1, 4.8200e-1, 4.8150e-1,
        2.5420e-1, 7.3770e-1, 6.6930e-1, 3.8980e-1, 5.8780e-1, 6.6680e-1,
        6.9580e-1, 3.6260e-1, 7.1870e-1, 2.6420e-1, 5.2890e-1, 6.1350e-1,
        2.4980e-1, 2.7930e-1, 7.9300e-1, 6.6690e-1, 5.9180e-1, 6.5240e-1,
        8.4440e-2, 8.1500e-1, 3.5880e-1, 3.7450e-1, 5.6660e-1, 2.5050e-1,
        5.6520e-1, 4.6880e-1, 9.7020e-2, 4.9380e-1]


def run_siril_harness(vals, siglow=4.0, sighigh=3.0):
    """运行官方 Siril 1.4.3 源码 harness，返回 (mask, stdout)。"""
    os.environ["PATH"] = (
        r"C:\msys64\mingw64\bin;" +
        r"F:\Astro dev\Astro CS Normalization Database\lib\astro_image_io;" +
        os.environ.get("PATH", ""))
    txt = "\n".join(repr(float(v)) for v in vals)
    r = subprocess.run(
        [HARNESS, repr(siglow), repr(sighigh)], input=txt,
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=60)
    if r.returncode != 0:
        raise RuntimeError("cli failed: " + r.stderr)
    mask = np.array([int(c) for c in r.stdout.splitlines()[0].split()],
                    dtype=int)
    return mask, r.stdout


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


def datasets():
    rng = np.random.default_rng(20260821)
    base = rng.normal(10.0, 0.6, 60)
    d = {}
    # no-outlier：窄散布小样本 → 4/3 sigma 下 Siril 无拒绝
    d["no_outlier"] = rng.normal(10.0, 0.2, 16)
    d["one_low"] = np.concatenate([base[:30], [2.0], base[30:]])
    d["one_high"] = np.concatenate([base[:30], [30.0], base[30:]])
    d["both_sides"] = np.concatenate([base[:20], [2.0], base[20:40],
                                      [30.0], base[40:]])
    # iterative：第一轮拒绝一个极端后，第二轮再暴露一个
    d["iterative"] = np.concatenate([base[:20], [2.0], base[20:40],
                                     [28.0, 32.0], base[40:]])
    # official test data（官方期望值已验证 harness）
    d["siril_set1"] = np.array(SET1, dtype=float)
    d["siril_set2"] = np.array(SET2, dtype=float)
    return d


def main():
    os.makedirs(OUT, exist_ok=True)
    rows = []
    ok = True
    for name, vals in datasets().items():
        ref, stdout = run_siril_harness(vals)
        cpp = run_cpp(vals)
        agree = int(np.sum(ref == cpp))
        rej_ref = int(np.sum(ref == 0))
        rej_cpp = int(np.sum(cpp == 0))
        if rej_ref == 0:
            print(f"[linear-oracle] {name} WARN: Siril 拒绝数为 0")
        rows.append({
            "case": name,
            "n": len(vals),
            "siril_harness_mask": ref.tolist(),
            "cpp_mask": cpp.tolist(),
            "agree": agree,
            "siril_rejected": rej_ref,
            "cpp_rejected": rej_cpp,
            "siril_stdout": stdout.strip(),
        })
        print(f"[linear-oracle] {name} n={len(vals)} agree={agree}/{len(vals)} "
              f"siril_rej={rej_ref} cpp_rej={rej_cpp}")
        if agree != len(vals):
            ok = False
    # permutation：Siril 与生产都在重排后给出相同 rejected set（按原索引）
    rng = np.random.default_rng(20260821)
    perm_case = datasets()["both_sides"]
    perm = rng.permutation(len(perm_case))
    ref_p, _ = run_siril_harness(perm_case[perm])
    cpp_p = run_cpp(perm_case[perm])
    rej_p_ref = set(int(perm[i]) for i in np.nonzero(ref_p == 0)[0])
    rej_p_cpp = set(int(perm[i]) for i in np.nonzero(cpp_p == 0)[0])
    ref_orig, _ = run_siril_harness(perm_case)
    cpp_orig = run_cpp(perm_case)
    rej_ref_orig = set(int(i) for i in np.nonzero(ref_orig == 0)[0])
    rej_cpp_orig = set(int(i) for i in np.nonzero(cpp_orig == 0)[0])
    perm_ok = (rej_p_ref == rej_ref_orig) and (rej_p_cpp == rej_cpp_orig) \
        and (rej_p_ref == rej_p_cpp)
    if not perm_ok:
        ok = False
    rows.append({
        "case": "both_sides_permutation",
        "n": len(perm_case),
        "siril_harness_mask": ref_p.tolist(),
        "cpp_mask": cpp_p.tolist(),
        "agree": int(np.sum(ref_p == cpp_p)),
        "siril_rejected": int(np.sum(ref_p == 0)),
        "cpp_rejected": int(np.sum(cpp_p == 0)),
        "permutation_invariant": perm_ok,
    })
    print(f"[linear-oracle] both_sides_permutation n={len(perm_case)} "
          f"permutation_invariant={perm_ok}")
    ref = {
        "provenance": PROVENANCE,
        "cases": rows,
    }
    with open(os.path.join(OUT, "linear_fit_reference.json"), "w",
              encoding="utf-8") as f:
        json.dump(ref, f, ensure_ascii=False, indent=2)
    print("LINEAR_FIT_ORACLE=" + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
