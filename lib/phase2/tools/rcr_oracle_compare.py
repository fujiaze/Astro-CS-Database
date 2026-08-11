# lib/phase2/tools/rcr_oracle_compare.py — RCR 独立 oracle 对照
#
# Oracle：官方 Robust Chauvenet Rejection（nickk124/robust-outlier-rejection，
# commit a8a29a6，Python 包 rcr 2.4.7）——仅 test oracle，不进入生产。
# 生产：lib/phase2 rejection.cpp P2_REJECT_RCR（论文独立实现）。
# 比较：rejected set（逐数据集）。
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
    return d


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


def run_official(vals):
    r = RCR()
    r.setRejectionTech(SS_MEDIAN_DL)
    r.performRejection(np.asarray(vals, dtype=float))
    # 官方 result.indices = 保留（clean）索引；拒绝 = 全集 - clean
    clean = set(int(i) for i in r.result.indices)
    return set(range(len(vals))) - clean


def main():
    os.makedirs(OUT, exist_ok=True)
    rows = []
    for name, vals in datasets().items():
        rej_cpp = run_cpp(vals)
        rej_off = run_official(vals)
        inter = len(rej_cpp & rej_off)
        union = len(rej_cpp | rej_off)
        jac = inter / union if union else 1.0
        rows.append({
            "case": name,
            "n": int(len(vals)),
            "cpp_rejected": sorted(rej_cpp),
            "official_rejected": sorted(rej_off),
            "jaccard": round(jac, 3),
        })
        print(f"[rcr-oracle] {name:18s} n={len(vals):3d} "
              f"cpp={len(rej_cpp):2d} off={len(rej_off):2d} "
              f"jaccard={jac:.2f}")
    ref = {
        "oracle": "nickk124/robust-outlier-rejection",
        "commit": "a8a29a602a482bce7a9f184af3fd5381a87928e3",
        "python_pkg": "rcr 2.4.7",
        "license": "non-commercial (oracle only)",
        "cases": rows,
    }
    with open(os.path.join(OUT, "rcr_reference_vectors.json"), "w",
              encoding="utf-8") as f:
        json.dump(ref, f, ensure_ascii=False, indent=2)
    # 门限（如实报告差异）：生产为论文核心独立实现（median/MAD +
    # 官方公开 nCorrect 近似 1.2591^(n^0.2052)）；官方校准表（LSDLUnityCF/
    # SSUnity + CF 修正）在小 n 与高污染下更精确，因此：
    #   - 至少 4/6 case Jaccard >= 0.7；
    #   - 无 case Jaccard = 0；
    #   - 简单/中污染 case（symmetric/asymmetric/dominant/satellite）>= 0.7；
    #   - small_n / high_contam 如实报告（>= 0.3）。
    ok = True
    n_ge07 = 0
    for r_ in rows:
        if r_["jaccard"] >= 0.7:
            n_ge07 += 1
        if r_["jaccard"] == 0.0:
            ok = False
        if r_["case"] in ("symmetric_2x6", "asymmetric_high",
                          "dominant_50pct", "satellite_streak"):
            if r_["jaccard"] < 0.7:
                ok = False
        elif r_["jaccard"] < 0.3:
            ok = False
    if n_ge07 < 4:
        ok = False
    print("RCR_ORACLE=" + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
