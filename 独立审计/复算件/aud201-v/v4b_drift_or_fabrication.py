# -*- coding: utf-8 -*-
"""
V4b：逐键判"漂移 vs 从未成立"。
对每个改动过 defaults.json 的提交，取该提交里 6 个 photometry 键的 source_ref，
在同一提交的历史版本权威文档上测 P1（该行是否真含该常数的陈述）。
=> 若某键的锚"曾经成立、后来破了"= 登记面与文档漂移；
=> 若"在所有登记面上都不成立"= 登记时即为虚标。
"""
import io
import json
import os
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8")
ROOT = r"F:\Astro dev\Astro CS Normalization Database"
OUT = os.path.dirname(os.path.abspath(__file__))
DEF = "eng/packaging/config/defaults.json"

PHOT = {
    "photometry.mag_tolerance": ["mag_tolerance"],
    "photometry.tukey_c": ["4.685"],
    "photometry.irls_max_iterations": ["max_iter=50", "50"],
    "photometry.irls_tolerance_dex": ["tol=1e-6", "1e-6"],
    "photometry.min_reference_stars": ["r_consistent"],
    "photometry.min_inlier_stars": ["r_inliers"],
}


def show(ref, path):
    r = subprocess.run(["git", "-C", ROOT, "show", f"{ref}:{path}"],
                       capture_output=True, text=True, encoding="utf-8")
    return r.stdout if r.returncode == 0 else None


def commits_touching(path):
    r = subprocess.run(["git", "-C", ROOT, "log", "--format=%h %ad", "--date=short", "--", path],
                       capture_output=True, text=True, encoding="utf-8")
    return [l.split() for l in r.stdout.splitlines() if l.strip()]


def main():
    cs = commits_touching(DEF)
    print("defaults.json 改动提交数:", len(cs))
    hist = {k: [] for k in PHOT}
    for sha, date in cs:
        raw = show(sha, DEF)
        if raw is None:
            continue
        try:
            doc = json.loads(raw)
        except json.JSONDecodeError:
            continue
        fmap = {f["key"]: f for f in doc.get("fields", []) if isinstance(f, dict)}
        for k, pats in PHOT.items():
            f = fmap.get(k)
            if not f:
                hist[k].append((sha, date, None, None, "键未登记"))
                continue
            ref = f.get("source_ref") or {}
            path, line = ref.get("path"), ref.get("line")
            if not path or not isinstance(line, int):
                hist[k].append((sha, date, f"{path}:{line}", None, "无 source_ref"))
                continue
            doclines = show(sha, path)
            if doclines is None:
                hist[k].append((sha, date, f"{path}:{line}", None, "文档不存在"))
                continue
            L = doclines.splitlines()
            txt = L[line - 1] if 1 <= line <= len(L) else ""
            ok = any(p in txt for p in pats if p)
            # 同时给该常数在该版文档里的真实行
            real = [i for i, l in enumerate(L, 1) if any(p in l for p in pats[:1])]
            hist[k].append((sha, date, f"{path}:{line}", ok,
                            f"真实行 {real[:4]}" if real else "文档内无该陈述"))
    res = {}
    for k, rows in hist.items():
        ever_true = [r for r in rows if r[3] is True]
        res[k] = {
            "n_commits_checked": len(rows),
            "ever_true": bool(ever_true),
            "last_true": (ever_true[0][0], ever_true[0][1]) if ever_true else None,
            "true_count": len(ever_true),
            "false_count": len([r for r in rows if r[3] is False]),
            "timeline_newest_first": [(r[0], r[1], r[2], r[3], r[4]) for r in rows[:8]],
        }
    io.open(os.path.join(OUT, "v4b_drift_or_fabrication.json"), "w",
            encoding="utf-8").write(json.dumps(res, indent=2, ensure_ascii=False))
    print(json.dumps(res, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
