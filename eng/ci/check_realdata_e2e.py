#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-REALDATA-E2E —— 把人工 L4 真实数据 E2E 脚本化（slow label）。

替代人工「跑 normalize x N → mosaic → export → 目视」：
  - --plan     打印将执行的生产命令与配置来源（dry-run，可审计）；
  - --execute  真跑（normalize 每帧配置 → mosaic → 校验产品），慢；
  - --verify-only  只校验已存在产品；
  - --self-test    用夹具产品验证断言逻辑能红能绿。

断言（fail-closed，任一产品缺失/退化即红）：
  E1 p3_writer.json：coverage_stats.covered_px > 0 且 <= total_px；
  E2 p3_verify.json：coverage_ok==1、reopen_ok==1、canonical_match!=false；
  E3 bunit（键存在时）必须 == "ADU/sr"（canonical 面亮度串；DATA_SEMANTICS §31.1a:
     2808-2810 / :2827-2830 —— 测光是否施加不改变 BUNIT）；
  E4 output_fits 存在时：有限像素占比 >= --min-finite-fraction（默认 0.5）；
  E5 零产品命中 ⇒ rc=2（不得空扫描判绿）。

前置（缺失时：--execute/--verify-only 以 77 SKIP，slow/waivable；--plan 仍输出）：
  真实数据 testdata/M42_T2T3_mosaic_Flying_dutchman、生产二进制 build/astrocs、
  normalize/mosaic 配置。

用法：
  python3 eng/ci/check_realdata_e2e.py --plan
  python3 eng/ci/check_realdata_e2e.py --execute [--work-dir DIR]
  python3 eng/ci/check_realdata_e2e.py --verify-only [--work-dir DIR]
  python3 eng/ci/check_realdata_e2e.py --self-test
