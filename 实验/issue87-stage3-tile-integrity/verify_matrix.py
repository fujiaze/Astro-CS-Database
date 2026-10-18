#!/usr/bin/env python3
"""Issue #87 / E2：修复前后 A/B 与合同判据的机器校验（只读证据，不重跑）。

读取 evidence/<prefix-tag>/results.json 与 evidence/<fixed-tag>/results.json
（默认 prefix/fixed），生成 evidence/verify.json：每条判据 = 期望 / 实测 /
PASS|FAIL。数字全部来自真实 CLI 运行记录；本脚本只做比较，不调用 acsd，
不修改产物。

独立重建复算（同一 476c023 源码、新构建的基线二进制）：
  python3 verify_matrix.py --prefix-tag prefix_rebuild --fixed-tag fixed_rebuild \
      --out evidence/verify_rebuild.json
"""
from __future__ import annotations

import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EVID = os.path.join(HERE, "evidence")


def load(tag: str) -> dict:
    return json.load(open(os.path.join(EVID, tag, "results.json"), encoding="utf-8"))


def index(doc: dict) -> dict[tuple[str, int], dict]:
    return {(r["case"], r["repeat"]): r for r in doc["results"]}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--prefix-tag", default="prefix")
    ap.add_argument("--fixed-tag", default="fixed")
    ap.add_argument("--out", default=os.path.join(EVID, "verify.json"))
    args = ap.parse_args()
    pre, fix = load(args.prefix_tag), load(args.fixed_tag)
    P, F = index(pre), index(fix)
    checks = []

    def add(name: str, expected, actual, ok: bool, note: str = "") -> None:
        checks.append({"check": name, "expected": expected, "actual": actual,
                       "pass": bool(ok), "note": note})

    # 1) A/B：所有 expect=fail 场景在修复前必须 exit0 + FITS + complete
    fail_cases = sorted({r["case"] for r in pre["results"] if r["expect"] == "fail"})
    for c in fail_cases:
        r = P[(c, 0)]
        ok = (r["exit_code"] == 0 and r["fits_present"]
              and (r["manifest"] or {}).get("status") == "complete")
        add(f"prefix_defect[{c}]",
            "exit=0 && FITS && manifest=complete",
            {"exit": r["exit_code"], "fits": r["fits_present"],
             "manifest": (r["manifest"] or {}).get("status")}, ok)
        rf = F[(c, 0)]
        ok2 = (rf["exit_code"] != 0 and not rf["fits_present"]
               and not rf["resampled_bin_present"] and not rf["resampled_json_present"]
               and not rf["writer_json_present"] and not rf["verify_json_present"]
               and (rf["manifest"] or {}).get("status") == "incomplete"
               and rf["tile_failures_present"] and not rf["tmp_leftovers"]
               and not rf["staging_dirs"])
        add(f"fixed_failclosed[{c}]",
            "nonzero exit && no FITS/bin/resampled/writer/verify && "
            "manifest=incomplete && p3_tile_failures.json && no tmp",
            {"exit": rf["exit_code"], "fits": rf["fits_present"],
             "bin": rf["resampled_bin_present"],
             "resampled_json": rf["resampled_json_present"],
             "writer": rf["writer_json_present"], "verify": rf["verify_json_present"],
             "manifest": (rf["manifest"] or {}).get("status"),
             "tile_failures": rf["tile_failures_present"],
             "tmp": rf["tmp_leftovers"]}, ok2)

    # 2) 合法/真实用例：修复前后 FITS signal 逐位一致 + bin 逐位一致
    ok_cases = sorted({r["case"] for r in pre["results"] if r["expect"] == "ok"})
    for c in ok_cases:
        a, b = P[(c, 0)], F[(c, 0)]
        fa, fb = a.get("fits") or {}, b.get("fits") or {}
        sa, sb = fa.get("data_sha256"), fb.get("data_sha256")
        add(f"bitwise_signal[{c}]", sa, sb, bool(sa) and sa == sb)
        ba = (a.get("resampled") or {}).get("bin_sha256")
        bb = (b.get("resampled") or {}).get("bin_sha256")
        add(f"bitwise_bin[{c}]", ba, bb, bool(ba) and ba == bb)

    # 3) 分类码 / tile 集合 / 排序去重（修复后）
    expect = {
        "declared_missing": ("P3-TILE-DECLARED-MISSING", [5]),
        "invalid_dims": ("P3-TILE-INVALID", [4]),
        "invalid_bitpix": ("P3-TILE-INVALID", [4]),
        "truncated": ("P3-TILE-INVALID", [4]),
        "checksum_bad": ("P3-TILE-CHECKSUM-FAILED", [4]),
        "io_error": ("P3-TILE-IO-ERROR", [4]),
        "undetermined_coarse_moc": ("P3-TILE-FOOTPRINT-UNDETERMINED", [5]),
        "undetermined_manifest_only": ("P3-TILE-FOOTPRINT-UNDETERMINED", [5]),
        "multi_bad": ("P3-TILE-DECLARED-MISSING", [0, 4, 5, 8]),
    }
    for c, (code, tiles) in expect.items():
        r = F[(c, 0)]
        got_tiles = r.get("failed_tiles") or []
        got_codes = r.get("failed_codes") or []
        ok = (code in (r.get("codes") or []) and got_tiles == tiles
              and got_tiles == sorted(got_tiles) and len(got_tiles) == len(set(got_tiles)))
        add(f"classification[{c}]", {"code": code, "tiles": tiles},
            {"codes": r.get("codes"), "tiles": got_tiles,
             "per_tile_codes": got_codes}, ok)

    # 3b) multi_bad 的 4 种码各 1 条
    r = F[("multi_bad", 0)]
    by = r.get("failed_by_code") or {}
    want = {"P3-TILE-DECLARED-MISSING": 1, "P3-TILE-INVALID": 1,
            "P3-TILE-CHECKSUM-FAILED": 1, "P3-TILE-IO-ERROR": 1}
    add("aggregate[multi_bad_by_code]", want, by, by == want)

    # 4) 同一坏 tile 多 worker：一次运行 = 1 条；3 次重复 tile/code 稳定
    reps = [F[("same_bad_multi_worker", i)] for i in range(3)]
    tiles = [r.get("failed_tiles") for r in reps]
    codes = [r.get("failed_codes") for r in reps]
    ok = (all(t == [4] for t in tiles) and all(c == ["P3-TILE-CHECKSUM-FAILED"] for c in codes))
    add("aggregate[same_bad_repeat_stable]", {"tiles": [[4]] * 3},
        {"tiles": tiles, "codes": codes}, ok)
    wc = [(F[("same_bad_multi_worker", i)].get("tile_failures") or {}).get("tile_errors", [{}])[0]
          .get("worker_count", 0) for i in range(3)]
    add("aggregate[same_bad_multi_worker]", "one entry + worker_count>1", wc,
        all(w > 1 for w in wc))

    # 5) CHECKSUM 明确规则
    ck_ok = (F[("checksum_ok", 0)].get("tile_cache") or {})
    ck_absent = (F[("checksum_absent", 0)].get("tile_cache") or {})
    add("checksum[present_verified]", "checksum_present_reads>0",
        ck_ok.get("checksum_present_reads"), (ck_ok.get("checksum_present_reads") or 0) > 0)
    add("checksum[absent_not_failure]", "exit=0 && checksum_absent_reads>0 && no tile error",
        {"exit": F[("checksum_absent", 0)]["exit_code"],
         "absent_reads": ck_absent.get("checksum_absent_reads"),
         "tile_error_entries": ck_absent.get("tile_error_entries")},
        F[("checksum_absent", 0)]["exit_code"] == 0
        and (ck_absent.get("checksum_absent_reads") or 0) > 0
        and (ck_absent.get("tile_error_entries") or 0) == 0)

    # 6) 合法稀疏 / 真实未覆盖：NaN 语义
    for c in ("sparse_moc_outside", "unhinted_outside"):
        r = F[(c, 0)]
        f = r.get("fits") or {}
        add(f"legal_nan[{c}]", "all NaN, exit 0",
            {"exit": r["exit_code"], "nan": f.get("nan_px"),
             "finite": f.get("finite_px")}, r["exit_code"] == 0
            and f.get("finite_px") == 0 and f.get("nan_px") == 4096)
    rp = F[("real_partial", 0)]
    fp = rp.get("fits") or {}
    add("real_partial[uncovered_nan_kept]", "exit 0 && nan>0 && finite>0",
        {"exit": rp["exit_code"], "nan": fp.get("nan_px"),
         "finite": fp.get("finite_px"), "sha": fp.get("data_sha256")},
        rp["exit_code"] == 0 and (fp.get("nan_px") or 0) > 0
        and (fp.get("finite_px") or 0) > 0)

    # 7) 单 PRIMARY signal HDU / BUNIT 形状（所有成功用例）
    for c in ok_cases:
        r = F[(c, 0)]
        f = r.get("fits") or {}
        ok = (f.get("nhdu") == 1 and f.get("naxis") == 2
              and f.get("bitpix") == -32 and f.get("bunit") == "ADU/sr")
        add(f"product_shape[{c}]", "1 HDU PRIMARY, NAXIS=2, BITPIX=-32, BUNIT=ADU/sr",
            {"nhdu": f.get("nhdu"), "naxis": f.get("naxis"),
             "bitpix": f.get("bitpix"), "bunit": f.get("bunit")}, ok)

    n_fail = sum(1 for c in checks if not c["pass"])
    out = {"schema": "acsd.issue87.verify/v1",
           "prefix_tag": args.prefix_tag,
           "fixed_tag": args.fixed_tag,
           "prefix_acsd_sha256": pre["acsd_sha256"],
           "fixed_acsd_sha256": fix["acsd_sha256"],
           "n_checks": len(checks), "n_fail": n_fail, "checks": checks}
    path = args.out if os.path.isabs(args.out) else os.path.join(HERE, args.out)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)
        f.write("\n")
    for c in checks:
        print(f"[{'PASS' if c['pass'] else 'FAIL'}] {c['check']}")
    print(f"wrote {path}: {len(checks)} checks, {n_fail} fail")
    return 0 if n_fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
