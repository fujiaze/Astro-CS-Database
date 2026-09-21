#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-IMPACT-MAP | eng/ci/impact_map.json 判据一致性门（changed-path -> checks 映射）。

权威依据
  - ENGINEERING_SPEC.md §8「机器一致性检查」：每项检查有正例与负例（能红能绿）、
    **可执行负例面**（--self-test 或 --fault-inject）、**fail-closed**（输入缺失/
    路径不存在/依赖不可用必须判红，不得崩溃后静默通过）、**锚存活**（硬编码引用的
    文件/目录必须存在）、**注册表双向一致**；
  - eng/ci/impact_map.json 自述契约（spec_ref = tasks/02_CI_TASKS.md :: V8-CI-004）：
    「所有引用 id 均存在于 eng/ci/checks.json 且为 fast profile 候选，由
    eng/ci/tests/test_impact_map.py 机器校验」。

为什么另立本门（W4-A3 根因）
  旧 eng/ci/tests/test_impact_map.py 只按**顶层 id** 索引注册表（`c["id"] for c in
  registry["checks"]`），而注册表的执行单元自 CI-001 ID 收敛后是**两层**结构：
  顶层聚合项（CHK-* 等 42 项）+ 其 steps[].id（138 项旧 ID 原样保留）。
  impact_map 引用的是 step id ⇒ 4 条既有红全部来自「step id 与顶层 id 混用」这一个根因。
  本门把该口径**显式登记为判据**，并补上原测试没有的可执行负例面。

判据（任一 R 违规 ⇒ exit 1）
  R1 id 闭包      impact_map 的 base_checks/rules[].checks/fallback 引用的每个 id
                  必须存在于注册表**顶层 ∪ steps**（两层都算登记）；
  R2 fast 候选    上项每个 id 必须落在 fast profile 候选面内（step 未显式声明
                  profiles 时继承父项）；
  R3 BASE 核心    每条规则必须携带 fallback 的核心集（BASE 十三类的领域无关映射）；
  R4 路径域覆盖   PROBE_DOMAINS 声明的每个**现行**路径域：(a) 其锚路径在当前树中必须
                  真实存在（锚存活，失效即判红）；(b) 其 glob 探针必须被至少一条规则的
                  paths 命中（用 eng/ci/run.py 的 _match_prefix 语义，与生产选择器同源）；
  R5 无退役引用   impact_map 不得引用已退役/RESERVED id（docs/ci/01_CHECKS.md §2.1/§2.3）；
  R6 结构完整     rules 非空、每规则 paths/checks 非空、无重复 paths 集合。

可执行负例面（--self-test，全部在内存 fixture 上跑，零副作用）
  N0 正例 ⇒ PASS
  N1 引用不存在 id ⇒ R1；N2 引用非 fast id ⇒ R2；N3 规则缺 BASE 核心 ⇒ R3；
  N4 声明的路径域无规则覆盖 ⇒ R4；N5 引用已退役 id ⇒ R5；N6 规则 paths 为空 ⇒ R6；
  N7 注册表/映射不存在 ⇒ fail-closed exit 2（不 traceback、不静默通过）

用法
  python3 eng/tools/quality/check_impact_map.py                  # 校验（CI 检查面）
  python3 eng/tools/quality/check_impact_map.py --json-out F     # 同时落证据 JSON
  python3 eng/tools/quality/check_impact_map.py --self-test      # 负例自检（N0..N7）
exit 0 = PASS；1 = 判据违规；2 = 输入不可用（fail-closed）。

