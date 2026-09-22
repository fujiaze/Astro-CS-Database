#!/usr/bin/env python3
"""RUNTIME-CI-001 CLI 模式路由矩阵（真实二进制端到端；CLI-001 命令树对齐版）。

对已构建的 astrocs 二进制跑冻结模式路由矩阵。命令面 = ASTROCS_DESIGN §6.2 唯一
命令树（normalize|mosaic|export (--json <cfg>|--template|--help) + help + --version
+ doctor + benchmark）；'phase1|2|3 <op>' 用户命令与 '--config' 旗标已随 CLI-001
删除（lib/infrastructure/cli/parser.cpp:31-33/39-43 一律 unknown command/flag → 2），
故本矩阵改在 mosaic/export 的 '--json' 运行面上驱动同一冻结路由门
（lib/infrastructure/cli/v6_mode_gate.h + v6_runtime_contract.h）：

  * mosaic 权重口径 token **全部拒绝**（FZ-WEIGHT-SINGLE-PATH：不存在可选择口径）：
    {point_information, surface_gls, psfsw_robust, psf_snr_power, auto, support_x_snr2,
     0, 1, 2, bogus} → rc=2 + 明确拒绝；
  * mosaic baseline {equal, pixel_ivar} → 放行 + 非生产告警（legacy 整数映射的目标）；
  * 未显式给出 --mode：门不介入（既有缺省路径不变）；
  * export 生产输出 {surface_brightness, point_source_flux, visualization} → 门放行；
  * export 非法 token → rc=2 + 拒绝（FZ-P3-MODES）；
  * normalize 不得接受 --mode（命令树白名单 → unknown flag → 2）；
  * phase1|2|3 <op> 与 --config 旧用户面已删除 → unknown command/flag → 2。

本矩阵的负例面必须真红：每个 reject case 断言 rc=2 且 stderr 出现对应冻结节点
（FZ-MODE-DEFERRED / FZ-FIELD-WEIGHTMODE / FZ-MODE-PRODUCTION / FZ-P3-MODES）的
拒绝理由。正例无法在无真实输入时 rc=0（会话继续执行并在输入/科学面上失败），
因此断言「模式门未拒绝 + 已进入门后的会话运行面」，而不是把负例反转成绿。

用法:
  python3 eng/tools/v6/v6_cli_mode_matrix.py --cli-bin <astrocs> [--work <dir>] [--json-out <path>]
缺失二进制 → rc 2（fail-closed，不静默通过）。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys

# ── 冻结路由表副本（与 v6_runtime_contract.h 对齐；此处只刻画「期望」，判定由真实二进制给出）──
REJECT_P2 = ["point_information", "surface_gls", "psfsw_robust", "psf_snr_power",
             "auto", "support_x_snr2", "0", "1", "2", "bogus"]
# FZ-WEIGHT-SINGLE-PATH：phase2 没有可放行的权重口径 token ⇒ 接受集为空。
ACCEPT_P2 = []
BASE_P2 = ["equal", "pixel_ivar"]
REJECT_P3 = ["psf_snr_power", "auto", "support_x_snr2", "bogus"]
ACCEPT_P3 = ["surface_brightness", "point_source_flux", "visualization"]

# reject token → 必须出现在 stderr 中的冻结节点/理由碎片（防止「任何 rc=2」冒充拒绝）。
REJECT_REASON_P2 = {
    "point_information": "FZ-WEIGHT-SINGLE-PATH",
    "surface_gls": "FZ-WEIGHT-SINGLE-PATH",
    "psfsw_robust": "FZ-MODE-RETIRED",
    "psf_snr_power": "FZ-MODE-DEFERRED",
    "auto": "FZ-FIELD-WEIGHTMODE",
    "support_x_snr2": "FZ-FIELD-WEIGHTMODE",
    "0": "legacy integer weight_mode",
    "1": "legacy integer weight_mode",
    "2": "legacy integer weight_mode",
    "bogus": "FZ-WEIGHT-SINGLE-PATH",
}
REJECT_REASON_P3 = {
    "psf_snr_power": "FZ-P3-MODES",
    "auto": "FZ-P3-MODES",
    "support_x_snr2": "FZ-P3-MODES",
    "bogus": "FZ-P3-MODES",
}

# 部分退役：旧 '--config' 驱动的 legacy 整数 weight_mode 分支已无 CLI 可达面。
# 语义仍由活动用例覆盖（见 replacement），此处只登记退役坐标与权威。
RETIRED_CASES = [
    {
        "id": "config-weight-mode-0-reject",
        "old": "phase2 validate --config <cfg_wm0.json>  (config weight_mode=0)",
        "replacement": "mosaic --json <cfg> --mode 0 -y  → rc=2 + FZ-FIELD-WEIGHTMODE",
        "reason": "--config 旗标随 CLI-001 从命令树删除；v6_mode_gate.h 的 "
                  "config.weight_mode 分支（读 p.values['--config']）已无 CLI 可达面。",
        "authority": "ASTROCS_DESIGN §6.2（唯一命令树）；lib/infrastructure/cli/parser.cpp:31-33,39-43（CLI-001）；"
                     "docs/ci/01_CHECKS.md §2.1（部分退役须改注册绑定）",
    },
    {
        "id": "config-weight-mode-2-baseline",
        "old": "phase2 validate --config <cfg_wm2.json>  (config weight_mode=2)",
        "replacement": "mosaic --json <cfg> --mode pixel_ivar -y  → 放行 + baseline 告警",
        "reason": "同上：config 整数 weight_mode 路由在 CLI 面上不可达；baseline 语义由 "
                  "显式 --mode equal/pixel_ivar 用例覆盖。",
        "authority": "ASTROCS_DESIGN §6.2；lib/infrastructure/cli/parser.cpp:31-33,39-43；"
                     "docs/ci/01_CHECKS.md §2.1",
    },
]


def run(binary, args, cwd):
    p = subprocess.run([binary] + args, capture_output=True, text=True, timeout=300, cwd=cwd)
    return p.returncode, (p.stderr or "") + (p.stdout or "")


def _emit(path, doc):
    if not path:
        return
    p = pathlib.Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


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
        _emit(args.json_out, {"schema": "astrocs.v6.cli-mode-matrix/v1",
                              "verdict": "FAIL", "reason": "binary missing", "cases": [],
                              "retired_cases": RETIRED_CASES})
        return 2

    work = pathlib.Path(args.work).resolve()
    work.mkdir(parents=True, exist_ok=True)
    out_dir = work / "out"
    out_dir.mkdir(parents=True, exist_ok=True)

    # 平铺会话配置（parser.cpp validate_config_full 的 flat_session 面）：
    # mosaic = hips_paths 数组 + output_dir；export = source.hips_dir + output_dir；
    # normalize = input_lights 数组 + output_dir。
    cfg_p2 = work / "cfg_p2.json"
    cfg_p2.write_text(json.dumps({
        "hips_paths": [str(work / "x.hips")], "output_dir": str(out_dir)}), encoding="utf-8")
    cfg_p3 = work / "cfg_p3.json"
    cfg_p3.write_text(json.dumps({
        "source": {"hips_dir": str(work / "x.hips")}, "output_dir": str(out_dir)}),
        encoding="utf-8")
    cfg_p1 = work / "cfg_p1.json"
    cfg_p1.write_text(json.dumps({
        "input_lights": [str(work / "x.fits")], "output_dir": str(out_dir)}), encoding="utf-8")
    # 旧 config-int 形态（已不可达；仅用于证明旧旗标被删）。
    cfg_wm0 = work / "cfg_wm0.json"
    cfg_wm0.write_text(json.dumps({
        "schema_version": "1", "weight_mode": 0,
        "hips_paths": [str(work / "x.hips")], "output_dir": str(out_dir)}), encoding="utf-8")

    cases = []
    fails = 0

    def check(cid, group, expect, cond, rc, err):
        nonlocal fails
        got = "rc=%d err=%s" % (rc, err.strip()[:220])
        cases.append({"id": cid, "group": group, "expect": expect, "ok": bool(cond),
                      "rc": rc, "got": got})
        if not cond:
            fails += 1
            print("  [FAIL] %s (%s): %s" % (cid, expect, got))
        else:
            print("  [ok] %s (%s) rc=%d" % (cid, expect, rc))

    p2_base = ["mosaic", "--json", str(cfg_p2)]
    p3_base = ["export", "--json", str(cfg_p3)]

    # 1) phase2 拒绝面（显式 --mode）：rc=2 + 冻结拒绝理由
    for tok in REJECT_P2:
        rc, err = run(binary, p2_base + ["--mode", tok, "-y"], ".")
        check("phase2-reject-" + tok, "phase2-mode-token", "reject",
              rc == 2 and "--mode rejected:" in err and REJECT_REASON_P2[tok] in err, rc, err)

    # 2) phase2 未显式给模式：门不介入（既有缺省路径不变），进入会话运行面
    rc, err = run(binary, p2_base + ["-y"], ".")
    check("phase2-default-absent-mode", "phase2-mode-token", "admit",
          "--mode rejected:" not in err and
          ("session run: budget" in err or "phase2 failed:" in err or "phase2 complete" in err),
          rc, err)

    # 3) phase2 生产/baseline 放行面：门未拒绝 + 已进会话运行面；baseline 另须告警
    for tok in ACCEPT_P2:
        rc, err = run(binary, p2_base + ["--mode", tok, "-y"], ".")
        check("phase2-accept-" + tok, "phase2-mode-token", "admit",
              "--mode rejected:" not in err and
              ("session run: budget" in err or "phase2 failed:" in err or "phase2 complete" in err),
              rc, err)
    for tok in BASE_P2:
        rc, err = run(binary, p2_base + ["--mode", tok, "-y"], ".")
        check("phase2-baseline-" + tok, "phase2-mode-token", "admit",
              "--mode rejected:" not in err and
              ("WARNING --mode=%s is a baseline" % tok) in err and
              ("session run: budget" in err or "phase2 failed:" in err),
              rc, err)

    # 4) phase3 拒绝面：rc=2 + FZ-P3-MODES
    for tok in REJECT_P3:
        rc, err = run(binary, p3_base + ["--export-mode", tok, "-y"], ".")
        check("phase3-reject-" + tok, "phase3-mode-token", "reject",
              rc == 2 and "--export-mode rejected:" in err and REJECT_REASON_P3[tok] in err, rc, err)

    # 5) phase3 生产输出面：门未拒绝 + 已进会话运行面（无真实输入 → 门后失败）
    for tok in ACCEPT_P3:
        rc, err = run(binary, p3_base + ["--export-mode", tok, "-y"], ".")
        check("phase3-accept-" + tok, "phase3-mode-token", "admit",
              "--export-mode rejected:" not in err and
              ("phase3 failed:" in err or "phase3 complete" in err), rc, err)

    # 6) normalize 不得接受 --mode（命令树白名单 → unknown flag → 2）
    rc, err = run(binary, ["normalize", "--json", str(cfg_p1), "--mode", "point_information", "-y"], ".")
    check("phase1-no-mode-flag", "phase1-command-tree", "reject",
          rc == 2 and "unknown flag '--mode'" in err, rc, err)

    # 7) 旧用户命令面已删除：phaseN <op> / --config → unknown command/flag → 2
    #    （CLI-001 唯一命令树的回归锚，防 phase1|2|3 复活）
    for phase, cfg in (("phase2", cfg_p2), ("phase1", cfg_p1)):
        rc, err = run(binary, [phase, "validate", "--config", str(cfg), "--mode", "point_information"], ".")
        check("legacy-%s-validate-removed" % phase, "legacy-surface-removed", "reject",
              rc == 2 and ("unknown command '%s validate'" % phase) in err, rc, err)
    rc, err = run(binary, ["mosaic", "--json", str(cfg_p2), "--config", str(cfg_wm0), "-y"], ".")
    check("legacy-config-flag-removed", "legacy-surface-removed", "reject",
          rc == 2 and "unknown flag '--config'" in err, rc, err)

    rejects = sum(1 for c in cases if c["expect"] == "reject")
    admits = sum(1 for c in cases if c["expect"] == "admit")
    assert rejects > 0 and admits > 0, "matrix must exercise both reject and admit surfaces"

    for r in RETIRED_CASES:
        print("  [retired] %s -> %s (%s)" % (r["id"], r["replacement"], r["authority"]))

    verdict = "PASS" if fails == 0 else "FAIL"
    _emit(args.json_out, {
        "schema": "astrocs.v6.cli-mode-matrix/v1",
        "cli_bin": str(binary),
        "cli_tree": "normalize|mosaic|export (--json <cfg>|--template|--help) + help + "
                    "--version + doctor + benchmark (ASTROCS_DESIGN §6.2)",
        "cases": cases,
        "counts": {"total": len(cases), "reject": rejects, "admit": admits,
                   "retired": len(RETIRED_CASES)},
        "retired_cases": RETIRED_CASES,
        "verdict": verdict,
        "failures": fails,
    })
    if fails == 0:
        print("V6_CLI_MODE_MATRIX_PASS cases=%d reject=%d admit=%d retired=%d"
              % (len(cases), rejects, admits, len(RETIRED_CASES)))
        return 0
    print("V6_CLI_MODE_MATRIX_FAIL failures=%d/%d" % (fails, len(cases)), file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
