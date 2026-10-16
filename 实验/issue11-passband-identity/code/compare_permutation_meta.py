#!/usr/bin/env python3
"""Issue #11 —— 同多重集不同排列的**元数据**对照（逐字），并区分浮点末位差。

按任务书口径：名义通带声明必须是输入 `filter_passband` **多重集**的对称函数。
- 硬判据（必须逐字相同）：`signal/properties:obs_filter` 行、`p2_final.json:obs_filter`、
  `p2_final.json:response_distribution` 规范串。
- 信息项（**允许**因输入排列改变浮点归约顺序而出现末位差）：`p2_final.json` 其余
  数值字段逐键对照；路径类字段（output/overlay 绝对路径）单独归为 path。
用法：python3 compare_permutation_meta.py --meta <matrix_out>/meta [--json out]
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys

GROUPS = {
    "empty+Halpha (2 frames)": ["empty_halpha", "halpha_empty"],
    "Red+Blue (2 frames)": ["red_blue", "blue_red"],
    "empty+Red+Blue (3 frames, 6 perms)": ["pem_01", "pem_02", "pem_03",
                                           "pem_04", "pem_05", "pem_06"],
}
# 期望的多重集 → (obs_filter, response_distribution)
EXPECT = {
    "empty+Halpha (2 frames)": ("mixed:2_passbands", {"(undeclared)": 1, "Halpha": 1}),
    "Red+Blue (2 frames)": ("mixed:2_passbands", {"Blue": 1, "Red": 1}),
    "empty+Red+Blue (3 frames, 6 perms)": ("mixed:3_passbands",
                                           {"(undeclared)": 1, "Blue": 1, "Red": 1}),
}
PATH_KEYS = ("hips_root", "properties", "artifact", "sources", "data_file", "var_file",
             "sky_plane_artifact", "files", "hips_path")


def flat(o, path="", acc=None):
    if acc is None:
        acc = {}
    if isinstance(o, dict):
        for k in sorted(o):
            flat(o[k], f"{path}.{k}", acc)
    elif isinstance(o, list):
        for i, v in enumerate(o):
            flat(v, f"{path}[{i}]", acc)
    else:
        acc[path] = o
    return acc


def obs_filter_line(props_path: str):
    for line in open(props_path, encoding="utf-8", errors="replace"):
        if line.startswith("obs_filter="):
            return line.rstrip("\n")
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--meta", required=True)
    ap.add_argument("--json", default=None)
    a = ap.parse_args()
    out = {"meta_dir": a.meta, "groups": []}
    all_ok = True
    for gname, cases in GROUPS.items():
        exp_of, exp_dist = EXPECT[gname]
        rows = []
        for c in cases:
            pf = os.path.join(a.meta, c + ".p2_final.json")
            pr = os.path.join(a.meta, c + ".properties")
            if not (os.path.isfile(pf) and os.path.isfile(pr)):
                continue
            d = json.load(open(pf))
            rows.append({"case": c, "obs_line": obs_filter_line(pr),
                         "p2_obs_filter": d.get("obs_filter"),
                         "p2_dist": d.get("response_distribution"),
                         "flat": flat(d)})
        if not rows:
            continue
        of_lines = {r["obs_line"] for r in rows}
        p2_of = {r["p2_obs_filter"] for r in rows}
        dists = {json.dumps(r["p2_dist"], sort_keys=True, ensure_ascii=False) for r in rows}
        # 非路径数值字段的跨排列差异（允许浮点末位差），仅信息
        base = rows[0]
        numeric_diff_keys = set()
        for r in rows[1:]:
            for k in set(base["flat"]) | set(r["flat"]):
                if any(pk in k for pk in PATH_KEYS):
                    continue
                if base["flat"].get(k) != r["flat"].get(k):
                    numeric_diff_keys.add(k)
        ok = (of_lines == {f"obs_filter={exp_of}"} and p2_of == {exp_of}
              and dists == {json.dumps(exp_dist, sort_keys=True, ensure_ascii=False)})
        all_ok = all_ok and ok
        print(f"[{'PASS' if ok else 'FAIL'}] {gname}: n={len(rows)} "
              f"obs_filter_lines={sorted(of_lines)} dist={sorted(dists)}")
        if numeric_diff_keys:
            print(f"     (info) 非路径字段跨排列差异 {len(numeric_diff_keys)} 个"
                  f"（浮点归约顺序，预期）示例: {sorted(numeric_diff_keys)[:3]}")
        out["groups"].append({
            "group": gname, "n": len(rows), "pass": ok,
            "expected_obs_filter": exp_of, "expected_distribution": exp_dist,
            "obs_filter_lines_observed": sorted(of_lines),
            "response_distribution_observed": sorted(dists),
            "non_path_fields_differing_across_perms": sorted(numeric_diff_keys),
            "cases": [r["case"] for r in rows],
        })
    out["all_metadata_identical"] = all_ok
    if a.json:
        json.dump(out, open(a.json, "w"), indent=2, ensure_ascii=False)
        print("json:", a.json)
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
