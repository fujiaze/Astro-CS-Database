#!/usr/bin/env python3
"""IMPL-P3-RSMP-001 证据汇总 → run/v6/p3-rsmp/evidence.json（run/ 为 gitignored 工作区）。"""
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
WORK = os.path.join(ROOT, "run", "v6", "p3-rsmp")
BUILD = os.path.join(WORK, "build")
TESTS = ["p3_rsmp_core_test", "p3_rsmp_oracle_test", "p3_rsmp_gate_test"]


def sh(cmd):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=120)
    return p.returncode, p.stdout.strip()


def main():
    head_rc, head = sh(["git", "rev-parse", "HEAD"])
    counts = {}
    for t in TESTS:
        exe = os.path.join(BUILD, t)
        if not os.path.exists(exe):
            counts[t] = {"rc": None, "note": "binary_missing"}
            continue
        p = subprocess.run([exe], capture_output=True, text=True, timeout=300)
        counts[t] = {"rc": p.returncode, "summary": (p.stdout or p.stderr).strip().splitlines()[-1]}
    mut_path = os.path.join(WORK, "mutation_summary.json")
    mut = None
    if os.path.exists(mut_path):
        with open(mut_path, "r", encoding="utf-8") as f:
            mut = json.load(f)
    ev = {
        "task": "IMPL-P3-RSMP-001",
        "title": "Phase3 科学重采样传播实现",
        "verification_head": head if head_rc == 0 else None,
        "reported_task_baseline_head": "44e1cb6596e38fac778b70ff75a92be5049331ad",
        "head_note": "main 在并行 W5 期间由控制器集成姊妹任务提交推进；44e1cb65 为祖先",
        "branch": "main",
        "build": {"generator": "Ninja", "build_type": "Release",
                  "standalone_cmake": "eng/tests/unit/v6_p3_rsmp/standalone/CMakeLists.txt"},
        "ctest_log": "run/v6/p3-rsmp/logs/03_ctest.log",
        "test_binaries": counts,
        "mutations": mut.get("counts") if mut else None,
        "mutation_detail": mut.get("mutations") if mut else None,
        "oracle_independence": {
            "file": "eng/tests/unit/v6_p3_rsmp/p3_rsmp_oracle.h",
            "does_not_include_production": True,
            "truth_sources": ["analytic_bilinear_error_bound",
                              "explicit_dense_matrix_transcription",
                              "fixed_seed_monte_carlo (SEED=20260915)"],
            "independent_solver": "gaussian_elimination_partial_pivoting (production uses Cholesky)",
        },
        "frozen_nodes": {
            "FZ-P3-MODES": ["G-P3-MODE-01", "parse_mode legacy/deferred reject"],
            "FZ-P3-FAILCLOSED": ["G-P3-SB-01..04", "G-P3-PSF-01..06", "G-P3-VIS-01", "G-P3-GLB-01"],
            "FZ-P3-QW-RECOMPUTE": ["G-P3-QW-01..04", "output-frame Q/W recompute",
                                   "unit + MC cross-check"],
            "FZ-P3-KERNEL-REGISTRY": ["G-P3-KRN-01..03", "bilinear_4quad independent oracle"],
            "FZ-P3-BUNIT-QUADRATIC": ["G-P3-SB-03", "is_quadratic_variance / is_inverse_pair"],
            "FZ-FORMULA-COV-PROP": ["C_y=R C_x R^T", "correlation deficit detected"],
            "CF-T-P3-CORR-EPSILON": ["OPEN -> fail-closed (epsilon_corr_ratified=false)"],
            "FZ-PROV-MINIMAL-SET": ["G-P3-PROV-01"],
        },
        "checks_total": None,
    }
    total = 0
    for t, v in counts.items():
        s = v.get("summary", "")
        if "checks=" in s:
            total += int(s.split("checks=")[1].split()[0])
    ev["checks_total"] = total
    os.makedirs(WORK, exist_ok=True)
    with open(os.path.join(WORK, "evidence.json"), "w", encoding="utf-8") as f:
        json.dump(ev, f, indent=1, ensure_ascii=False)
    print("evidence.json written: checks_total=%s mutations=%s" %
          (ev["checks_total"], ev["mutations"]))
    ok = (counts[TESTS[0]]["rc"] == 0 and counts[TESTS[1]]["rc"] == 0 and
          counts[TESTS[2]]["rc"] == 0 and mut is not None and
          mut["counts"]["caught"] == mut["counts"]["mutations"] and mut["counts"]["pristine_ok"])
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