exit 0 = 通过；exit 1 = finding；exit 2 = 配置/环境错误；exit 77 = 前置缺失（waivable SKIP）。
"""
from __future__ import annotations

import argparse
import glob
import json
import math
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gate_common as gc  # noqa: E402

CHECK_ID = "CHK-REALDATA-E2E"
SKIP_EXIT = 77
DEFAULT_WORK_DIR = "run/RELEASE-02/fix-gates/e2e"
DEFAULT_NORMALIZE_GLOB = "run/RELEASE-01/e2e/l4/configs/p1_m42_*_red.json"
DEFAULT_MOSAIC_CONFIG = "run/RELEASE-02/L4-rebuild/mosaic_49.json"
DEFAULT_BINARY = "build/astrocs"
CANONICAL_SB = "ADU/sr"   # §31.1 FZ-UNIT-SIGNAL-SB canonical 面亮度串


def _load_json(path):
    return json.loads(pathlib.Path(path).read_text(encoding="utf-8"))


def _read_fits_stats(path):
    import numpy as np  # noqa: PLC0415
    from astropy.io import fits  # noqa: PLC0415
    with fits.open(path, memmap=False) as hdul:
        data = np.asarray(hdul[0].data, dtype="float64")
    total = data.size
    finite = int(np.isfinite(data).sum())
    finite_vals = data[np.isfinite(data)]
    return {
        "total_px": total,
        "finite_px": finite,
        "finite_fraction": (finite / total) if total else 0.0,
        "median": float(np.median(finite_vals)) if finite_vals.size else None,
    }


def verify_products(work_dir: pathlib.Path, min_finite_fraction=0.5):
    """返回 (findings, detail)。work_dir 下找 p3_writer.json / p3_verify.json。"""
    findings = []
    writers = sorted(work_dir.glob("**/p3_writer.json"))
    verifiers = sorted(work_dir.glob("**/p3_verify.json"))
    if not writers and not verifiers:
        raise gc.GateError("ANCHOR_STALE: %s 下零 p3_writer.json/p3_verify.json" % work_dir)
    detail = {"writers": [str(p) for p in writers], "verifiers": [str(p) for p in verifiers]}
    for path in writers:
        doc = _load_json(path)
        rel = path.name
        stats = doc.get("coverage_stats") or {}
        covered = stats.get("covered_px")
        total = stats.get("total_px")
        if not isinstance(covered, (int, float)) or covered <= 0:
            findings.append("E2E_COVERAGE:%s covered_px=%r" % (rel, covered))
        elif isinstance(total, (int, float)) and covered > total:
            findings.append("E2E_COVERAGE:%s covered_px>total_px" % rel)
        # B1 口径订正（CONTRACT-GAPS-01）：产品 BUNIT 一律取 canonical 面亮度串，与
        # photometry_applied 解耦（docs/contracts/DATA_SEMANTICS.md §31.1a:2808-2810 /
        # :2827-2830）。键缺失不判（本门只判"写了但写错"）。
        bunit = str(doc.get("bunit", "")).strip()
        if bunit and bunit != CANONICAL_SB:
            findings.append("E2E_BUNIT:%s bunit=%s 非 canonical 面亮度串 %s"
                            "（DATA_SEMANTICS §31.1a）" % (rel, bunit, CANONICAL_SB))
        if int(doc.get("reopen_ok", 1)) == 0:
            findings.append("E2E_REOPEN:%s reopen_ok=0" % rel)
        fits_path = doc.get("output_fits")
        if fits_path and pathlib.Path(fits_path).is_file():
            try:
                fs = _read_fits_stats(fits_path)
                detail.setdefault("fits", {})[fits_path] = fs
                if fs["finite_fraction"] < min_finite_fraction:
                    findings.append("E2E_FINITE:%s finite_fraction=%.4f < %.4f"
                                    % (rel, fs["finite_fraction"], min_finite_fraction))
            except ImportError:
                findings.append("E2E_PREREQ: astropy/numpy 缺失，无法校验 output_fits（fail-closed）")
    for path in verifiers:
        doc = _load_json(path)
        rel = path.name
        if int(doc.get("coverage_ok", 0)) != 1:
            findings.append("E2E_VERIFY:%s coverage_ok!=1" % rel)
        if int(doc.get("reopen_ok", 0)) != 1:
            findings.append("E2E_VERIFY:%s reopen_ok!=1" % rel)
        if doc.get("canonical_match") is False:
            findings.append("E2E_VERIFY:%s canonical_match=false" % rel)
    return findings, detail


def discover(repo: pathlib.Path, normalize_glob, mosaic_config, binary):
    norm_cfgs = sorted(repo.glob(normalize_glob))
    mosaic_cfg = repo / mosaic_config
    bin_path = repo / binary
    missing = []
    if not norm_cfgs:
        missing.append("normalize 配置 %s" % normalize_glob)
    if not mosaic_cfg.is_file():
        missing.append("mosaic 配置 %s" % mosaic_config)
    if not bin_path.is_file():
        missing.append("生产二进制 %s" % binary)
    return norm_cfgs, mosaic_cfg, bin_path, missing


def build_plan(repo, norm_cfgs, mosaic_cfg, bin_path, work_dir):
    steps = []
    for cfg in norm_cfgs:
        steps.append([str(bin_path), "normalize", "--json", str(cfg), "-y", "--events-jsonl"])
    steps.append([str(bin_path), "mosaic", "--json", str(mosaic_cfg), "-y", "--events-jsonl"])
    return {"work_dir": str(work_dir), "steps": steps,
            "expected_products": ["*/p3_writer.json", "*/p3_verify.json", "output_phase3.fits"]}


def execute(repo, plan, work_dir, timeout):
    work_dir.mkdir(parents=True, exist_ok=True)
    for i, cmd in enumerate(plan["steps"]):
        log = work_dir / ("step_%02d.log" % i)
        proc = subprocess.run(cmd, cwd=str(repo), capture_output=True, text=True,
                              timeout=timeout)
        log.write_text(proc.stdout + "\n--- STDERR ---\n" + proc.stderr, encoding="utf-8")
        if proc.returncode != 0:
            print("%s_FAIL: step %d rc=%d cmd=%s（日志 %s）"
                  % (CHECK_ID, i, proc.returncode, " ".join(cmd), log), file=sys.stderr)
            return 1
    return 0


# --------------------------------------------------------------------------- selftest ----
_GOOD_WRITER = {"bunit": "ADU/sr", "reopen_ok": 1,   # canonical 面亮度串（B1 口径订正）
                "coverage_stats": {"covered_px": 100, "total_px": 400},
                "output_fits": None}
_BAD_WRITER = {"bunit": "ADU", "reopen_ok": 1,
               "coverage_stats": {"covered_px": 0, "total_px": 400},
               "output_fits": None}
_BAD_VERIFY = {"coverage_ok": 0, "reopen_ok": 0, "canonical_match": False}
_GOOD_VERIFY = {"coverage_ok": 1, "reopen_ok": 1, "canonical_match": True}


def _write_fixture(root: pathlib.Path, writer, verifier):
    (root / "e2e").mkdir(parents=True, exist_ok=True)
    (root / "e2e/p3_writer.json").write_text(json.dumps(writer), encoding="utf-8")
    (root / "e2e/p3_verify.json").write_text(json.dumps(verifier), encoding="utf-8")


def _selftest() -> int:
    import tempfile

    failures = []
    with tempfile.TemporaryDirectory() as td:
        base = pathlib.Path(td)
        d_ok = base / "ok"
        _write_fixture(d_ok, _GOOD_WRITER, _GOOD_VERIFY)
        findings, _ = verify_products(d_ok / "e2e")
        if findings:
            failures.append("green_e2e: unexpected findings %s" % findings)
        else:
            print("SELFTEST_PASS green_e2e")
        d_bad = base / "bad"
        _write_fixture(d_bad, _BAD_WRITER, _BAD_VERIFY)
        findings, _ = verify_products(d_bad / "e2e")
        if not findings:
            failures.append("red_e2e: expected findings")
        else:
            print("SELFTEST_PASS red_e2e (findings=%d)" % len(findings))
        d_empty = base / "empty"
        (d_empty / "e2e").mkdir(parents=True, exist_ok=True)
        try:
            verify_products(d_empty / "e2e")
            failures.append("zero_products_should_raise: expected GateError")
        except gc.GateError:
            print("SELFTEST_PASS zero_products (fail-closed GateError)")
    if failures:
        for item in failures:
            print("SELFTEST_FAIL: " + item)
        return 1
    print("SELFTEST_PASS: all cases match expectation")
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-REALDATA-E2E 真实数据 E2E（slow）")
    ap.add_argument("--repo", default=str(gc.repo_root()))
    ap.add_argument("--work-dir", default=DEFAULT_WORK_DIR)
    ap.add_argument("--normalize-glob", default=DEFAULT_NORMALIZE_GLOB)
    ap.add_argument("--mosaic-config", default=DEFAULT_MOSAIC_CONFIG)
    ap.add_argument("--binary", default=DEFAULT_BINARY)
    ap.add_argument("--min-finite-fraction", type=float, default=0.5)
    ap.add_argument("--timeout", type=int, default=5400)
    ap.add_argument("--plan", action="store_true")
    ap.add_argument("--execute", action="store_true")
    ap.add_argument("--verify-only", action="store_true", dest="verify_only")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return _selftest()
    repo = pathlib.Path(args.repo).resolve()
    work_dir = (repo / args.work_dir) if not pathlib.Path(args.work_dir).is_absolute() \
        else pathlib.Path(args.work_dir)
    norm_cfgs, mosaic_cfg, bin_path, missing = discover(
        repo, args.normalize_glob, args.mosaic_config, args.binary)

    if args.plan or (not args.execute and not args.verify_only):
        plan = build_plan(repo, norm_cfgs, mosaic_cfg, bin_path, work_dir)
        print(json.dumps({"check_id": CHECK_ID, "missing_prerequisites": missing, **plan},
                         ensure_ascii=False, indent=2))
        # --plan 或未指定模式：只输出计划（dry-run），不进入产品校验（无产物时应显式选
        # --execute/--verify-only，而不是把「没跑」当「没通过」）。
        return 0

    if args.execute:
        if missing:
            print("%s_SKIP: 前置缺失 %s（slow/waivable）" % (CHECK_ID, missing))
            return SKIP_EXIT
        plan = build_plan(repo, norm_cfgs, mosaic_cfg, bin_path, work_dir)
        rc = execute(repo, plan, work_dir, args.timeout)
        if rc != 0:
            return rc

    try:
        findings, detail = verify_products(work_dir, args.min_finite_fraction)
    except gc.GateError as exc:
        print("%s_FAIL: %s" % (CHECK_ID, exc), file=sys.stderr)
        return 2
    if findings:
        gc.print_findings(CHECK_ID, findings)
        gc.report("FAIL", CHECK_ID, findings, detail, args.json_out)
        return 1
    print("%s_PASS writers=%d verifiers=%d" % (CHECK_ID, len(detail["writers"]),
                                               len(detail["verifiers"])))
    gc.report("PASS", CHECK_ID, [], detail, args.json_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())