只读；仅 stdlib。
"""
from __future__ import annotations

import argparse
import datetime as _dt
import importlib.util
import json
import pathlib
import re
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[3]
REGISTRY_REL = "eng/ci/checks.json"
MAP_REL = "eng/ci/impact_map.json"
DOC_REL = "docs/ci/01_CHECKS.md"
RUNNER_REL = "eng/ci/run.py"

# R4 探针面：(路径域, 锚存活路径, glob 探针)。
# W4-A3 按 ARCH-001 / ROOT-008 迁移后的**真实布局**重锚：迁移前的 schemas/ evidence/
# modules/ runtime/ graph/ launch/ AstroCS.wiki/ 在现行根清单中不存在，不得再作判据锚。
# 锚存活（ENGINEERING_SPEC §8）= ANCHORS 第 2 列必须真实存在，否则判红（不得静默跳过）。
PROBE_DOMAINS = [
    ("VERSION", "VERSION", "VERSION"),
    ("CMakeLists.txt", "CMakeLists.txt", "CMakeLists.txt"),
    ("CMakePresets.json", "CMakePresets.json", "CMakePresets.json"),
    ("eng/cmake/**", "eng/cmake/install_layout.cmake", "eng/cmake/install_layout.cmake"),
    ("eng/ci/**", "eng/ci/run.py", "eng/ci/run.py"),
    ("docs/**", "docs/DOCUMENT_INDEX.yaml", "docs/VERSIONING.md"),
    ("docs/contracts/**", "docs/contracts", "docs/contracts/DATA_SEMANTICS.md"),
    ("docs/science/**", "docs/science", "docs/science/ASTROMETRY.md"),
    ("lib/**", "lib/algorithms/psf/src/dpsf_psf.cpp", "lib/algorithms/psf/src/dpsf_psf.cpp"),
    ("lib/infrastructure/**", "lib/infrastructure/cli/main.cpp",
     "lib/infrastructure/cli/main.cpp"),
    ("lib/infrastructure/observability/**", "lib/infrastructure/observability",
     "lib/infrastructure/observability/PENDING.md"),
    ("lib/include/**", "lib/include/astrocs/common_abi_v1.h", "lib/include/astrocs/common_abi_v1.h"),
    ("eng/contracts/**", "eng/contracts/schemas/run_manifest.schema.json", "eng/contracts/schemas/run_manifest.schema.json"),
    ("third_party/**", "lib/third_party/nlohmann", "lib/third_party/nlohmann/json.hpp"),
    ("eng/tests/**", "eng/tests/testkit/registry.json", "eng/tests/testkit/registry.json"),
    ("testdata/**", "testdata/index.json", "testdata/index.json"),
    ("eng/tools/**", "eng/tools/quality/check_module_map.py", "eng/tools/quality/check_module_map.py"),
    ("eng/tools/quality/**", "eng/tools/quality/check_module_map.py",
     "eng/tools/quality/check_module_map.py"),
    ("eng/tools/monitoring/**", "eng/tools/monitoring/run_monitored.py",
     "eng/tools/monitoring/run_monitored.py"),
    ("eng/packaging/**", "packaging", "eng/packaging/astrocs.product.json"),
    (".github/**", ".github/workflows/ci-linux.yml", ".github/workflows/ci-linux.yml"),
    ("artifacts/**", "artifacts/ci", "artifacts/ci/run.json"),
    ("reports/**", "reports", "reports/README.md"),
    ("工程控制/**", "工程控制", "工程控制/PROJECT-GOVERNANCE-01/OPEN_ITEMS.md"),
    ("AGENTS.md", "AGENTS.md", "AGENTS.md"),
    ("memory.md", "memory.md", "memory.md"),
    ("README.md", "README.md", "README.md"),
]


def _utc_now() -> str:
    return _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# ------------------------------------------------------------------ 注册表索引 ----
def index_registry(registry: dict) -> dict:
    """把两层注册表摊平成统一索引（W4-A3 根因：step id 与顶层 id 混用）。

    返回 {top, steps, fast, retired}：fast 为顶层 ∪ step 的 fast 候选集合，
    step 未显式声明 profiles 时继承父项（与 eng/ci/run_checks.py 的继承语义一致）。
    """
    top: set[str] = set()
    steps: set[str] = set()
    fast: set[str] = set()
    for entry in registry.get("checks", []) or []:
        cid = entry.get("id")
        if not isinstance(cid, str) or not cid:
            continue
        top.add(cid)
        parent_fast = "fast" in (entry.get("profiles") or [])
        if parent_fast:
            fast.add(cid)
        for step in entry.get("steps", []) or []:
            sid = step.get("id")
            if not isinstance(sid, str) or not sid:
                continue
            steps.add(sid)
            step_profiles = step.get("profiles")
            if ("fast" in step_profiles) if step_profiles else parent_fast:
                fast.add(sid)
    return {"top": top, "steps": steps, "fast": fast, "retired": set()}


def registry_ids(idx: dict) -> set:
    return idx["top"] | idx["steps"]


_ID_TOKEN_RE = re.compile(r"`([A-Za-z0-9][A-Za-z0-9_.\-]*)`")


def parse_retired(doc_text: str) -> set:
    """从 docs/ci/01_CHECKS.md §2.1（退役）与 §2.3（RESERVED）抽取 id。

    两表的语义都是「不得出现在注册表 / 映射面」。表格一格可能登记**多个** id
    （如 §2.1 的 `WORKSPACE-ADOPTION` / `RECONCILE-STATE`），故逐格取全部
    登记形态词元，而不是只看首格或首个反引号串。
    """
    ids: set[str] = set()
    for start_marker, end_marker in (("### 2.1", "### 2.2"), ("### 2.3", "\n---")):
        i = doc_text.find(start_marker)
        if i < 0:
            continue
        j = doc_text.find(end_marker, i)
        seg = doc_text[i:(j if j > i else len(doc_text))]
        for line in seg.splitlines():
            s = line.strip()
            if not s.startswith("|"):   # 只取表格数据行（正文/引用块不算登记）
                continue
            cells = [c.strip() for c in s.strip("|").split("|")]
            if not cells or set(cells[0]) <= set("-: "):
                continue
            if cells[0].lower() in ("退役项", "reserved项", "reserved"):
                continue
            # §2.1/§2.3 的「退役项 / RESERVED 项」列 = 本表登记的判据对象；
            # 逐格取全部 id 形态词元（一格可登记多个，如 WORKSPACE-ADOPTION / RECONCILE-STATE），
            # 但排除文件路径形态（含 /）——检查器文件名不是注册项 id。
            # 只取「退役项 / RESERVED 项」列中**括号前**的主登记词元：括号内是
            # 归属注记（父聚合项、检查器文件名、保留项），不是本表登记的判据对象
            # （例如 \`WORKSPACE-ADOPTION\` 行的括号里写着父项 \`CHK-ENV-ADOPTION\`，
            # 后者仍在用，若一并收进退役集会反过来咬活动注册项）。
            head = cells[0].split("（")[0].split("(")[0]
            for tok in re.findall(r"[A-Z][A-Za-z0-9]*(?:-[A-Za-z0-9]+)+", head):
                if "/" not in tok:
                    ids.add(tok)
    return ids


# ------------------------------------------------------------------- 选择器语义 ----
def _load_match_prefix(repo: pathlib.Path):
    """复用 eng/ci/run.py 的 _match_prefix（与生产选择器同源，避免两套 glob 语义）。"""
    spec = importlib.util.spec_from_file_location("astrocs_ci_run_impactgate",
                                                 repo / RUNNER_REL)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod._match_prefix


def _map_ids(impact: dict) -> set:
    out: set = set(impact.get("base_checks", []) or [])
    out |= set(impact.get("fallback", []) or [])
    for rule in impact.get("rules", []) or []:
        out |= set(rule.get("checks", []) or [])
    return out


# ------------------------------------------------------------------------- 判据 ----
def evaluate(impact: dict, registry: dict, *, match_prefix, root: pathlib.Path,
             retired=None, probe_domains=None) -> dict:
    idx = index_registry(registry)
    idx["retired"] = set(retired or ())
    known = registry_ids(idx)
    findings: list = []

    def bad(rule: str, detail: str) -> None:
        findings.append({"rule": rule, "severity": "P1", "detail": detail})

    # R6 结构完整（先判；后续规则依赖结构）
    rules = impact.get("rules")
    if not isinstance(rules, list) or not rules:
        bad("R6_structure", "rules 必须是非空数组")
        rules = []
    for i, rule in enumerate(rules):
        if not isinstance(rule, dict):
            bad("R6_structure", "rules[%d] 不是对象" % i)
            continue
        if not rule.get("paths"):
            bad("R6_structure", "rules[%d].paths 为空" % i)
        if not rule.get("checks"):
            bad("R6_structure", "rules[%d].checks 为空" % i)
    seen_pathsets: dict = {}
    for i, rule in enumerate(rules):
        if not isinstance(rule, dict):
            continue
        key = tuple(sorted(rule.get("paths") or []))
        if key in seen_pathsets:
            bad("R6_structure", "rules[%d] 与 rules[%d] 的 paths 集合重复"
                % (seen_pathsets[key], i))
        seen_pathsets[key] = i

    # R1 id 闭包
    unknown = sorted(_map_ids(impact) - known)
    if unknown:
        bad("R1_id_closure",
            "引用注册表不存在的 id（顶层 ∪ steps 两层都算登记）：%s" % unknown)

    # R2 fast 候选
    non_fast = sorted(_map_ids(impact) - idx["fast"])
    if non_fast:
        bad("R2_fast_candidates", "引用非 fast 候选 id（违反 prefer fast）：%s" % non_fast)

    # R3 BASE 核心
    fallback = set(impact.get("fallback", []) or [])
    base = fallback - {"WORKSPACE-ADOPTION", "RECONCILE-STATE"}
    for i, rule in enumerate(rules):
        if not isinstance(rule, dict):
            continue
        missing = sorted(base - set(rule.get("checks", []) or []))
        if missing:
            bad("R3_base_core", "rules[%d]（%s…）缺 BASE 核心类：%s"
                % (i, (rule.get("paths") or ["?"])[:2], missing))

    # R4 路径域覆盖（锚存活 + glob 探针命中）
    probes = PROBE_DOMAINS if probe_domains is None else probe_domains
    for item in probes:
        domain, anchor, probe = item
        if not (root / anchor).exists():
            bad("R4_probe_domain",
                "锚存活失效：%s 的锚 %s 在当前树不存在" % (domain, anchor))
            continue
        hit = any(match_prefix(probe, pat)
                  for rule in rules if isinstance(rule, dict)
                  for pat in (rule.get("paths") or []))
        if not hit:
            bad("R4_probe_domain",
                "路径域未被任何规则覆盖：%s（探针 %s）" % (domain, probe))

    # R5 无退役引用
    refs_retired = sorted(_map_ids(impact) & idx["retired"])
    if refs_retired:
        bad("R5_no_retired_ref", "引用已退役/RESERVED id：%s" % refs_retired)

    verdict = "IMPACT_MAP_PASS" if not findings else "IMPACT_MAP_FAIL"
    return {
        "tool": "eng/tools/quality/check_impact_map.py",
        "rule": "CHK-IMPACT-MAP / V8-CI-004 (eng/ci/impact_map.json 判据一致性)",
        "generated_utc": _utc_now(),
        "registry": REGISTRY_REL,
        "impact_map": MAP_REL,
        "registry_top_ids": len(idx["top"]),
        "registry_step_ids": len(idx["steps"]),
        "registry_fast_ids": len(idx["fast"]),
        "mapped_ids": len(_map_ids(impact)),
        "rules": len(rules),
        "probe_domains": len(list(probes)),
        "probe_anchors": [{"domain": d, "anchor": a, "probe": p}
                          for d, a, p in (PROBE_DOMAINS if probe_domains is None
                                          else probe_domains)],
        "findings": findings,
        "error_count": len(findings),
        "verdict": verdict,
    }


# --------------------------------------------------------------- 可执行负例面 ----
def _fixture_registry() -> dict:
    return {"schema_version": 1, "checks": [
        {"id": "VERSION-CONSISTENCY", "profiles": ["fast"], "platform": "any",
         "command": ["true"], "timeout_seconds": 1, "heavy": False,
         "mutates_workspace": False, "outputs": [], "waivable": False},
        {"id": "CHK-AGG", "profiles": ["fast"], "platform": "any",
         "command": ["true"], "timeout_seconds": 1, "heavy": False,
         "mutates_workspace": False, "outputs": [], "waivable": False,
         "steps": [{"id": "CON-COMMENTS", "command": ["true"], "timeout_seconds": 1}]},
        {"id": "SLOW-ONLY", "profiles": ["linux-main"], "platform": "any",
         "command": ["true"], "timeout_seconds": 1, "heavy": False,
         "mutates_workspace": False, "outputs": [], "waivable": False},
        {"id": "RETIRED-ONE", "profiles": ["fast"], "platform": "any",
         "command": ["true"], "timeout_seconds": 1, "heavy": False,
         "mutates_workspace": False, "outputs": [], "waivable": False},
    ]}


def _fixture_map(**over) -> dict:
    m = {
        "rules": [{"paths": ["src/**"],
                   "checks": ["VERSION-CONSISTENCY", "CON-COMMENTS"]}],
        "fallback": ["VERSION-CONSISTENCY", "CON-COMMENTS"],
    }
    m.update(over)
    return m


def _mp(probe: str, pat: str) -> bool:
    """自测用的最小 _match_prefix（与 eng/ci/run.py 同语义：dir/** 前缀命中）。"""
    if pat.endswith("/**"):
        base = pat[:-3].rstrip("/")
        return probe == base or probe.startswith(base + "/")
    return probe == pat


def _run_case(reg, impact, *, retired=(),
              probes=(("src/**", "src", "src/a.cpp"),)) -> dict:
    with tempfile.TemporaryDirectory(prefix="impact-gate-") as td:
        root = pathlib.Path(td)
        (root / "src").mkdir(parents=True, exist_ok=True)
        (root / "src" / "a.cpp").write_text("x\n", encoding="utf-8")
        return evaluate(impact, reg, match_prefix=_mp, root=root,
                        retired=set(retired), probe_domains=list(probes))


def self_test() -> int:
    reg = _fixture_registry()
    cases = [
        ("N0 正例", _run_case(reg, _fixture_map()), 0, ""),
        ("N1 引用不存在 id",
         _run_case(reg, _fixture_map(rules=[{
             "paths": ["src/**"],
             "checks": ["VERSION-CONSISTENCY", "CON-COMMENTS", "NO-SUCH-ID"]}])),
         1, "R1_id_closure"),
        ("N2 引用非 fast id",
         _run_case(reg, _fixture_map(rules=[{
             "paths": ["src/**"],
             "checks": ["VERSION-CONSISTENCY", "CON-COMMENTS", "SLOW-ONLY"]}])),
         1, "R2_fast_candidates"),
        ("N3 规则缺 BASE 核心",
         _run_case(reg, _fixture_map(rules=[{
             "paths": ["src/**"], "checks": ["VERSION-CONSISTENCY"]}])),
         1, "R3_base_core"),
        ("N4 声明路径域无规则覆盖",
         _run_case(reg, _fixture_map(),
                   probes=(("src/**", "src", "src/a.cpp"),
                           ("docs/**", "src", "docs/x.md"))),
         1, "R4_probe_domain"),
        ("N5 引用已退役 id",
         _run_case(reg, _fixture_map(rules=[{
             "paths": ["src/**"],
             "checks": ["VERSION-CONSISTENCY", "CON-COMMENTS", "RETIRED-ONE"]}]),
                   retired=("RETIRED-ONE",)),
         1, "R5_no_retired_ref"),
        ("N6 规则 paths 为空",
         _run_case(reg, _fixture_map(rules=[{
             "paths": [], "checks": ["CON-COMMENTS"]}])),
         1, "R6_structure"),
    ]

    ok = True
    for name, res, want_rc, want_rule in cases:
        codes = [f["rule"] for f in res["findings"]]
        rc = 0 if res["verdict"] == "IMPACT_MAP_PASS" else 1
        good = (rc == want_rc) and ((want_rc == 0) or (want_rule in codes))
        ok = ok and good
        print("[selftest] %-30s rc=%-2d want=%-2d %s"
              % (name, rc, want_rc, "OK" if good else "MISMATCH"))
        if not good:
            print("[selftest]   命中规则=%s 期望含 %s" % (codes, want_rule))

    with tempfile.TemporaryDirectory(prefix="impact-gate-fc-") as td:
        rc = main(["--root", td])
        good = rc == 2
        ok = ok and good
        print("[selftest] %-30s rc=%-2d want=%-2d %s"
              % ("N7 输入缺失 fail-closed", rc, 2, "OK" if good else "MISMATCH"))
    print("[selftest] %d cases, %s" % (len(cases) + 1, "ALL OK" if ok else "FAILED"))
    return 0 if ok else 1


# ------------------------------------------------------------------------- main ----
def _refuse(code: str, detail: str) -> int:
    """fail-closed：输入不可用即 exit 2 并点名，不 traceback、不静默通过。"""
    sys.stderr.write("IMPACT_MAP_INPUT_UNAVAILABLE: %s %s\n" % (code, detail))
    print(json.dumps({"tool": "eng/tools/quality/check_impact_map.py",
                      "verdict": "IMPACT_MAP_INPUT_UNAVAILABLE",
                      "code": code, "detail": detail},
                     ensure_ascii=False, indent=2))
    return 2


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-IMPACT-MAP 判据一致性门")
    ap.add_argument("--root", default=str(REPO))
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()

    root = pathlib.Path(args.root).resolve()
    reg_path, map_path, runner = root / REGISTRY_REL, root / MAP_REL, root / RUNNER_REL
    if not reg_path.is_file():
        return _refuse("registry_missing", str(reg_path))
    if not map_path.is_file():
        return _refuse("impact_map_missing", str(map_path))
    if not runner.is_file():
        return _refuse("runner_missing", str(runner))
    try:
        registry = json.loads(reg_path.read_text(encoding="utf-8"))
        impact = json.loads(map_path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001 - 解析失败即输入不可用
        return _refuse("parse_error", "%s: %s" % (map_path, exc))
    if not isinstance(registry.get("checks"), list) or not registry["checks"]:
        return _refuse("registry_empty", str(reg_path))
    if not isinstance(impact.get("rules"), list):
        return _refuse("impact_map_bad_shape", str(map_path))
    try:
        match_prefix = _load_match_prefix(root)
    except Exception as exc:  # noqa: BLE001
        return _refuse("runner_load_error", "%s: %s" % (runner, exc))

    doc_path = root / DOC_REL
    retired = (parse_retired(doc_path.read_text(encoding="utf-8"))
               if doc_path.is_file() else set())

    res = evaluate(impact, registry, match_prefix=match_prefix, root=root,
                   retired=retired)
    payload = json.dumps(res, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        out = pathlib.Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(payload + "\n", encoding="utf-8")
    print(payload)
    return 0 if res["verdict"] == "IMPACT_MAP_PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
