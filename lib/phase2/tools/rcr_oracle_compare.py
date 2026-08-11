# lib/phase2/tools/rcr_oracle_compare.py — RCR 独立 oracle 对照
#
# Oracle：官方 Robust Chauvenet Rejection（nickk124/robust-outlier-rejection，
# commit a8a29a6，Python 包 rcr 2.4.7）——仅 test oracle，不进入生产。
# 生产：lib/phase2 rejection.cpp P2_REJECT_RCR（论文独立实现）。
# 比较：rejected set（逐数据集，逐元素精确对齐）。
import json
import os
import subprocess
import sys

import numpy as np
from rcr import RCR, SS_MEDIAN_DL

CLI = r"F:\Astro dev\Astro CS Normalization Database\lib\phase2\build\rejection_cli.exe"
OUT = r"F:\Astro dev\Astro CS Normalization Database\run\phase2\rcr_oracle"


def datasets():
    rng = np.random.default_rng(20260819)
    d = {}
    d["symmetric_2x6"] = np.concatenate(
        [rng.normal(10, 1, 40), [16.0, 4.0]])
    d["asymmetric_high"] = np.concatenate(
        [rng.normal(10, 1, 40), [25.0, 26.0, 27.0]])
    d["high_contam_20pct"] = np.concatenate(
        [rng.normal(10, 1, 80), rng.normal(25, 2, 20)])
    d["dominant_50pct"] = np.concatenate(
        [rng.normal(10, 1, 50), rng.normal(22, 2, 50)])
    d["small_n_8"] = np.array([10, 10.2, 9.8, 10.1, 9.9, 30.0, 31.0, 10.05])
    d["satellite_streak"] = np.concatenate(
        [rng.normal(10, 1, 60), [14.0, 14.5, 15.0, 15.2]])
    # V4 R4 扩展：中间污染 / 加权 case（G4 要求 symmetric、asymmetric
    # one-sided、intermediate、high contamination、dominant、small-N、
    # weighted）
    d["intermediate_15pct"] = np.concatenate(
        [rng.normal(10, 1, 85), rng.normal(30, 2, 15)])
    d["weighted_bulk"] = np.array(
        [10.0, 10.2, 9.8, 10.1, 9.9, 10.05, 30.0, 31.0, 10.3, 10.15,
         10.25, 10.12, 9.95, 10.18, 10.22])
    return d


def weighted_sets():
    # 加权 case：w 与 y 同序（官方 performRejection(w, y) 权重语义）
    return {
        "weighted_bulk": (
            np.array([1.0, 1.1, 0.9, 1.2, 0.8, 1.05, 3.0, 3.0, 1.0, 1.0,
                      1.0, 1.0, 1.0, 1.0, 1.0]),
            datasets()["weighted_bulk"],
        ),
        "weighted_low_outlier": (
            np.array([1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
                      1.0, 1.0, 1.0, 1.0, 1.0, 1.0]),
            np.array([10.0, 10.2, 9.8, 10.1, 9.9, 10.05, 10.3, 10.15,
                      10.25, 10.12, 9.95, 10.18, 10.22, 2.0, 10.1, 10.0]),
        ),
    }


def run_cpp(vals):
    os.environ["PATH"] = (
        r"C:\msys64\mingw64\bin;" +
        r"F:\Astro dev\Astro CS Normalization Database\lib\astro_image_io;" +
        os.environ.get("PATH", ""))
    txt = "\n".join(repr(float(v)) for v in vals)
    r = subprocess.run(
        [CLI, "6", "-4", "3", "8", "3"], input=txt,
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=60)
    if r.returncode != 0:
        raise RuntimeError("cli failed: " + r.stderr)
    mask = np.array([int(c) for c in r.stdout.splitlines()[0].split()],
                    dtype=bool)
    return set(np.nonzero(mask == 0)[0].tolist())


def run_cpp_weighted(vals, weights):
    os.environ["PATH"] = (
        r"C:\msys64\mingw64\bin;" +
        r"F:\Astro dev\Astro CS Normalization Database\lib\astro_image_io;" +
        os.environ.get("PATH", ""))
    txt = "\n".join(f"{repr(float(w))} {repr(float(v))}"
                    for w, v in zip(weights, vals))
    r = subprocess.run(
        [CLI, "6", "-4", "3", "8", "3", "1"], input=txt,
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=60)
    if r.returncode != 0:
        raise RuntimeError("cli failed: " + r.stderr)
    mask = np.array([int(c) for c in r.stdout.splitlines()[0].split()],
                    dtype=bool)
    return set(np.nonzero(mask == 0)[0].tolist())


