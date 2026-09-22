#!/usr/bin/env python3
"""RUNTIME-CI-001 CLI 模式路由矩阵（真实二进制端到端；CLI-001 命令树对齐版）。

对已构建的 astrocs 二进制跑冻结模式路由矩阵。命令面 = ASTROCS_DESIGN §7.1 唯一
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
（FZ-MODE-RETIRED / FZ-MODE-DEFERRED / FZ-FIELD-WEIGHTMODE / FZ-WEIGHT-SINGLE-PATH /
FZ-P3-MODES）的拒绝理由。正例无法在无真实输入时 rc=0（会话继续执行并在输入/科学面上
失败），因此断言「模式门未拒绝 + 已进入门后的会话运行面」，而不是把负例反转成绿。

## 规范依据（本矩阵守的是「CLI 面不存在权重模式选择机制」这条裁决面）

  * ASTROCS_DESIGN.md §3.1：「全程只有 SNR，没有『权重模式』这个概念」——权重是
    Phase2 集成时按天球像素对应的输入帧集合**现场算出的派生量**；Phase1/Phase3
    不产生、不消费权重。
  * ASTROCS_DESIGN.md §7.1（唯一命令树）：公开面只有
    mosaic --json <config.json> / --template / --help；--mode / --export-mode 是
    **不写进 help 的内部旗标**（command_tree.h value_flags()），其唯一职责是
    fail-closed 拒绝，不是提供选择。
  * docs/science/PSF_SIGNAL_WEIGHT.md §4（单一权重口径，无模式选择）逐字：
    「实现面对任何口径 token 一律 fail-closed 显式拒绝并给出迁移提示，不得静默接受…
    **没有任何 token 属于合法口径**（CLI --mode 面对 phase2 全部 fail-closed）」，
    拒绝理由分别引用 FZ-MODE-RETIRED / FZ-MODE-DEFERRED / FZ-FIELD-WEIGHTMODE /
    FZ-WEIGHT-SINGLE-PATH。故「显式拒绝」是**收紧后的要求**：把 --mode 降级成
    unknown flag 会丢掉迁移提示，反而不满足该条。
  * docs/science/CONTROL_WEIGHT_SNR.md §1/§2a：stage2 的 local_snr / frame_snr 是
    **相对质量权重场**（quality_weight），不是科学信噪比，更不是可选口径；科学 SNR
    由逐源 σ_F（帧级基准 m_5）定义。
  * eng/ci/check_no_weight_mode_code.py（CHK-NO-WEIGHT-MODE-CODE，FZ-WEIGHT-SINGLE-PATH）
    C1「模式选择机制不存在」+ C2「退役对象的**拒绝面**必须存活（收紧不是删除）」，
    并逐字点名 route_phase2_weight_token 是合法名字（只做 fail-closed 判定与 baseline
    登记）。本矩阵是该代码门在**真实二进制**上的运行面佐证。
  * docs/algorithms/PLATESOLVE.md（相邻口径复核）：全文无 mode / 权重模式条款
    ⇒ 不构成第二权威，不改变上述判定。

## 驱动面：为什么每条运行用例都带 -force（ASTROCS_DESIGN.md §4.5）

本矩阵的配置**故意**指向不存在的 x.hips（不依赖任何真实数据即可判定模式门）。
§4.5 运行前预检：「🔴 error：文件找不到、路径错误… error 阻塞运行…**存在 error 时
-y / -yes 不能越过**；**-force 跳过整个检查步骤直接运行**，后果由用户承担」。
⇒ 只带 -y 时进程在**预检**就返回 3（输入缺失），永远到不了模式门
（cmd_session2_run 第一行的 v6cli::mode_gate），矩阵会把「门没被触达」误报成
「模式门失效」（2026-09-22 V6-CLI-MODE-MATRIX 现场 20/24 即此形态：全部 rc=3 +
hips_paths[0] 指向的路径不存在）。
-force 是规范明文的总开关：跳过预检，**不跳过模式门** ⇒ 门被真实触达；放行用例
随后在会话/科学面自然失败（rc≠0），断言只要求「门未拒绝 + 已进会话运行面」。

用法:
  python3 eng/tools/v6/v6_cli_mode_matrix.py --cli-bin <astrocs> [--work <dir>] [--json-out <path>]
  python3 eng/tools/v6/v6_cli_mode_matrix.py --cli-bin <astrocs> --self-test
      # 正例/负例自检：真实二进制 ⇒ 必绿；注入一条「权重模式选择」分支 ⇒ 必红
缺失二进制 → rc 2（fail-closed，不静默通过）。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys

# ── 驱动开关（ASTROCS_DESIGN.md §4.5）────────────────────────────────────────
# §4.5 明文：「存在 error 时 -y / -yes 不能越过；-force 跳过整个检查步骤直接运行」。
# 本矩阵的配置故意指向不存在的输入 ⇒ 预检必出 error ⇒ 不带 -force 时进程在预检就
# 返回 3，永远到不了 v6cli::mode_gate（2026-09-22 现场 20/24 全是这个形态）。
# -force 跳过预检但**不跳过模式门** ⇒ 被测面被真实触达。
FORCE = ["-force"]

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
        # 订正（2026-09-23）：原写「--mode 0 -y → rc=2」不成立 —— §4.5 下 -y 越不过
        # 预检 error（现场 rc=3）。可执行的替代命令必须带 -force 才真正走到模式门。
        "replacement": "mosaic --json <cfg> --mode 0 -force -y  → rc=2 + FZ-FIELD-WEIGHTMODE",
        "reason": "--config 旗标随 CLI-001 从命令树删除；v6_mode_gate.h 的 "
                  "config.weight_mode 分支（读 p.values['--config']）已无 CLI 可达面。",
        "authority": "ASTROCS_DESIGN §3.1/§4.5/§7.1；docs/science/PSF_SIGNAL_WEIGHT.md §4；"
                     "lib/infrastructure/cli/parser.cpp:31-33,39-43（CLI-001）；"
                     "docs/ci/01_CHECKS.md §2.1（部分退役须改注册绑定）",
    },
    {
        "id": "config-weight-mode-2-baseline",
        "old": "phase2 validate --config <cfg_wm2.json>  (config weight_mode=2)",
        "replacement": "mosaic --json <cfg> --mode pixel_ivar -force -y  → 放行 + baseline 告警",
        "reason": "同上：config 整数 weight_mode 路由在 CLI 面上不可达；baseline 语义由 "
                  "显式 --mode equal/pixel_ivar 用例覆盖。",
        "authority": "ASTROCS_DESIGN §3.1/§4.5/§7.1；docs/science/PSF_SIGNAL_WEIGHT.md §4；"
                     "lib/infrastructure/cli/parser.cpp:31-33,39-43；"
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


def run_matrix(binary, work, json_out=""):
    """对 binary 跑一遍冻结模式路由矩阵；返回 (rc, doc)。rc=0 全绿、1 有红、2 前置缺失。"""
    binary = pathlib.Path(binary).resolve()
    work = pathlib.Path(work).resolve()
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

    # §4.5: 全部运行用例带 -force（跳过预检、直达模式门）；见文件头「驱动面」节。
    p2_base = ["mosaic", "--json", str(cfg_p2), *FORCE]
    p3_base = ["export", "--json", str(cfg_p3), *FORCE]

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
    doc = {
        "schema": "astrocs.v6.cli-mode-matrix/v1",
        "cli_bin": str(binary),
        "cli_tree": "normalize|mosaic|export (--json <cfg>|--template|--help) + help + "
                    "--version + doctor + benchmark (ASTROCS_DESIGN §7.1)",
        "driver_flags": list(FORCE),
        "cases": cases,
        "counts": {"total": len(cases), "reject": rejects, "admit": admits,
                   "retired": len(RETIRED_CASES)},
        "retired_cases": RETIRED_CASES,
        "verdict": verdict,
        "failures": fails,
    }
    _emit(json_out, doc)
    if fails == 0:
        print("V6_CLI_MODE_MATRIX_PASS cases=%d reject=%d admit=%d retired=%d"
              % (len(cases), rejects, admits, len(RETIRED_CASES)))
        return 0, doc
    print("V6_CLI_MODE_MATRIX_FAIL failures=%d/%d" % (fails, len(cases)), file=sys.stderr)
    return 1, doc


# ── --self-test：证明本矩阵**能绿也能红**（ENGINEERING_SPEC §8 / AGENTS §9）──────
# 负例注入面 = 「权重模式选择」这一层本身：把真实二进制对 mosaic --mode 0 的
# fail-closed 拒绝（FZ-FIELD-WEIGHTMODE）换成「静默接受、按 baseline 口径放行」——
# 即凭空长出一条可选择的口径。判据若真有鉴别力，phase2-reject-0 必须转红，
# 且**只**它转红（无连带、无静默）。
_INJECTED_WRAPPER = '''#!/usr/bin/env python3
"""V6-CLI-MODE-MATRIX --self-test 负例：注入一条「权重模式选择」分支。

