#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CI-001 门极性 + GAP-027 证据harness（临时目录 fixture，不改仓库）。

产物：
  run/PROJECT-GOVERNANCE-01/CI-001/evidence/<GATE>/{positive,negative}.log
  ci/polarity_evidence.json
  run/PROJECT-GOVERNANCE-01/CI-001/evidence/gap027_probe.log
证据种类（诚实标注，不冒充）：
  selftest                —— 检查器自带 --selftest（内含正/负例）
  unittest-suite          —— 现有单测套件（内含正例+负例断言）
  missing-dependency      —— 依赖物缺失 → 必须 rc≠0（fail-closed 负例）
  mutation                —— 注入缺陷 → rc≠0
  not_proven              —— 本机无法证明（附原因，不放假绿）
"""
import json, os, pathlib, subprocess, sys, tempfile

REPO = pathlib.Path(".").resolve()
EV = REPO / "run/PROJECT-GOVERNANCE-01/CI-001/evidence"
EV.mkdir(parents=True, exist_ok=True)
PY = sys.executable

# gate -> (grade, positive cmd, positive kind, negative cmd | None, negative kind, note)
PAIRS = [
    ("CHK-MODULE-MANIFEST", "P0",
     [PY, "tools/quality/check_module_map.py", "--selftest"], "selftest",
     lambda tmp: [PY, "tools/quality/check_module_map.py", "--repo-root", tmp, "--quiet"],
     "missing-dependency", "检查器自带 selftest（正例）；空仓缺 MODULE_MAP.yaml → 必须红"),
    ("CHK-ROOT-CLEAN", "P0",
     [PY, "-m", "pytest", "tests/quality/test_root_cleanliness.py", "-q"], "unittest-suite",
     lambda tmp: [PY, "tools/quality/check_root_cleanliness.py", "--root", tmp,
                  "--manifest", "ci/root_manifest.json", "--quiet"],
     "missing-dependency", "套件含 clean-tree 正例与 3 个负例；空树缺必需条目 → 必须红"),
    ("API-DOCS", "P0",
     [PY, "-m", "pytest", "tests/quality/test_doc_machine_check.py", "-q"], "unittest-suite",
     lambda tmp: [PY, "tools/check_api_docs.py", "--repo", tmp],
     "mutation", "GAP-027 直接负例：--repo 非完整检出（无 lib/）→ 必须红（原为静默回落 rc=0）"),
    ("CHK-SCHEMA", "P0",
     [PY, "tools/monitoring/check_log_contract.py", "--selfcheck"], "selftest",
     lambda tmp: [PY, "tools/quality/check_task_result_schema.py", "--results-dir", tmp],
     "missing-dependency", "GAP-027 直接负例：TASK_RESULT 零命中 → 必须红（原为 PASS）"),
    ("CHK-STATIC", "P1",
     [PY, "tools/quality/check_prod_reachability.py", "--selftest"], "selftest",
     lambda tmp: [PY, "tools/quality/check_complexity.py", "--paths",
                  str(pathlib.Path(tmp) / "nope1") + "," + str(pathlib.Path(tmp) / "nope2"),
                  "--output", str(pathlib.Path(tmp) / "cx.json")],
     "missing-dependency", "GAP-027 直接负例：--paths 全缺 → 必须红（原为 rc=0 空转绿）"),
    ("CHK-SCI-REF", "P0",
     [PY, "tools/quality/check_traceability.py"], "real-repo",
     lambda tmp: [PY, "tools/check_unit_closure.py"],  # real-repo green; neg below
     "not_proven", "正例=真仓 TRACEABILITY rc=0；负例见 SCI-REF-NEG（临时树删单位节）"),
    ("CHK-DANGLING", "P1",
     [PY, "tools/doccheck/check_doc_index.py", "--strict"], "real-repo",
     lambda tmp: [PY, "tools/doccheck/check_doc_index.py", "--root", tmp, "--strict"],
     "missing-dependency", "真仓 rc=0；空树缺 docs/INDEX.md → 必须红"),
    ("CHK-RESOURCE", "P0",
     [PY, "tools/quality/check_serial_heavy.py", "--selftest"], "selftest", None,
     "not_proven", "selftest 含负例；ctest 类资源门需构建树，本机不重跑"),
    ("CHK-ISA-EQ", "P1",
     [PY, "tools/quality/check_isa_leak.py", "--selftest"], "selftest", None,
     "not_proven", "selftest 纯内存正负例；ISA 超差注入需重建 provider"),
    ("CHK-ENV-ADOPTION", "P1",
     [PY, "ci/verify_toolchain.py"], "real-repo", None,
     "not_proven", "真仓 rc=0（上次 fast 运行）；负例需伪造 toolchain.lock 漂移"),
    ("CHK-SECRET-HYGIENE", "P0",
     [PY, "tools/quality/check_secret_hygiene.py", "--scope", "tracked", "--json-out",
      "run/ci/secret-hygiene/polarity.json"], "real-repo", None,
     "not_proven", "ROOT-006 已给真仓与注入证据（其自测 22 项）；本波不重复"),
    ("CHK-KNOWN-FAILURES-BASELINE", "P1",
     [PY, "tools/quality/known_failures_baseline.py", "--mode", "selftest"], "selftest", None,
     "not_proven", "selftest S11/S12 断言 missing_results_fails_closed（审计确认）"),
]
NOT_PROVEN = {
    "CHK-BUILD-LINUX": "需真构建（2400s 级）+ monitor；负例需注入编译错误后重建",
    "CHK-BUILD-WIN": "platform=windows：本机 SKIPPED(platform)，无 MSVC",
    "CHK-FMT": "未注册：宿主缺 clang-format（RESERVED）",
    "CHK-DUAL-TOL": "未注册：需 Windows 侧候选产物（RESERVED）",
    "CHK-AGENT-HARD-RULES": "未注册：实现者 AGENTS-GOV 按裁决冻结（RETIRE-PENDING-GOV-001）",
    "CHK-WARN": "负例需无构建树环境；真仓运行会 touch 源文件强制重编（本波不跑）",
    "CHK-CONTRACT-REF": "负例可做（临时树 INDEX.yaml 悬空）但本轮预算未覆盖",
    "CHK-CONTRACT-TEST": "负例需临时树 TRACEABILITY.csv 悬空；本轮预算未覆盖",
    "CHK-ORACLE": "需 ctest 目标 + 构建树；负例目标存在但本轮不重跑",
    "CHK-INVARIANT": "同上",
    "CHK-ABI": "ABI-BOUNDARY 无 --repo（硬锚），负例需改判据支持；ctest 类同 ORACLE",
    "CHK-SYNTH-P1": "ctest 合成链，需构建树",
    "CHK-SYNTH-P2": "同上（含 v6 CLI 模式矩阵，需构建产物 astrocs）",
    "CHK-SYNTH-P3": "同上；v6_p3_rsmp_mutation_driver 目标当前 build/ 缺",
    "CHK-NWORKER": "需运行时确定性驱动 + 多 worker 注入",
    "CHK-SANITIZER": "需 clang sanitizer 重建（2400s 级）",
    "CHK-COVERAGE": "P2 报告项：无阈值/无红判据（审计建议另立）",
    "CHK-PACKAGE": "platform=windows SKIPPED；Linux 打包链未注册",
    "CHK-UNIT": "unittest/ctest 全量；负例矩阵已由 ci/tests 负例套件承担（本轮未重跑）",
}


def run(cmd, timeout=900):
    try:
        p = subprocess.run([str(c) for c in cmd], cwd=str(REPO), capture_output=True,
                           text=True, timeout=timeout)
        return p.returncode, (p.stdout or "") + (p.stderr or "")
    except subprocess.TimeoutExpired:
        return 124, "TIMEOUT after %ss" % timeout
    except OSError as exc:
        return 127, "OSError: %s" % exc


def main() -> int:
    index = {"schema_version": 1, "task": "CI-001",
             "generated_from_registry_sha256": __import__("hashlib").sha256(
                 (REPO / "ci/checks.json").read_bytes()).hexdigest(),
             "evidence_root": "run/PROJECT-GOVERNANCE-01/CI-001/evidence",
             "gates": {}, "not_proven": NOT_PROVEN}
    probe_log = []
    with tempfile.TemporaryDirectory() as td:
        tmp = pathlib.Path(td)
        empty = tmp / "empty-repo"
        empty.mkdir()
        rel_empty = os.path.relpath(empty, REPO)
        for gate, grade, pos, pkind, negfn, nkind, note in PAIRS:
            d = EV / gate
            d.mkdir(parents=True, exist_ok=True)
            prc, pout = run(pos)
            (d / "positive.log").write_text("$ %s\nrc=%s\n%s" % (" ".join(map(str, pos)), prc, pout[-4000:]), encoding="utf-8")
            entry = {"grade": grade, "positive": {"cmd": [str(c) for c in pos], "kind": pkind,
                                                  "rc": prc, "log": str(d / "positive.log")},
                     "note": note}
            if nkind == "not_proven" or negfn is None:
                entry["negative"] = {"kind": "not_proven", "reason": note}
            else:
                ncmd = negfn(rel_empty)
                nrc, nout = run(ncmd)
                (d / "negative.log").write_text("$ %s\nrc=%s\n%s" % (" ".join(map(str, ncmd)), nrc, nout[-4000:]), encoding="utf-8")
                entry["negative"] = {"cmd": [str(c) for c in ncmd], "kind": nkind, "rc": nrc,
                                     "log": str(d / "negative.log")}
                entry["polarity_ok"] = (prc == 0 and nrc != 0)
            index["gates"][gate] = entry
            probe_log.append("%-28s pos rc=%s (%s) neg rc=%s (%s)" % (
                gate, prc, pkind, entry.get("negative", {}).get("rc", "-"), entry["negative"]["kind"]))
    ok = sum(1 for g in index["gates"].values() if g.get("polarity_ok"))
    index["summary"] = {"gates_total": len(index["gates"]) + len(NOT_PROVEN),
                        "gates_with_local_polarity_proof": ok,
                        "gates_not_proven": len(NOT_PROVEN)}
    (REPO / "ci/polarity_evidence.json").write_text(
        json.dumps(index, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (EV / "gap027_probe.log").write_text("\n".join(probe_log) + "\n", encoding="utf-8")
    print("\n".join(probe_log))
    print("polarity_proof=%d not_proven=%d" % (ok, len(NOT_PROVEN)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