def run_official(vals):
    r = RCR()
    r.setRejectionTech(SS_MEDIAN_DL)
    r.performRejection(np.asarray(vals, dtype=float))
    # 官方 result.indices = 保留（clean）索引；拒绝 = 全集 - clean
    clean = set(int(i) for i in r.result.indices)
    return set(range(len(vals))) - clean


def run_official_weighted(vals, weights):
    r = RCR()
    r.setRejectionTech(SS_MEDIAN_DL)
    r.performRejection(np.asarray(weights, dtype=float),
                       np.asarray(vals, dtype=float))
    clean = set(int(i) for i in r.result.indices)
    return set(range(len(vals))) - clean


def main():
    os.makedirs(OUT, exist_ok=True)
    rows = []
    ok = True
    for name, vals in datasets().items():
        rej_cpp = run_cpp(vals)
        rej_off = run_official(vals)
        exact = rej_cpp == rej_off
        only_cpp = sorted(rej_cpp - rej_off)
        only_off = sorted(rej_off - rej_cpp)
        if not exact:
            ok = False
        rows.append({
            "case": name,
            "n": int(len(vals)),
            "cpp_rejected": sorted(rej_cpp),
            "official_rejected": sorted(rej_off),
            "exact_match": exact,
            "only_cpp": only_cpp,
            "only_official": only_off,
        })
        print(f"[rcr-oracle] {name:22s} n={len(vals):3d} "
              f"cpp={len(rej_cpp):2d} off={len(rej_off):2d} "
              f"exact={exact}"
              + (f" only_cpp={only_cpp} only_off={only_off}" if not exact
                 else ""))
    for name, (weights, vals) in weighted_sets().items():
        rej_cpp = run_cpp_weighted(vals, weights)
        rej_off = run_official_weighted(vals, weights)
        exact = rej_cpp == rej_off
        only_cpp = sorted(rej_cpp - rej_off)
        only_off = sorted(rej_off - rej_cpp)
        if not exact:
            ok = False
        rows.append({
            "case": name + "_weighted",
            "n": int(len(vals)),
            "cpp_rejected": sorted(rej_cpp),
            "official_rejected": sorted(rej_off),
            "exact_match": exact,
            "only_cpp": only_cpp,
            "only_official": only_off,
        })
        print(f"[rcr-oracle] {name + '_weighted':22s} n={len(vals):3d} "
              f"cpp={len(rej_cpp):2d} off={len(rej_off):2d} "
              f"exact={exact}"
              + (f" only_cpp={only_cpp} only_off={only_off}" if not exact
                 else ""))
    # permutation gate：输入顺序重排 → rejected mask（按 frame_id/原索引）
    # 不变（生产 RCR 对值排序取中位数、残差极值 first-wins 均与顺序无关）
    perm_cases = {}
    for name, vals in datasets().items():
        if name in ("symmetric_2x6", "high_contam_20pct", "small_n_8"):
            perm_cases[name] = vals
    for name, vals in perm_cases.items():
        rng = np.random.default_rng(20260819)
        perm = rng.permutation(len(vals))
        vals_p = vals[perm]
        rej_p = run_cpp(vals_p)
        rej_orig = run_cpp(vals)
        # perm[p] = 重排后位置 p 处的原索引
        rej_p_orig = {int(perm[i]) for i in rej_p}
        perm_ok = rej_p_orig == rej_orig
        if not perm_ok:
            ok = False
        rows.append({
            "case": name + "_permutation",
            "n": int(len(vals)),
            "cpp_rejected": sorted(rej_orig),
            "official_rejected": sorted(rej_p_orig),
            "exact_match": perm_ok,
            "only_cpp": sorted(rej_orig - rej_p_orig),
            "only_official": sorted(rej_p_orig - rej_orig),
        })
        print(f"[rcr-oracle] {name + '_permutation':22s} n={len(vals):3d} "
              f"permutation_invariant={perm_ok}")
    ref = {
        "oracle": "nickk124/robust-outlier-rejection",
        "commit": "a8a29a602a482bce7a9f184af3fd5381a87928e3",
        "python_pkg": "rcr 2.4.7",
        "license": "non-commercial (oracle only)",
        "technique": "SS_MEDIAN_DL (performRejection sequential "
                     "MEDIAN+DOUBLE_LINE -> MEDIAN+68th -> MEAN+SD)",
        "compare_semantics": "rejected mask 逐元素一致；任何差异必须"
                             "逐样本解释且限定为浮点 tie，不允许算法级偏差",
        "cases": rows,
    }
    with open(os.path.join(OUT, "rcr_reference_vectors.json"), "w",
              encoding="utf-8") as f:
        json.dump(ref, f, ensure_ascii=False, indent=2)
    print("RCR_ORACLE=" + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