真实二进制对 mosaic ... --mode 0 是 fail-closed 拒绝（FZ-FIELD-WEIGHTMODE，rc=2）。
本包装器把该 token 改写成 pixel_ivar —— 于是「0」从必须拒绝的非法 token 变成一条
**可选择的口径**（负责人裁决要禁掉的那一层）。其余调用原样转发真实二进制。
"""
import subprocess
import sys

REAL = %r
args = sys.argv[1:]
if args and args[0] == "mosaic" and "--mode" in args:
    i = args.index("--mode")
    if i + 1 < len(args) and args[i + 1] == "0":
        args = args[:i + 1] + ["pixel_ivar"] + args[i + 2:]
sys.exit(subprocess.call([REAL] + args))
'''


def self_test(binary, work):
    """① 真实二进制 ⇒ 必绿；② 注入模式选择分支 ⇒ 必红且恰 phase2-reject-0 红。"""
    binary = pathlib.Path(binary).resolve()
    work = pathlib.Path(work).resolve() / "selftest"
    work.mkdir(parents=True, exist_ok=True)

    print("[self-test] 正例：真实二进制 %s" % binary)
    rc_pos, doc_pos = run_matrix(binary, work / "pos", str(work / "pos.json"))

    wrapper = work / "injected_cli.py"
    wrapper.write_text(_INJECTED_WRAPPER % str(binary), encoding="utf-8")
    wrapper.chmod(0o755)
    print("[self-test] 负例：注入模式选择分支（--mode 0 变成可选择的口径）")
    rc_neg, doc_neg = run_matrix(wrapper, work / "neg", str(work / "neg.json"))

    failed = [c["id"] for c in doc_neg.get("cases", []) if not c.get("ok")]
    ok = (rc_pos == 0 and doc_pos.get("verdict") == "PASS"
          and rc_neg != 0 and failed == ["phase2-reject-0"])
    _emit(str(work / "selftest.json"), {
        "schema": "astrocs.v6.cli-mode-matrix-selftest/v1",
        "positive": {"verdict": doc_pos.get("verdict"), "rc": rc_pos},
        "negative": {"verdict": doc_neg.get("verdict"), "rc": rc_neg,
                     "failed_cases": failed},
        "verdict": "PASS" if ok else "FAIL",
    })
    print("V6_CLI_MODE_MATRIX_SELFTEST_%s 正例=%s 负例=%s 负例失败用例=%s"
          % ("PASS" if ok else "FAIL", doc_pos.get("verdict"),
             doc_neg.get("verdict"), failed))
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--cli-bin", required=True)
    ap.add_argument("--work", default="run/v6/RUNTIME-CI-001/cli-matrix")
    ap.add_argument("--json-out", default="")
    ap.add_argument("--self-test", action="store_true",
                    help="正例/负例自检：注入一条模式选择分支必须判红")
    args = ap.parse_args(argv)
    binary = pathlib.Path(args.cli_bin).resolve()
    if not binary.exists():
        print("V6_CLI_MATRIX_FAIL(prerequisite): astrocs binary missing: %s -> fail-closed"
              % binary, file=sys.stderr)
        _emit(args.json_out, {"schema": "astrocs.v6.cli-mode-matrix/v1",
                              "verdict": "FAIL", "reason": "binary missing", "cases": [],
                              "retired_cases": RETIRED_CASES})
        return 2
    if args.self_test:
        return self_test(binary, args.work)
    rc, _doc = run_matrix(binary, args.work, args.json_out)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
