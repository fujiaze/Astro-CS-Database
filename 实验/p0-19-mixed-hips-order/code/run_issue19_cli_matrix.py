#!/usr/bin/env python3
"""P0-19（Issue #19）跨 hips_order fail-closed —— 真实 CLI 正/反例矩阵。

驱动**真实生产入口** `acsd mosaic --json cfg -y`（build/linux-sci-p5/acsd），
不模拟任何语义。夹具由生产写侧生成（make_hips_fixture.cpp；见同目录）。

用例（Issue #19 施工任务书要求）：
  · 同阶正例：(0,0) 同路径、(0,0) 异滤镜、(0,0) 异设备/异 order?（同阶）、
    (1,1) 异滤镜、单帧 (0)/(1)、显式空 obs_filter；
  · 混阶反例：(0,1)、(1,0) 排列互换、(0,1,0) 三帧、异滤镜混阶；
  · 期望：只有混阶被拒（CLI 退出码 3=INPUT），不发布 p2_coverage.json；
    同阶照常通过 coverage（后续因夹具缺观测几何在 sky_plane 失败，属预期，
    不是本次修复引入——pre-fix 完全相同）。

用法:
    python3 实验/p0-19-mixed-hips-order/code/run_issue19_cli_matrix.py \
        [--acsd build/linux-sci-p5/acsd] [--work /tmp/p019] [--evidence <dir>]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
NAMED = "mixed hips_order unsupported without spherical resampling"

# (case, hips_order/filter 组合, 期望)
# order 序列 → 夹具；同名可重复。expect: ok=coverage 通过, reject=必须拒绝。
CASES = [
    ("same00_samepath", [("ord0_halpha", 0, "Halpha")] * 2, "ok"),
    ("same00_filter_diff", [("ord0_halpha", 0, "Halpha"),
                            ("ord0_red", 0, "Red")], "ok"),
    ("same00_empty_filter", [("ord0_halpha", 0, "Halpha"),
                             ("ord0_empty", 0, "")], "ok"),
    ("same11_filter_diff", [("ord1_halpha", 1, "Halpha"),
                            ("ord1_red", 1, "Red")], "ok"),
    ("single0", [("ord0_halpha", 0, "Halpha")], "ok"),
    ("single1", [("ord1_halpha", 1, "Halpha")], "ok"),
    ("mixed01", [("ord0_halpha", 0, "Halpha"),
                 ("ord1_halpha", 1, "Halpha")], "reject"),
    ("mixed10_perm", [("ord1_halpha", 1, "Halpha"),
                      ("ord0_halpha", 0, "Halpha")], "reject"),
    ("mixed010_three", [("ord0_halpha", 0, "Halpha"),
                        ("ord1_halpha", 1, "Halpha"),
                        ("ord0_empty", 0, "")], "reject"),
    ("mixed01_filter_diff", [("ord0_halpha", 0, "Halpha"),
                             ("ord1_red", 1, "Red")], "reject"),
]


def sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def ensure_fixtures(work: str) -> str:
    fx = os.path.join(work, "fx")
    need = ["ord0_halpha", "ord1_halpha", "ord0_red", "ord0_empty"]
    if all(os.path.isdir(os.path.join(fx, n)) for n in need):
        return fx
    gen = "/tmp/make_hips_fixture"
    if not os.path.exists(gen):
        src = os.path.join(REPO, "实验/p0-19-mixed-hips-order/code/make_hips_fixture.cpp")
        cmd = ["g++", "-std=c++17", "-O2",
               "-I", os.path.join(REPO, "lib/infrastructure/aio/include"),
               "-I", os.path.join(REPO, "lib/infrastructure/aio/src"), src,
               "-Wl,--start-group",
               os.path.join(REPO, "build/linux-sci-p5/libacsd_aio.a"),
               os.path.join(REPO, "build/linux-sci-p5/libacsd_cfitsio.a"),
               os.path.join(REPO, "build/linux-sci-p5/libacsd_common.a"),
               os.path.join(REPO, "build/linux-sci-p5/libacsd_hips.a"),
               os.path.join(REPO, "build/linux-sci-p5/libacsd_core.a"),
               "-Wl,--end-group", "-lz", "-lpthread", "-o", gen]
        subprocess.run(cmd, check=True)
    os.makedirs(fx, exist_ok=True)
    for name, order, filt in [("ord0_halpha", 0, "Halpha"), ("ord1_halpha", 1, "Halpha"),
                              ("ord0_red", 0, "Red"), ("ord0_empty", 0, "")]:
        subprocess.run([gen, os.path.join(fx, name), str(order), filt],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return fx


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--acsd", default="build/linux-sci-p5/acsd")
    ap.add_argument("--work", default="/tmp/p019")
    ap.add_argument("--evidence",
                    default=os.path.join(REPO, "实验/p0-19-mixed-hips-order/evidence"))
    a = ap.parse_args()
    acsd = a.acsd if os.path.isabs(a.acsd) else os.path.join(REPO, a.acsd)
    if not os.path.exists(acsd):
        print("SKIP: acsd not found: %s" % acsd)
        return 0
    os.makedirs(a.evidence, exist_ok=True)
    logs = os.path.join(a.evidence, "02-post-fix-cli-logs")
    os.makedirs(logs, exist_ok=True)
    fx = ensure_fixtures(a.work)

    results = []
    failures = 0
    for name, specs, expect in CASES:
        out_dir = os.path.join(a.work, "matrix", name)
        shutil.rmtree(out_dir, ignore_errors=True)
        os.makedirs(out_dir, exist_ok=True)
        cfg = {"schema_version": "1",
               "hips_paths": [os.path.join(fx, s[0]) for s in specs],
               "output_dir": out_dir}
        cfg_path = os.path.join(logs, name + ".cfg.json")
        with open(cfg_path, "w") as f:
            json.dump(cfg, f, indent=2)
        p = subprocess.run([acsd, "mosaic", "--json", cfg_path, "-y"],
                           capture_output=True, text=True)
        err = p.stderr
        with open(os.path.join(logs, name + ".stderr"), "w") as f:
            f.write(err)
        with open(os.path.join(logs, name + ".stdout"), "w") as f:
            f.write(p.stdout)
        cov = os.path.join(out_dir, "p2_coverage.json")
        cov_present = os.path.exists(cov)
        named = NAMED in (err + p.stdout)
        # 从 stderr 截取报错行（不含绝对路径噪声）
        err_line = ""
        for line in err.splitlines():
            if "phase2 failed" in line or "error final" in line:
                err_line = line.strip()
        if expect == "ok":
            ok = cov_present and not named
        else:
            ok = (p.returncode == 3) and (not cov_present) and named
        if not ok:
            failures += 1
        results.append({
            "case": name,
            "orders_filters": [[s[1], s[2]] for s in specs],
            "expect": expect,
            "exit_code": p.returncode,
            "coverage_artifact_present": cov_present,
            "named_error_present": named,
            "pass": ok,
            "stderr_line": err_line,
            "config": cfg_path,
            "stderr_log": os.path.join(logs, name + ".stderr"),
        })
        print("[%s] %-22s exit=%d coverage=%-8s named=%s" %
              ("PASS" if ok else "FAIL", name, p.returncode,
               "present" if cov_present else "ABSENT", named))

    # 排列不变性：mixed01 与 mixed10_perm 的诊断行逐字相同（去掉 run_id/时间）
    def norm(line):
        import re
        return re.sub(r"\[[0-9T:\-Z]+\] seq=\d+ ", "", line)
    m01 = next(r for r in results if r["case"] == "mixed01")["stderr_line"]
    m10 = next(r for r in results if r["case"] == "mixed10_perm")["stderr_line"]
    perm_invariant = norm(m01) == norm(m10)

    report = {
        "issue": "https://github.com/fujiaze/Astro-CS-Database/issues/19",
        "acsd": os.path.relpath(acsd, REPO),
        "acsd_sha256": sha256(acsd),
        "fixtures_dir": fx,
        "fixture_properties_sha256": {
            n: sha256(os.path.join(fx, n, "signal", "properties"))
            for n in ["ord0_halpha", "ord1_halpha", "ord0_red", "ord0_empty"]
        },
        "named_error": NAMED,
        "permutation_invariant_diagnostic": perm_invariant,
        "cases": results,
        "n_fail": failures,
    }
    with open(os.path.join(a.evidence, "02-post-fix-cli-matrix.json"), "w") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
    print("permutation-invariant diagnostic: %s" % perm_invariant)
    print("report: 实验/p0-19-mixed-hips-order/evidence/02-post-fix-cli-matrix.json")
    print("FAIL=%d / %d cases" % (failures, len(CASES)))
    return 1 if (failures or not perm_invariant) else 0


if __name__ == "__main__":
    sys.exit(main())
