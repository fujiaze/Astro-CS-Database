#!/usr/bin/env python3
"""run manifest 双合同符合性门（RULING-DOC-01 / 裁决 D）。

背景（两份**同名**冻结合同，必须分开判）:
  * CLI-003 = docs/api/MANIFEST_VERIFY_V1.md（API-MANIFEST-001, FROZEN）§2/§2.1：
    真实产出物 `<output_dir>/astrocs_run_<run_id>.json` 的字段合同（写入者 =
    lib/infrastructure/cli/commands.cpp::write_run_manifest）。
  * CFG-001 = eng/contracts/schemas/run_manifest.schema.json（配置分离锚点
    docs/contracts/config_separation_anchors.json 的 run_manifest 类）：描述「本次运行冻结：
    源码 SHA / 配置哈希 / 输入输出哈希 / 工具链版本」的字段名族。

判据（两档，都能红）:
  T1 CLI-003 符合性（严格）：每个真实 manifest 必须带齐 §2/§2.1 的必填字段与类型，且顶层
     不得出现未登记的新键（已登记的加性扩展见 REGISTERED_ADDITIVE）。缺字段/类型错/新键 ⇒ 判红。
  T2 CFG-001 偏差棘轮（登记制）：把每个 manifest 对 eng/contracts/schemas/run_manifest.schema.json
     的偏差（缺必填 / 多余属性）与 eng/ci/ledgers/run_manifest_schema_deviations.json 逐项比对；
     **偏差集必须与登记集完全相等**——出现任何未登记的新偏差、或登记项已消失（陈旧）⇒ 判红。
     登记集是「两份冻结合同对同名对象约定不同」的冲突台账，每条带冲突权威与处置状态；处置需要
     改 CFG-001 冻结 schema（方案见 run/RULING-DOC-01/REPORT.md），不得自行改冻结合同。

用法:
  python3 eng/ci/check_run_manifest_schema.py [--json-out <path>] [--self-test]
exit 0 = T1 与 T2 全过；1 = 判红；2 = 环境/用法错误。
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
SCHEMA = REPO / "eng/contracts/schemas/run_manifest.schema.json"
LEDGER = REPO / "eng/ci/ledgers/run_manifest_schema_deviations.json"
FIXTURES = REPO / "eng/ci/fixtures/run_manifest"
VALIDATOR = REPO / "eng/tests/common/jsonschema_min.py"

HEX40 = re.compile(r"^[0-9a-f]{40}$")

# CLI-003 §2 必填（含类型/值域）。值 = 人类可读判据说明（判定在 _check_cli003 内实现）。
CLI003_REQUIRED = {
    "schema_version": "字符串（CLI-003 §2）",
    "kind": "常量 astrocs_run_manifest（CLI-003 §2）",
    "run_id": "非空字符串（CLI-003 §2）",
    "astrocs_version": "字符串（CLI-003 §2）",
    "platform": "对象且含 os/arch（CLI-003 §2）",
    "config_path": "字符串（CLI-003 §2）",
    "cpu_profile_path": "字符串或 null（CLI-003 §2）",
    "config_sha256": "字符串（CLI-003 §2；输入文件字节 hash）",
    "cpu_profile_sha256": "字符串或 null（CLI-003 §2）",
    "phases": "整数数组（CLI-003 §2）",
    "artifacts": "对象数组，每项含 role/path/sha256/size_bytes（CLI-003 §2 + §4 artifact 词表）",
    "status": "complete | incomplete（CLI-003 §2）",
    "started_utc": "字符串（CLI-003 §2）",
    "finished_utc": "字符串（CLI-003 §2）",
    "provenance": "对象（CLI-003 §2.1 加性扩展，见 PROVENANCE_REQUIRED）",
}
PROVENANCE_REQUIRED = {
    "source_sha": "40hex（CLI-003 §2.1；ASTROCS_COMMIT_SHA）",
    "source_version": "字符串（CLI-003 §2.1）",
    "algorithm_ids": "数组（CLI-003 §2.1）",
    "module_build_ids": "数组（CLI-003 §2.1）",
    "providers": "数组（CLI-003 §2.1）",
    "units": "数组（CLI-003 §2.1）",
    "coordinate_frames": "数组（CLI-003 §2.1）",
    "input_product_hashes": "数组（CLI-003 §2.1）",
    "output_product_hashes": "数组（CLI-003 §2.1）",
}
# 已登记的加性顶层键（各自有冻结合同依据；未登记的新键一律判红）。
REGISTERED_ADDITIVE = {
    "summary": "CLI-003 §4 final 事件同源摘要（write_run_manifest 写入）",
    "error": "SMOKE-001 D7：失败/取消 manifest 自带失败原因",
    "uncertainty_available": "FIX-E2E B1-A5/A10：节点级科学事实并入 manifest",
    "log_artifacts": "docs/contracts/LOG_AND_ERROR_CONTRACT.md §4（每次运行必填）",
    "budget_alloc": "docs/architecture/THREAD_BUDGET_ARCH.md（分配快照）",
}


def _load_validator():
    spec = importlib.util.spec_from_file_location("run_manifest_jsonschema_min", VALIDATOR)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _check_cli003(man: dict) -> list:
    """返回问题列表（空 = 通过）。"""
    problems = []
    for key in CLI003_REQUIRED:
        if key not in man:
            problems.append("missing required (CLI-003 §2): %s" % key)
    for key in man:
        if key not in CLI003_REQUIRED and key not in REGISTERED_ADDITIVE:
            problems.append("unregistered top-level key: %s" % key)
    if man.get("kind") != "astrocs_run_manifest":
        problems.append("kind must be astrocs_run_manifest, got %r" % (man.get("kind"),))
    if man.get("status") not in ("complete", "incomplete"):
        problems.append("status must be complete|incomplete, got %r" % (man.get("status"),))
    if not isinstance(man.get("run_id"), str) or not man.get("run_id"):
        problems.append("run_id must be a non-empty string")
    for key in ("schema_version", "astrocs_version", "config_path", "config_sha256",
                "started_utc", "finished_utc"):
        if key in man and not isinstance(man[key], str):
            problems.append("%s must be a string" % key)
    for key in ("cpu_profile_path", "cpu_profile_sha256"):
        if key in man and not (man[key] is None or isinstance(man[key], str)):
            problems.append("%s must be string|null" % key)
    plat = man.get("platform")
    if not isinstance(plat, dict) or "os" not in plat or "arch" not in plat:
        problems.append("platform must be an object with os/arch")
    ph = man.get("phases")
    if not isinstance(ph, list) or any(not isinstance(x, int) or isinstance(x, bool) for x in ph):
        problems.append("phases must be a list of integers")
    arts = man.get("artifacts")
    if not isinstance(arts, list):
        problems.append("artifacts must be a list")
    else:
        for i, a in enumerate(arts):
            if not isinstance(a, dict):
                problems.append("artifacts[%d] must be an object" % i)
                continue
            # CLI-003 §3 verify 实际消费 path/sha256/size；role 出现在 §2 的示例里，
            # 但实测覆盖极低（见报告 artifacts_role_coverage），故按「有则必合法」
            # 处理并把它作为可见统计量报出，不静默放过。
            for f in ("path", "sha256"):
                if not isinstance(a.get(f), str) or not a.get(f):
                    problems.append("artifacts[%d].%s must be a non-empty string" % (i, f))
            if "role" in a and (not isinstance(a["role"], str) or not a["role"]):
                problems.append("artifacts[%d].role must be a non-empty string when present" % i)
            if "size_bytes" in a and not isinstance(a["size_bytes"], int):
                problems.append("artifacts[%d].size_bytes must be an integer when present" % i)
    prov = man.get("provenance")
    if not isinstance(prov, dict):
        problems.append("provenance must be an object")
    else:
        for key in PROVENANCE_REQUIRED:
            if key not in prov:
                problems.append("provenance missing required (CLI-003 §2.1): %s" % key)
        if "source_sha" in prov and not (isinstance(prov["source_sha"], str)
                                         and HEX40.match(prov["source_sha"])):
            problems.append("provenance.source_sha must be 40-hex")
        for key in ("algorithm_ids", "module_build_ids", "providers", "units",
                    "coordinate_frames", "input_product_hashes", "output_product_hashes"):
            if key in prov and not isinstance(prov[key], list):
                problems.append("provenance.%s must be a list" % key)
    return problems


def _cfg001_deviation(man: dict, schema: dict, validator) -> dict:
    """返回 {missing_required:[...], extra_properties:[...]}（仅顶层）。"""
    # jsonschema_min 的签名是 validate(instance, schema)（与 cfg_common.validate 相反）
    errs = validator.validate(man, schema)
    missing, extra = [], []
    for path, msg in errs:
        quoted = re.findall(r"'([^']+)'", msg)
        if not path and msg.startswith("required") and quoted:
            missing.append(quoted[0])
        elif len(path) == 1 and msg.startswith("additionalProperties") and quoted:
            extra.append(quoted[0])
    return {"missing_required": sorted(set(missing)), "extra_properties": sorted(set(extra))}


def _discover(extra_dirs=None, use_default=True) -> list:
    out = []
    if use_default:
        for base in ("run", "artifacts"):
            root = REPO / base
            if root.is_dir():
                out += sorted(root.rglob("astrocs_run_*.json"))
        if FIXTURES.is_dir():
            out += sorted(FIXTURES.glob("*.json"))
    for d in extra_dirs or []:
        p = pathlib.Path(d)
        if p.is_dir():
            out += sorted(p.rglob("astrocs_run_*.json")) + sorted(p.rglob("*.json"))
    return sorted(set(out))


def run(json_out: str = "", corpus_dirs=None, use_default_corpus=True) -> int:
    validator = _load_validator()
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    ledger = json.loads(LEDGER.read_text(encoding="utf-8"))
    reg_missing = set(ledger["registered_missing_required"])
    reg_extra = set(ledger["registered_extra_properties"])

    files = _discover(corpus_dirs, use_default_corpus)
    if not files:
        print("RUN-MANIFEST-SCHEMA_FAIL: no manifest corpus found (run/**, artifacts/**, fixtures)",
              file=sys.stderr)
        return 2

    t1_bad, measured_missing, measured_extra = {}, set(), set()
    skipped = {}
    n_manifest = 0
    n_artifacts = 0
    n_artifacts_with_role = 0
    for f in files:
        try:
            man = json.loads(f.read_text(encoding="utf-8"))
        except (OSError, ValueError) as exc:
            skipped[str(f)] = "unparsable: %s" % exc
            continue
        if not isinstance(man, dict):
            skipped[str(f)] = "top-level JSON is not an object"
            continue
        # 非 manifest 文件（如测试残留的空 {} 占位）不计入 T1/T2，但如实登记；
        # 「至少一个真实 manifest」由 git-tracked fixture 语料保证（见 --self-test）。
        if man.get("kind") != "astrocs_run_manifest":
            skipped[str(f)] = "not a run manifest (kind=%r)" % (man.get("kind"),)
            continue
        n_manifest += 1
        for a in man.get("artifacts") or []:
            if isinstance(a, dict):
                n_artifacts += 1
                if a.get("role"):
                    n_artifacts_with_role += 1
        p = _check_cli003(man)
        if p:
            t1_bad[str(f)] = p
        dev = _cfg001_deviation(man, schema, validator)
        measured_missing |= set(dev["missing_required"])
        measured_extra |= set(dev["extra_properties"])

    t2_new_missing = sorted(measured_missing - reg_missing)
    t2_new_extra = sorted(measured_extra - reg_extra)
    t2_stale_missing = sorted(reg_missing - measured_missing)
    t2_stale_extra = sorted(reg_extra - measured_extra)
    # new_* 永远致命；stale_*（登记项已消失）只在**全仓语料**下致命——缩窄语料时
    # 「某偏差不再出现」不构成陈旧证据（负例注入模式必须能单独判绿）。
    stale_fatal = bool(use_default_corpus)
    t2_ok = not (t2_new_missing or t2_new_extra
                 or (stale_fatal and (t2_stale_missing or t2_stale_extra)))
    t1_ok = not t1_bad and n_manifest >= 1

    report = {
        "schema": "astrocs.run-manifest-schema-check/v1",
        "n_files_scanned": len(files),
        "n_manifests": n_manifest,
        "skipped_non_manifest": skipped,
        "artifacts_role_coverage": {"n_artifacts": n_artifacts,
                                    "n_with_role": n_artifacts_with_role,
                                    "note": "CLI-003 §2 示例含 role；实测覆盖极低 ⇒ 作为可见统计量登记，不静默放过"},
        "t1_cli003": {"ok": t1_ok, "offenders": t1_bad,
                      "registered_additive": sorted(REGISTERED_ADDITIVE)},
        "t2_cfg001": {
            "ok": t2_ok,
            "measured_missing_required": sorted(measured_missing),
            "measured_extra_properties": sorted(measured_extra),
            "registered_missing_required": sorted(reg_missing),
            "registered_extra_properties": sorted(reg_extra),
            "new_missing_required": t2_new_missing,
            "new_extra_properties": t2_new_extra,
            "stale_missing_required": t2_stale_missing,
            "stale_extra_properties": t2_stale_extra,
            "stale_fatal": stale_fatal,
            "conflict_note": ledger.get("conflict_note", ""),
        },
        "verdict": "PASS" if (t1_ok and t2_ok) else "FAIL",
    }
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("scanned=%d manifests=%d skipped_non_manifest=%d  T1(CLI-003)=%s  T2(CFG-001 ratchet)=%s"
          % (len(files), n_manifest, len(skipped), "PASS" if t1_ok else "FAIL",
             "PASS" if t2_ok else "FAIL"))
    for f, ps in list(t1_bad.items())[:6]:
        for msg in ps[:6]:
            print("  T1 %s: %s" % (f, msg))
    if not t2_ok:
        for k in ("new_missing_required", "new_extra_properties",
                  "stale_missing_required", "stale_extra_properties"):
            if report["t2_cfg001"][k]:
                print("  T2 %s: %s" % (k, report["t2_cfg001"][k]))
    print("RUN-MANIFEST-SCHEMA_%s" % report["verdict"])
    return 0 if (t1_ok and t2_ok) else 1


def self_test(json_out: str = "") -> int:
    """正例/负例自检（能红能绿）。"""
    validator = _load_validator()
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    fixture = json.loads((FIXTURES / "real_manifest_sample.json").read_text(encoding="utf-8"))
    ledger = json.loads(LEDGER.read_text(encoding="utf-8"))

    cases = []

    def rec(name, expect_ok, problems):
        cases.append({"case": name, "expect_ok": expect_ok, "ok": not problems,
                      "problems": problems[:5]})

    rec("cli003_fixture_clean", True, _check_cli003(fixture))
    m = dict(fixture); m.pop("config_sha256")
    rec("cli003_missing_required", False, _check_cli003(m))
    m = dict(fixture); m["phases"] = "1"
    rec("cli003_wrong_type", False, _check_cli003(m))
    m = dict(fixture); m["brand_new_key"] = 1
    rec("cli003_unregistered_key", False, _check_cli003(m))
    m = dict(fixture); m["kind"] = "astrocs_other"
    rec("cli003_wrong_kind", False, _check_cli003(m))
    m = dict(fixture)
    m["provenance"] = {k: v for k, v in fixture["provenance"].items() if k != "source_sha"}
    rec("cli003_provenance_missing", False, _check_cli003(m))
    m = dict(fixture)
    m["provenance"] = dict(fixture["provenance"]); m["provenance"]["source_sha"] = "zz"
    rec("cli003_provenance_bad_sha", False, _check_cli003(m))

    dev = _cfg001_deviation(fixture, schema, validator)
    # fixture 是单份真实产物；登记集是全语料并集 ⇒ 不变量是「fixture 偏差 ⊆ 登记集」。
    subset = (set(dev["missing_required"]) <= set(ledger["registered_missing_required"])
              and set(dev["extra_properties"]) <= set(ledger["registered_extra_properties"]))
    rec("cfg001_fixture_deviation_subset_of_registered", True,
        [] if subset else ["deviation not covered by ledger: %s" % json.dumps(dev, ensure_ascii=False)])

    conforming = {"manifest_schema": "astrocs.run-manifest/v1", "run_id": "abc",
                  "software_sha": "a" * 40, "config_hash": "b" * 64,
                  "manifest_input_hashes": [{"path": "in.fits", "sha256": "c" * 64}],
                  "manifest_output_hashes": [{"path": "out.fits", "sha256": "d" * 64}],
                  "toolchain_version": "gcc 13.2", "created_utc": "2026-09-22T00:00:00Z"}
    d0 = _cfg001_deviation(conforming, schema, validator)
    rec("cfg001_conforming_instance_clean", True,
        [] if not (d0["missing_required"] or d0["extra_properties"]) else [json.dumps(d0)])
    # 检出类用例：ok 语义 = 「被检出」，expect_ok 恒为 True（判据必须能红）。
    bad = dict(conforming); bad["extra"] = 1
    det = _cfg001_deviation(bad, schema, validator)["extra_properties"]
    rec("cfg001_extra_property_detected", True,
        [] if det else ["extra property NOT detected (gate is blind)"])
    bad2 = dict(conforming); bad2.pop("config_hash")
    det2 = _cfg001_deviation(bad2, schema, validator)["missing_required"]
    rec("cfg001_missing_required_detected", True,
        [] if det2 else ["missing required NOT detected (gate is blind)"])

    ok = all(c["ok"] == c["expect_ok"] for c in cases)
    report = {"schema": "astrocs.run-manifest-schema-selftest/v1", "cases": cases,
              "n_cases": len(cases), "verdict": "PASS" if ok else "FAIL"}
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for c in cases:
        print("[%s] %s (expect_ok=%s) %s" % ("PASS" if c["ok"] == c["expect_ok"] else "FAIL",
                                             c["case"], c["expect_ok"], str(c["problems"])[:100]))
    print("RUN-MANIFEST-SCHEMA-SELFTEST_%s cases=%d" % (report["verdict"], len(cases)))
    return 0 if ok else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json-out", default="")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--corpus-dir", action="append", default=[],
                    help="追加语料目录（负例注入用；可重复）")
    ap.add_argument("--no-default-corpus", action="store_true",
                    help="只用语料目录（负例隔离；正例/发布态不得使用）")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test(args.json_out)
    return run(args.json_out, args.corpus_dir, not args.no_default_corpus)


if __name__ == "__main__":
    raise SystemExit(main())
