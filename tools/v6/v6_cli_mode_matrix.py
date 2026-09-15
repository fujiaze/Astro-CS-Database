#!/usr/bin/env python3
"""RUNTIME-CI-001 CLI 模式路由矩阵（真实二进制端到端）。

对已构建的 astrocs 二进制跑冻结模式路由矩阵：
  * phase2 production {point_information, surface_gls, psfsw_robust} → 不被门拒绝；
  * phase2 baseline {equal, pixel_ivar} → 放行但告警（非生产）；
  * phase2 拒绝 {psf_snr_power, auto, support_x_snr2, 0, 1, 2, bogus} → rc=2 + 明确拒绝；
  * phase3 {surface_brightness, point_source_flux, visualization} → 门放行；
  * phase3 非法 token → rc=2 + 拒绝；
  * phase1 不得接受 --mode（parser 白名单拒绝）；
  * config legacy weight_mode: 0 拒绝 / 1|2 baseline 放行。

用法:
  python3 tools/v6/v6_cli_mode_matrix.py --cli-bin <astrocs> [--work <dir>] [--json-out <path>]
缺失二进制 → rc 2（fail-closed，不静默通过）。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys

REJECT_P2 = ["psf_snr_power", "auto", "support_x_snr2", "0", "1", "2", "bogus"]
ACCEPT_P2 = ["point_information", "surface_gls", "psfsw_robust"]
BASE_P2 = ["equal", "pixel_ivar"]
REJECT_P3 = ["psf_snr_power", "auto", "support_x_snr2", "bogus"]
ACCEPT_P3 = ["surface_brightness", "point_source_flux", "visualization"]


def run(binary, args, cwd):
    p = subprocess.run([binary] + args, capture_output=True, text=True, timeout=300, cwd=cwd)
    return p.returncode, (p.stderr or "") + (p.stdout or "")


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--cli-bin", required=True)
    ap.add_argument("--work", default="run/v6/RUNTIME-CI-001/cli-matrix")
    ap.add_argument("--json-out", default="")
    args = ap.parse_args(argv)
    binary = pathlib.Path(args.cli_bin).resolve()
    if not binary.exists():
        print("V6_CLI_MATRIX_FAIL(prerequisite): astrocs binary missing: %s -> fail-closed"
              % binary, file=sys.stderr)
        _emit(args.json_out, {"verdict": "FAIL", "reason": "binary missing", "cases": []})
        return 2

    work = pathlib.Path(args.work)
    work.mkdir(parents=True, exist_ok=True)
    out_dir = work / "out"
    out_dir.mkdir(parents=True, exist_ok=True)
    cfg = work / "cfg.json"
    cfg.write_text(json.dumps({"hips_paths": ["run/v6/RUNTIME-CI-001/cli-matrix/x.hips"],
                               "output_dir": str(out_dir)}), encoding="utf-8")
    cfg_wm0 = work / "cfg_wm0.json"
    cfg_wm0.write_text(json.dumps({"schema_version": "1", "weight_mode": 0,
                                   "hips_paths": ["x.hips"], "output_dir": str(out_dir)}),
                       encoding="utf-8")
    cfg_wm2 = work / "cfg_wm2.json"
    cfg_wm2.write_text(json.dumps({"schema_version": "1", "weight_mode": 2,
                                   "hips_paths": ["x.hips"], "output_dir": str(out_dir)}),
                       encoding="utf-8")

    cases = []
    fails = 0

    def check(cid, cond, detail):
        nonlocal fails
        cases.append({"id": cid, "ok": bool(cond), "detail": detail})
        if not cond:
            fails += 1
            print("  [FAIL] %s: %s" % (cid, detail))
        else:
            print("  [ok] %s" % cid)

    for tok in REJECT_P2:
        rc, err = run(binary, ["phase2", "validate", "--config", str(cfg), "--mode", tok], ".")
        check("phase2-reject-" + tok, rc == 2 and "rejected" in err,
              "rc=%d err=%s" % (rc, err.strip()[:120]))
    # 未显式给出模式旗标：保持既有缺省路径（不被模式门拒绝）。显式选择才是生产入口。
    rc, err = run(binary, ["phase2", "validate", "--config", str(cfg)], ".")
    check("phase2-default-absent-mode", rc == 0 and "rejected:" not in err,
          "rc=%d err=%s" % (rc, err.strip()[:120]))
    for tok in ACCEPT_P2 + BASE_P2:
        rc, err = run(binary, ["phase2", "validate", "--config", str(cfg), "--mode", tok], ".")
        check("phase2-accept-" + tok, rc == 0 and "rejected:" not in err,
              "rc=%d err=%s" % (rc, err.strip()[:120]))
    for tok in REJECT_P3:
        rc, err = run(binary, ["phase3", "validate", "--config", str(cfg),
                               "--export-mode", tok], ".")
        check("phase3-reject-" + tok, rc == 2 and "rejected" in err,
              "rc=%d err=%s" % (rc, err.strip()[:120]))
    for tok in ACCEPT_P3:
        rc, err = run(binary, ["phase3", "validate", "--config", str(cfg),
                               "--export-mode", tok], ".")
        check("phase3-gate-accept-" + tok, "FZ-P3-MODES" not in err and "--export-mode rejected" not in err,
              "rc=%d err=%s" % (rc, err.strip()[:120]))
    # phase1 无模式旗标
    rc, err = run(binary, ["phase1", "validate", "--config", str(cfg),
                           "--mode", "point_information"], ".")
    check("phase1-no-mode-flag", rc == 2 and "not allowed" in err,
          "rc=%d err=%s" % (rc, err.strip()[:120]))
    # config legacy weight_mode
    rc, err = run(binary, ["phase2", "validate", "--config", str(cfg_wm0)], ".")
    check("config-weight-mode-0-reject", rc == 2 and "weight_mode" in err and "rejected" in err,
          "rc=%d err=%s" % (rc, err.strip()[:140]))
    rc, err = run(binary, ["phase2", "validate", "--config", str(cfg_wm2)], ".")
    check("config-weight-mode-2-baseline", "rejected:" not in err,
          "rc=%d err=%s" % (rc, err.strip()[:140]))

    verdict = "PASS" if fails == 0 else "FAIL"
    _emit(args.json_out, {"schema": "astrocs.v6.cli-mode-matrix/v1",
                          "cli_bin": str(binary), "cases": cases, "verdict": verdict,
                          "failures": fails})
    if fails == 0:
        print("V6_CLI_MODE_MATRIX_PASS cases=%d" % len(cases))
        return 0
    print("V6_CLI_MODE_MATRIX_FAIL failures=%d/%d" % (fails, len(cases)), file=sys.stderr)
    return 1


def _emit(path, doc):
    if not path:
        return
    p = pathlib.Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
