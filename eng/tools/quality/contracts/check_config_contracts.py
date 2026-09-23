#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_config_contracts.py — T403 config contracts checker（§9.73 A44 后判据反转）

权威依据
  - ASTROCS_DESIGN.md §3.1:171「全程只有 SNR，没有"权重模式"这个概念」；
  - ASTROCS_DESIGN.md §3.1:175「权重的产生链固定为两步、**没有可选择项**」；
  - docs/science/PSF_SIGNAL_WEIGHT.md §4:72「**没有可选择的口径**：不存在口径选择键、
    口径枚举、口径配置项或口径产物」；
  - 工程控制 GAP_AUDIT §9.73 裁决 A44（负责人 2026-09-20）；
  - ENGINEERING_SPEC.md §8（每项检查有正例与负例、能红能绿；fail-closed；锚存活）。

判据（**正向约束**，全部 fail-closed）
  P1  字段面（lib/algorithms/coverage/include/astro/phase2/stage2_common.h，去注释）：
      **裸** weight_mode 标识符出现次数必须为 0 —— P2Stage2Config 不再承载 legacy
      整数权重模式域。sky_plane_weight_mode（天光面 GLS **拟合**权重）与
      snr_weight_mode（UPM 拟合内部诊断）是**另外的对象**，前缀使其不被本判据命中，
      不在本检查域内（归属说明见 docs/contracts/ 与任务回执）。
  P2  token 面（lib/algorithms/coverage/src/stage2_common.cpp，去注释）：
      legacy 取值 token 字面量 "ivar" / "equal" / "support_x_snr2" 出现次数必须为 0；
      legacy 报错串 weight_mode 只支持 必须为 0。
      （"auto" **不**作探针：它仍是 acr_route 的合法缺省值，不是 legacy 权重 token。）
  P3  拒绝面存活（**非退化**判据）：P1/P2 全绿还不够 —— stage2_common.cpp 必须同时含
      in.contains("weight_mode") 与 §9.73，证明该键**出现即 fail-closed 具名拒绝**，
      而不是被静默忽略。缺任一 ⇒ 判红（防「删干净了、也没拒绝面」冒充完成）。
  P4  示例配置：lib/algorithms/coverage/configs/*.json 必须合法 JSON 且含
      inputs/integration；且任一示例**不得**携带 weight_mode 键（死键清零）。
  P5  保留判据：acr_route 的 auto 缺省与 acr_route 只支持 报错串在位。
  P6  fail-closed：锚文件/锚目录缺失 ⇒ rc=2（不得把「文件不存在」当「无违规」）。

用法
  python3 eng/tools/quality/contracts/check_config_contracts.py [--repo .] [--out-json F] [--out-junit F]
  python3 eng/tools/quality/contracts/check_config_contracts.py --self-test   # 临时树正例/负例
Exit: 0 PASS, 1 contract FAIL, 2 env error / fail-closed。
"""
import argparse
import json
import pathlib
import re
import shutil
import sys
import tempfile

HEADER_REL = "lib/algorithms/coverage/include/astro/phase2/stage2_common.h"
PARSER_REL = "lib/algorithms/coverage/src/stage2_common.cpp"
CONFIGS_REL = "lib/algorithms/coverage/configs"

# P1：**裸**标识符（前后都不是 word char）—— sky_plane_weight_mode / snr_weight_mode
# 因前缀是 word char 而不命中，故本判据不会误伤那两个**另外的对象**。
BARE_WEIGHT_MODE_RE = re.compile(r"(?<![\w])weight_mode(?![\w])")
# P2：legacy 取值 token（字符串字面量）与 legacy 报错串。
LEGACY_TOKEN_LITERALS = ('"ivar"', '"equal"', '"support_x_snr2"')
LEGACY_ERROR_STRING = "weight_mode 只支持"
# P3：拒绝面锚（出现即具名 fail-closed）。两类面分开判：
#   * 机制锚必须在**代码面**（去注释）命中 —— 注释里的同名字符串不算实现；
#   * 条款引用锚允许落在注释（冻结条款 id 的规范位置就是注释/文档串）。
REJECT_FACE_CODE_ANCHORS = ('in.contains("weight_mode")',)
REJECT_FACE_RAW_ANCHORS = ("§9.73",)
# P5：保留判据（与本次改动无关，防顺手删）。
KEEP_ANCHORS = ('acr_route", std::string("auto")', "acr_route 只支持")


def strip_comments(text):
    """去掉块注释与行注释，只留代码面（避免把注释里的历史符号名当活代码）。"""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    out = []
    for line in text.splitlines():
        s = line.lstrip()
        if s.startswith("//") or s.startswith("#"):
            continue
        out.append(line)
    return "\n".join(out)


class EnvError(Exception):
    """锚点不可用 ⇒ fail-closed（rc=2）。"""


def read_anchor(repo, rel):
    p = pathlib.Path(repo) / rel
    if not p.is_file():
        raise EnvError("ANCHOR_MISSING: %s" % rel)
    return p.read_text(encoding="utf-8", errors="ignore")


def check_repo(repo):
    """返回 findings 列表（每条含 id/severity/observed/expected）。空 = PASS。"""
    findings = []

    header = strip_comments(read_anchor(repo, HEADER_REL))
    parser_raw = read_anchor(repo, PARSER_REL)
    parser = strip_comments(parser_raw)
    cfg_dir = pathlib.Path(repo) / CONFIGS_REL
    if not cfg_dir.is_dir():
        raise EnvError("ANCHOR_MISSING: %s" % CONFIGS_REL)

    # ── P1 字段面：裸 weight_mode 必须为 0 ────────────────────────────────
    hits = BARE_WEIGHT_MODE_RE.findall(header)
    if hits:
        findings.append({
            "id": "CFG-WEIGHTMODE-FIELD-REVIVED", "severity": "P0",
            "file": HEADER_REL, "observed": "bare weight_mode x%d in code" % len(hits),
            "expected": "0 (no legacy integer weight mode field; §9.73 A44)",
        })

    # ── P2 token 面：legacy 取值 token 与 legacy 报错串必须为 0 ────────────
    for tok in LEGACY_TOKEN_LITERALS:
        if tok in parser:
            findings.append({
                "id": "CFG-WEIGHTMODE-TOKEN-REVIVED", "severity": "P0",
                "file": PARSER_REL, "symbol": tok,
                "observed": "legacy weight token literal present in code",
                "expected": "absent (no selectable weight mode; §9.73 A44)",
            })
    if LEGACY_ERROR_STRING in parser:
        findings.append({
            "id": "CFG-WEIGHTMODE-ERROR-REVIVED", "severity": "P0",
            "file": PARSER_REL, "symbol": LEGACY_ERROR_STRING,
            "observed": "legacy weight_mode error string present in code",
            "expected": "absent (key deleted; §9.73 A44)",
        })

    # ── P3 拒绝面存活（非退化）────────────────────────────────────────────
    for anchor, surface in ([(a, parser) for a in REJECT_FACE_CODE_ANCHORS] +
                            [(a, parser_raw) for a in REJECT_FACE_RAW_ANCHORS]):
        if anchor not in surface:
            findings.append({
                "id": "CFG-WEIGHTMODE-REJECT-FACE-MISSING", "severity": "P0",
                "file": PARSER_REL, "symbol": anchor,
                "observed": "rejection face anchor missing",
                "expected": "present (key present => named fail-closed reject)",
            })

    # ── P5 保留判据 ───────────────────────────────────────────────────────
    for anchor in KEEP_ANCHORS:
        if anchor not in parser:
            findings.append({
                "id": "CFG-KEEP-ANCHOR-MISSING", "severity": "P1",
                "file": PARSER_REL, "symbol": anchor,
                "observed": "anchor missing", "expected": "present",
            })

    # ── P4 示例配置 ───────────────────────────────────────────────────────
    cfgs = sorted(cfg_dir.glob("*.json"))
    if not cfgs:
        raise EnvError("ANCHOR_MISSING: %s/*.json" % CONFIGS_REL)
    for cfg in cfgs:
        rel = cfg.relative_to(pathlib.Path(repo)).as_posix()
        try:
            j = json.loads(cfg.read_text(encoding="utf-8"))
        except Exception as e:
            findings.append({
                "id": "CFG-EXAMPLE-JSON", "severity": "P1", "file": rel,
                "observed": str(e), "expected": "valid JSON",
            })
            continue
        if "inputs" not in j or "integration" not in j:
            findings.append({
                "id": "CFG-EXAMPLE-KEYS", "severity": "P1", "file": rel,
                "observed": "missing inputs/integration", "expected": "both present",
            })
        if isinstance(j.get("integration"), dict) and "weight_mode" in j["integration"]:
            findings.append({
                "id": "CFG-EXAMPLE-DEAD-KEY", "severity": "P0", "file": rel,
                "observed": "integration.weight_mode present in example config",
                "expected": "absent (dead key; §9.73 A44)",
            })
    return findings


def _write_tree(root, header, parser, cfgs):
    root = pathlib.Path(root)
    (root / pathlib.Path(HEADER_REL).parent).mkdir(parents=True, exist_ok=True)
    (root / pathlib.Path(PARSER_REL).parent).mkdir(parents=True, exist_ok=True)
    (root / CONFIGS_REL).mkdir(parents=True, exist_ok=True)
    (root / HEADER_REL).write_text(header, encoding="utf-8")
    (root / PARSER_REL).write_text(parser, encoding="utf-8")
    for name, body in cfgs.items():
        (root / CONFIGS_REL / name).write_text(body, encoding="utf-8")


_GOOD_HEADER = ("struct P2Stage2Config {\n"
                "    int sky_plane_weight_mode = 0;\n"
                "    int snr_weight_mode = 0;\n};\n")
_GOOD_PARSER = (
    'bool p2_stage2_parse_config(const nlohmann::json& j) {\n'
    '    const auto& in = j["integration"];\n'
    '    // §9.73 A44：该键出现即 fail-closed\n'
    '    if (in.contains("weight_mode")) { return false; }\n'
    '    cfg->acr_route = in.value("acr_route", std::string("auto"));\n'
    '    if (cfg->acr_route != "auto") { *err = "acr_route 只支持 auto/cpu"; }\n'
    '}\n')
_GOOD_CFGS = {"stage2_overlap.example.json":
              '{"inputs": {"hips": ["a", "b"]}, "integration": {"precision": "fp32"}}'}


def self_test():
    """能红能绿：正例必绿；legacy 复活（字段/token/报错串）+ 拒绝面缺失 + 死键必红；锚缺失 fail-closed。"""
    cases = []
    tmp = tempfile.mkdtemp(prefix="cfg_contracts_selftest_")
    try:
        def run(name, header, parser, cfgs, want_rc):
            root = pathlib.Path(tmp) / name
            _write_tree(root, header, parser, cfgs)
            try:
                f = check_repo(root)
                rc = 0 if not f else 1
                ids = [x["id"] for x in f]
            except EnvError as e:
                rc, ids = 2, [str(e)]
            cases.append((name, rc, want_rc, ids))

        run("green_clean", _GOOD_HEADER, _GOOD_PARSER, _GOOD_CFGS, 0)
        run("red_field_revived",
            _GOOD_HEADER.replace("    int snr_weight_mode = 0;",
                                 "    int snr_weight_mode = 0;\n    int weight_mode = 2;"),
            _GOOD_PARSER, _GOOD_CFGS, 1)
        run("red_token_revived", _GOOD_HEADER,
            _GOOD_PARSER.replace('if (in.contains("weight_mode")) { return false; }',
                                 'if (in.value("weight_mode", std::string("auto")) == "equal") {}'),
            _GOOD_CFGS, 1)
        run("red_error_string_revived", _GOOD_HEADER,
            _GOOD_PARSER.replace('if (in.contains("weight_mode")) { return false; }',
                                 '*err = "weight_mode 只支持 auto/ivar/equal/support_x_snr2";'),
            _GOOD_CFGS, 1)
        run("red_reject_face_missing", _GOOD_HEADER,
            _GOOD_PARSER.replace('if (in.contains("weight_mode")) { return false; }', ''),
            _GOOD_CFGS, 1)
        run("red_dead_key_in_example", _GOOD_HEADER, _GOOD_PARSER,
            {"stage2_overlap.example.json":
             '{"inputs": {"hips": ["a"]}, "integration": {"weight_mode": "auto"}}'}, 1)
        # fail-closed：锚目录缺失 ⇒ rc=2（不得当"无违规"）
        root = pathlib.Path(tmp) / "failclosed_no_configs"
        _write_tree(root, _GOOD_HEADER, _GOOD_PARSER, {})
        shutil.rmtree(root / CONFIGS_REL)
        try:
            check_repo(root)
            cases.append(("failclosed_missing_configs", 0, 2, []))
        except EnvError as e:
            cases.append(("failclosed_missing_configs", 2, 2, [str(e)]))

        ok = True
        for name, rc, want, ids in cases:
            good = (rc == want)
            ok &= good
            print("SELFTEST_%s %s (rc=%d want=%d) %s"
                  % ("PASS" if good else "FAIL", name, rc, want,
                     "" if good else "ids=%s" % ids))
        print("SELF_TEST %s cases=%d" % ("PASS" if ok else "FAIL", len(cases)))
        return 0 if ok else 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return self_test()

    try:
        findings = check_repo(args.repo)
    except EnvError as e:
        print(json.dumps({"tool": "check_config_contracts", "status": "ENV_ERROR",
                          "findings": [{"id": "ENV", "severity": "P0",
                                        "observed": str(e), "expected": "anchors present"}],
                          "passed": False}, ensure_ascii=False, indent=2))
        return 2

    status = "PASS" if not findings else "FAIL"
    result = {"tool": "check_config_contracts", "status": status,
              "findings": findings, "passed": status == "PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(
            json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0", "P1")])
        junit = ('<testsuite name="check_config_contracts" tests="1" failures="%d">'
                 '<testcase classname="config" name="contracts"/></testsuite>' % failures)
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
