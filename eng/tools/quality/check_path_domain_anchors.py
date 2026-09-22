#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-PATH-DOMAIN-ANCHORS | eng/ci/checks.json 的 changed_paths 路径域锚存活门。

权威依据
  - docs/ci/CI_SPEC.md §2.3（选择规则）："选中 = 候选 ∩ { step | ∃ p ∈
    step.changed_paths：p 与改动集中某路径匹配 }"，并规定 glob 语义（dir/** 命中
    dir 及其任意子孙；含 * ? [ 的模式按 shell glob，* 不跨 /；不含通配符的模式为
    精确路径匹配）；
  - docs/ci/01_CHECKS.md §1「锚存活」：判据里硬编码引用的仓库路径必须存在；失效时
    以 ANCHOR_STALE: <常量名> <路径> 显式失败并点名，不得 traceback、不得静默降级；
    §1「fail-closed」：输入缺失 / 路径不存在 / 依赖不可用时必须判红，不得把
    「文件不存在」当「无违规」（scanned == 0 ⇒ rc != 0）；
  - ENGINEERING_SPEC.md §10（机器一致性检查）：fail-closed、锚存活、
    eng/ci/checks.json 是唯一检查注册表。

为什么另立本门（根因）
  changed_paths 是**路径域**：增量档据此决定"改了哪些文件要跑哪些门"。但注册表
  结构校验器（validate_registry.py R1–R15）只校验 changed_paths 的**类型**，没有
  任何门校验这些 glob 的**基目录是否存在** ⇒ 目录一旦退役（ARCH-001 目录等价迁移、
  ROOT-007/ROOT-008 根目录整合），该路径域静默失效：改动落在继承者路径上不再触发
  该门，而 UNCOVERED_CHANGED_PATHS 也不会报警（失效域指向的路径不存在，改动集里
  根本不会有落在其中的文件）。本门把"路径域失效"从静默降级变成判红。

判据（任一 A 违规 ⇒ exit 1）
  A1 结构       changed_paths 必须是字符串数组，元素非空、非绝对路径、无 .. 穿越；
                违规逐条点名（ANCHOR_STALE 风格），不得静默跳过；
  A2 锚存活     每个路径域的基目录必须在当前树中存在：
                  dir/**            ⇒ 前缀目录 dir 必须存在；
                  含 * ? [ 的模式    ⇒ 最长无通配前缀目录必须存在；
                  不含通配符        ⇒ 该精确路径必须存在；
                失效即判红，并点名 (检查项 id, step id, 路径域, 基目录) 与处置指引
                （可删除的冗余域给出 redundant_with；否则给最近现存祖先）；
  A3 家族前缀预留 合法但"当前不存在"的路径域（例如按需创建的轮次目录家族）必须显式
                登记于 eng/ci/path_domain_reservations.json；登记项自带判据（棘轮，
                只减不增）：
                  - kind 必须是 family_prefix，且 pattern/reason/owner/sample_path 齐备；
                  - pattern 必须仍被注册表引用（不再引用 ⇒ 判红 RESERVATION_UNREFERENCED）；
                  - sample_path 必须不存在（家族已落地 ⇒ 判红 RESERVATION_OBSOLETE，
                    应删除登记、改回普通域）；
                  - sample_path 必须在**生产选择器语义**（eng/ci/incremental.py
                    ::glob_match，与 run_checks.py 同源）下命中该 pattern，否则登记的
                    是"永不命中的预留"（假绿）⇒ 判红 RESERVATION_INEFFECTIVE。
  A4 扫描面     changed_paths 条目数为 0 ⇒ 判红（fail-closed：空扫描不得判绿）。
  A5 注册表     checks.json 缺失 / 不可解析 / checks 非非空数组 ⇒ exit 2 并点名
                （PATH_DOMAIN_INPUT_UNAVAILABLE），不得 traceback、不得静默通过。

设计约束
  - **不内置任何"允许的死路径"白名单**：合法的家族前缀预留只能走 A3 的显式登记，
    且登记项自身受上述棘轮判据约束；
  - glob 语义不另写一份：直接加载 eng/ci/incremental.py::glob_match（生产同源），
    避免第二套路径语义（ENGINEERING_SPEC §10 唯一注册表 + 确定性执行器）。

可执行负例面（--self-test，全部在临时目录 fixture 上跑，零副作用）
  N0 正例 ⇒ PASS；N1 dir/** 基目录缺失 / N2 精确路径缺失 / N3 通配前缀缺失 ⇒ A2；
  N4 已登记家族预留且样例未落地 ⇒ PASS；N5 登记项不再被引用 / N6 样例已落地 /
  N7 样例在选择器语义下不命中 ⇒ A3；N8 结构不符 ⇒ A1；N9 空扫描面 ⇒ A4；
  N10 冗余域给出 redundant_with；N11/N12 注册表缺失/坏 JSON ⇒ fail-closed exit 2。

用法
  python3 eng/tools/quality/check_path_domain_anchors.py                    # 校验
  python3 eng/tools/quality/check_path_domain_anchors.py --json-out F       # 落证据
  python3 eng/tools/quality/check_path_domain_anchors.py --registry F       # 换注册表（预览修复）
  python3 eng/tools/quality/check_path_domain_anchors.py --self-test        # 负例自检
exit 0 = PASS；1 = 判据违规；2 = 输入不可用（fail-closed）。

只读；仅 stdlib。
"""
from __future__ import annotations

import argparse
import datetime as _dt
import importlib.util
import json
import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parents[3]
REGISTRY_REL = "eng/ci/checks.json"
RESERVATIONS_REL = "eng/ci/path_domain_reservations.json"
INCREMENTAL_REL = "eng/ci/incremental.py"

# A3 允许的登记形态。除 family_prefix 外的任何"预留"都不被接受：其余形态要么
# 应当直接指向现存路径（A2），要么应当删除。
ALLOWED_RESERVATION_KINDS = ("family_prefix",)
WILDCARDS = "*?["


def _utc_now() -> str:
    return _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# --------------------------------------------------------------- 路径域语义 ----
def domain_base(pattern: str) -> tuple:
    """把路径域折成 (基目录, 形态)。形态 ∈ {dir_prefix, glob, exact}。

    与 docs/ci/CI_SPEC.md §2.3 的三种语义一一对应：
      dir/**           ⇒ 前缀域，基目录 = 去掉 /** 后的目录；
      含 * ? [         ⇒ shell glob，基目录 = **最长无通配前缀**所在目录；
      不含通配符       ⇒ 精确路径，基目录 = 路径自身。
    基目录为空串表示仓库根（例如 '**' 形态），恒存在。
    """
    p = pattern.strip().replace("\\", "/")
    while p.startswith("./"):
        p = p[2:]
    if p.endswith("/**"):
        return p[:-3].rstrip("/"), "dir_prefix"
    if any(ch in p for ch in WILDCARDS):
        idx = min(p.find(ch) for ch in WILDCARDS if ch in p)
        prefix = p[:idx]
        if "/" in prefix:
            return prefix.rsplit("/", 1)[0], "glob"
        return "", "glob"
    return p, "exact"


def _domain_target(pattern: str, base: str, kind: str) -> str:
    """需要存在的那个路径（判据对象）。"""
    if kind == "exact":
        return pattern.strip().replace("\\", "/")
    return base


def _is_absolute(p: str) -> bool:
    return p.startswith("/") or (len(p) >= 2 and p[1] == ":")


def _has_dotdot(p: str) -> bool:
    return any(seg == ".." for seg in p.split("/"))


# ------------------------------------------------------------------ 注册表 ----
def iter_domains(registry: dict) -> tuple:
    """展开注册表，返回 (rows, struct_errors)。

    row = {"check_id", "step_id", "pattern"}；struct_errors 为逐条点名的结构违规。
    顶层项与 steps 都算：step 未声明 changed_paths 时按 run_checks.py::expand_steps
    的继承语义取父项域（本函数只校验"声明了的"，继承面由父项自身的行覆盖）。
    """
    rows: list = []
    errors: list = []

    def take(check_id: str, step_id: str, where: str, entry: dict) -> None:
        if "changed_paths" not in entry:
            return
        pats = entry.get("changed_paths")
        if not isinstance(pats, list):
            errors.append("ANCHOR_STALE: CHANGED_PATHS_STRUCTURE %s: changed_paths "
                          "必须是数组（实际 %s）" % (where, type(pats).__name__))
            return
        if not pats:
            errors.append("ANCHOR_STALE: CHANGED_PATHS_STRUCTURE %s: changed_paths "
                          "为空数组（空域不得登记）" % where)
            return
        for i, pat in enumerate(pats):
            if not isinstance(pat, str) or not pat.strip():
                errors.append("ANCHOR_STALE: CHANGED_PATHS_STRUCTURE %s[%d]: 路径域必须"
                              "是非空字符串（实际 %r）" % (where, i, pat))
                continue
            if _is_absolute(pat) or _has_dotdot(pat):
                errors.append("ANCHOR_STALE: CHANGED_PATHS_STRUCTURE %s[%d]: 路径域必须是"
                              "仓库相对 POSIX 路径且无 .. 穿越（实际 %r）" % (where, i, pat))
                continue
            rows.append({"check_id": check_id, "step_id": step_id, "pattern": pat})

    for ci, entry in enumerate(registry.get("checks") or []):
        if not isinstance(entry, dict):
            errors.append("ANCHOR_STALE: CHANGED_PATHS_STRUCTURE checks[%d]: 注册项必须是"
                          "对象（实际 %s）" % (ci, type(entry).__name__))
            continue
        cid = entry.get("id")
        if not isinstance(cid, str) or not cid:
            errors.append("ANCHOR_STALE: CHANGED_PATHS_STRUCTURE checks[%d]: 注册项缺 id"
                          % ci)
            continue
        take(cid, cid, cid, entry)
        steps = entry.get("steps")
        if steps is None:
            continue
        if not isinstance(steps, list) or not steps:
            errors.append("ANCHOR_STALE: CHANGED_PATHS_STRUCTURE %s.steps: 必须是非空数组"
                          % cid)
            continue
        for sj, step in enumerate(steps):
            if not isinstance(step, dict):
                errors.append("ANCHOR_STALE: CHANGED_PATHS_STRUCTURE %s.steps[%d]: step 必须"
                              "是对象" % (cid, sj))
                continue
            sid = step.get("id")
            if not isinstance(sid, str) or not sid:
                sid = cid
            take(cid, sid, "%s/%s" % (cid, sid), step)
    return rows, errors


def load_reservations(path: pathlib.Path) -> tuple:
    """读登记表；返回 (reservations, errors)。缺失/坏结构都算判据面不可信。"""
    if not path.is_file():
        return [], ["ANCHOR_STALE: RESERVATIONS_MISSING %s 不存在（家族前缀预留必须显式"
                    "登记，登记面缺失即 fail-closed）" % path]
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        return [], ["ANCHOR_STALE: RESERVATIONS_UNPARSABLE %s: %s" % (path, exc)]
    if not isinstance(doc, dict) or not isinstance(doc.get("reservations"), list):
        return [], ["ANCHOR_STALE: RESERVATIONS_STRUCTURE %s: reservations 必须是数组"
                    % path]
    out, errors = [], []
    for i, item in enumerate(doc["reservations"]):
        if not isinstance(item, dict):
            errors.append("ANCHOR_STALE: RESERVATIONS_STRUCTURE reservations[%d]: 必须是"
                          "对象" % i)
            continue
        miss = [k for k in ("pattern", "kind", "reason", "owner", "sample_path")
                if not isinstance(item.get(k), str) or not item.get(k).strip()]
        if miss:
            errors.append("ANCHOR_STALE: RESERVATIONS_STRUCTURE reservations[%d] (%s): "
                          "缺必填字段 %s" % (i, item.get("pattern"), miss))
            continue
        out.append(item)
    return out, errors


def _load_glob_match(repo: pathlib.Path):
    """加载生产选择器的 glob 实现（eng/ci/incremental.py::glob_match），不另写一套。"""
    spec = importlib.util.spec_from_file_location("astrocs_ci_incremental_pathdomain",
                                                 repo / INCREMENTAL_REL)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.glob_match


# --------------------------------------------------------------------- 判据 ----
def evaluate(registry: dict, *, root: pathlib.Path, glob_match, reservations,
             registry_rel: str = REGISTRY_REL,
             reservations_rel: str = RESERVATIONS_REL) -> dict:
    rows, findings = iter_domains(registry)
    findings = [{"rule": "A1_structure", "severity": "P0", "detail": e}
                for e in findings]

    def bad(rule: str, **kw) -> None:
        findings.append(dict({"rule": rule, "severity": "P0"}, **kw))

    by_pattern: dict = {}
    for row in rows:
        by_pattern.setdefault(row["pattern"], []).append(row)

    # 现存域索引（按 owner 分组，供"冗余可删除"提示）
    existing_by_owner: dict = {}
    for row in rows:
        base, kind = domain_base(row["pattern"])
        target = _domain_target(row["pattern"], base, kind)
        exists = (not target) or (root / target).exists()
        row["base"], row["kind"], row["exists"] = base, kind, exists
        if exists:
            existing_by_owner.setdefault((row["check_id"], row["step_id"]), []).append(row)

    reserved_patterns = {}
    for item in reservations:
        reserved_patterns.setdefault(item["pattern"], []).append(item)

    def nearest_ancestor(base: str) -> str:
        cur = base
        while cur and "/" in cur:
            cur = cur.rsplit("/", 1)[0]
            if (root / cur).exists():
                return cur
        return ""

    # A2 锚存活（未登记的失效域 ⇒ 判红）
    stale = 0
    for row in rows:
        if row["exists"]:
            continue
        if row["pattern"] in reserved_patterns:
            continue                      # A3 登记的家族前缀预留，另行判据
        stale += 1
        owner = (row["check_id"], row["step_id"])
        covering = sorted({q["pattern"] for q in existing_by_owner.get(owner, [])
                           if row["base"] == q["base"]
                           or row["base"].startswith(q["base"] + "/")})
        if not covering:
            # 父项域也可能覆盖 step 域（继承面），一并提示
            covering = sorted({q["pattern"]
                               for q in existing_by_owner.get((row["check_id"],
                                                               row["check_id"]), [])
                               if row["base"] == q["base"]
                               or row["base"].startswith(q["base"] + "/")})
        bad("A2_domain_stale",
            check_id=row["check_id"], step_id=row["step_id"], pattern=row["pattern"],
            base=row["base"], kind=row["kind"],
            action=("删除该域（已由同门现存域覆盖：%s）" % ", ".join(covering)
                    if covering else "改为现行路径域或删除该域"),
            redundant_with=covering,
            nearest_existing_ancestor=nearest_ancestor(row["base"]))

    # A3 家族前缀预留登记（登记项自身带棘轮判据）
    for item in reservations:
        pat = item["pattern"]
        refs = by_pattern.get(pat) or []
        if not refs:
            bad("A3_reservation_unreferenced", pattern=pat, detail=(
                "登记项未被注册表任何 changed_paths 引用 ⇒ 预留已不再需要，删除登记"))
            continue
        if item["kind"] not in ALLOWED_RESERVATION_KINDS:
            bad("A3_reservation_structure", pattern=pat, detail=(
                "kind=%r 非法（允许 %s）" % (item["kind"], list(ALLOWED_RESERVATION_KINDS))))
            continue
        base, kind = domain_base(pat)
        sample = item["sample_path"].strip().replace("\\", "/")
        if not glob_match(sample, pat):
            bad("A3_reservation_ineffective", pattern=pat, sample_path=sample, detail=(
                "样例路径在**生产选择器语义**（eng/ci/incremental.py::glob_match）下不命中"
                "该路径域 ⇒ 这是永不命中的预留（假绿）；注意 dir/** 形态不展开通配"))
            continue
        if (root / sample).exists():
            bad("A3_reservation_obsolete", pattern=pat, sample_path=sample, detail=(
                "样例路径已真实存在 ⇒ 家族已落地，预留不再需要：删除登记并改回普通域"))
            continue

    # A4 扫描面（fail-closed：空扫描不得判绿）
    if not rows:
        bad("A4_scan_face_empty", detail=(
            "注册表未解析出任何 changed_paths 条目 ⇒ 扫描面为空，fail-closed 判红"
            "（不得把「扫不到」当「无违规」）"))

    verdict = "PATH_DOMAIN_ANCHORS_PASS" if not findings else "PATH_DOMAIN_ANCHORS_FAIL"
    return {
        "tool": "eng/tools/quality/check_path_domain_anchors.py",
        "rule": ("CHK-PATH-DOMAIN-ANCHORS / docs/ci/CI_SPEC.md §2.3 + "
                 "docs/ci/01_CHECKS.md §1（锚存活 / fail-closed）"),
        "generated_utc": _utc_now(),
        "registry": registry_rel,
        "reservations": reservations_rel,
        "scanned_domains": len(rows),
        "distinct_patterns": len(by_pattern),
        "owners": len({(r["check_id"], r["step_id"]) for r in rows}),
        "registered_reservations": len(reservations),
        "stale_domains": stale,
        "findings": findings,
        "error_count": len(findings),
        "verdict": verdict,
    }


# --------------------------------------------------------------- 可执行负例面 ----
def _fixture_registry(patterns, *, steps=None, check_id="CHK-FIX") -> dict:
    entry = {"id": check_id, "profiles": ["fast"], "command": ["true"],
             "timeout_seconds": 1, "changed_paths": list(patterns)}
    if steps is not None:
        entry["steps"] = steps
    return {"schema_version": 1, "checks": [entry]}


_GLOB_MATCH_CACHE: list = []


def _default_glob_match():
    """自测用的生产选择器 glob（加载一次；与 main 走同一实现）。"""
    if not _GLOB_MATCH_CACHE:
        _GLOB_MATCH_CACHE.append(_load_glob_match(REPO))
    return _GLOB_MATCH_CACHE[0]


def _run_case(root: pathlib.Path, registry: dict, *, reservations=(), glob_match=None,
              make=()) -> dict:
    glob_match = glob_match or _default_glob_match()
    for rel in make:
        p = root / rel
        if rel.endswith("/"):
            p.mkdir(parents=True, exist_ok=True)
        else:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("x\n", encoding="utf-8")
    return evaluate(registry, root=root, glob_match=glob_match,
                    reservations=list(reservations))


def self_test() -> int:
    import tempfile

    glob_match = _load_glob_match(REPO)
    ok = True
    cases: list = []

    def case(name, res, want_rc, want_rule=""):
        codes = [f["rule"] for f in res["findings"]]
        rc = 0 if res["verdict"] == "PATH_DOMAIN_ANCHORS_PASS" else 1
        good = (rc == want_rc) and ((want_rc == 0) or (want_rule in codes))
        cases.append((name, rc, want_rc, good, codes, want_rule))

    with tempfile.TemporaryDirectory(prefix="path-domain-") as td:
        root = pathlib.Path(td)
        (root / "lib" / "algorithms").mkdir(parents=True)
        (root / "lib" / "algorithms" / "a.cpp").write_text("x\n", encoding="utf-8")

        case("N0 正例（域全存在）",
             _run_case(root, _fixture_registry(["lib/**", "lib/algorithms/a.cpp"])),
             0)

        case("N1 dir/** 基目录缺失",
             _run_case(root, _fixture_registry(["lib/retired/**"])),
             1, "A2_domain_stale")

        case("N2 精确路径缺失",
             _run_case(root, _fixture_registry(["schemas/x.schema.json"])),
             1, "A2_domain_stale")

        case("N3 通配前缀缺失",
             _run_case(root, _fixture_registry(["lib/retired/*/src/**"])),
             1, "A2_domain_stale")

        # 注意：dir/** 形态**不展开通配**（走字面前缀分支），家族前缀必须写成
        # run/E2E-*/**/* 这种"至少一层子路径"的 glob 形态才真的会被命中。
        resv = [{"pattern": "run/E2E-*/**/*", "kind": "family_prefix",
                 "sample_path": "run/E2E-20260101T000000Z/sub/a.json",
                 "reason": "轮次目录家族按需创建", "owner": "ci"}]
        case("N4 已登记家族预留（样例未落地）",
             _run_case(root, _fixture_registry(["lib/**", "run/E2E-*/**/*"]),
                       reservations=resv),
             0)

        case("N5 登记项不再被注册表引用",
             _run_case(root, _fixture_registry(["lib/**"]), reservations=resv),
             1, "A3_reservation_unreferenced")

        case("N6 样例已落地（预留不再需要）",
             _run_case(root, _fixture_registry(["lib/**", "run/E2E-*/**/*"]),
                       reservations=resv,
                       make=["run/E2E-20260101T000000Z/sub/a.json"]),
             1, "A3_reservation_obsolete")

        bad_resv = [dict(resv[0], pattern="run/E2E-*/**",
                         sample_path="run/E2E-20260101T000000Z/a.json")]
        case("N7 样例在选择器语义下不命中（假绿预留）",
             _run_case(root, _fixture_registry(["lib/**", "run/E2E-*/**"]),
                       reservations=bad_resv),
             1, "A3_reservation_ineffective")

        reg = _fixture_registry(["lib/**"])
        reg["checks"][0]["changed_paths"] = "lib/**"
        case("N8 结构不符（changed_paths 非数组）",
             _run_case(root, reg), 1, "A1_structure")

        empty = {"schema_version": 1, "checks": [
            {"id": "CHK-X", "profiles": ["fast"], "command": ["true"],
             "timeout_seconds": 1}]}
        case("N9 空扫描面（fail-closed）",
             _run_case(root, empty), 1, "A4_scan_face_empty")

        res = _run_case(root, _fixture_registry(["lib/**", "lib/retired/**"]))
        f = next((x for x in res["findings"] if x["rule"] == "A2_domain_stale"), {})
        good = f.get("redundant_with") == ["lib/**"]
        cases.append(("N10 冗余域给出 redundant_with", 0 if good else 1, 0, good,
                      [f.get("redundant_with")], "redundant_with=['lib/**']"))

    with tempfile.TemporaryDirectory(prefix="path-domain-fc-") as td:
        rc = main(["--root", td])
        cases.append(("N11 注册表缺失 fail-closed", rc, 2, rc == 2, [], "rc=2"))
    with tempfile.TemporaryDirectory(prefix="path-domain-fc2-") as td:
        root = pathlib.Path(td)
        (root / "eng" / "ci").mkdir(parents=True)
        (root / "eng" / "ci" / "checks.json").write_text("{not json", encoding="utf-8")
        rc = main(["--root", td])
        cases.append(("N12 注册表坏 JSON fail-closed", rc, 2, rc == 2, [], "rc=2"))

    for name, rc, want, good, codes, want_rule in cases:
        ok = ok and good
        print("[selftest] %-42s rc=%-2d want=%-2d %s"
              % (name, rc, want, "OK" if good else "MISMATCH"))
        if not good:
            print("[selftest]   命中=%s 期望含 %s" % (codes, want_rule))
    print("[selftest] %d cases, %s" % (len(cases), "ALL OK" if ok else "FAILED"))
    return 0 if ok else 1


# ------------------------------------------------------------------------- main ----
def _refuse(code: str, detail: str) -> int:
    """fail-closed：输入不可用即 exit 2 并点名，不 traceback、不静默通过。"""
    sys.stderr.write("ANCHOR_STALE: %s %s\n" % (code, detail))
    print(json.dumps({"tool": "eng/tools/quality/check_path_domain_anchors.py",
                      "verdict": "PATH_DOMAIN_INPUT_UNAVAILABLE",
                      "code": code, "detail": detail}, ensure_ascii=False, indent=2))
    return 2


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-PATH-DOMAIN-ANCHORS 路径域锚存活门")
    ap.add_argument("--root", default=str(REPO))
    ap.add_argument("--registry", default=None,
                    help="注册表路径（默认 %s；相对路径按 --root 解析）" % REGISTRY_REL)
    ap.add_argument("--reservations", default=None,
                    help="家族前缀预留登记表（默认 %s）" % RESERVATIONS_REL)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()

    root = pathlib.Path(args.root).resolve()
    reg_rel = args.registry or REGISTRY_REL
    reg_path = pathlib.Path(reg_rel)
    if not reg_path.is_absolute():
        reg_path = root / reg_path
    res_rel = args.reservations or RESERVATIONS_REL
    res_path = pathlib.Path(res_rel)
    if not res_path.is_absolute():
        res_path = root / res_path

    if not reg_path.is_file():
        return _refuse("registry_missing", str(reg_path))
    try:
        registry = json.loads(reg_path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        return _refuse("registry_unparsable", "%s: %s" % (reg_path, exc))
    if not isinstance(registry, dict) or not isinstance(registry.get("checks"), list) \
            or not registry["checks"]:
        return _refuse("registry_bad_shape",
                       "%s: checks 必须是非空数组" % reg_path)
    try:
        glob_match = _load_glob_match(root)
    except Exception as exc:  # noqa: BLE001
        return _refuse("selector_load_error", "%s: %s" % (root / INCREMENTAL_REL, exc))

    reservations, res_errors = load_reservations(res_path)
    if res_errors:
        return _refuse("reservations_unusable", "; ".join(res_errors))

    res = evaluate(registry, root=root, glob_match=glob_match,
                   reservations=reservations,
                   registry_rel=(reg_rel if not reg_path.is_absolute()
                                 else str(reg_path)),
                   reservations_rel=(res_rel if not res_path.is_absolute()
                                     else str(res_path)))
    for f in res["findings"]:
        if f["rule"] == "A1_structure":
            print("FAIL: %s" % f["detail"])
        elif f["rule"] == "A4_scan_face_empty":
            print("FAIL: %s" % f["detail"])
        elif f["rule"] == "A2_domain_stale":
            print("FAIL: %s/%s 路径域 %s (基目录 %s，形态 %s) 不存在；处置：%s"
                  % (f["check_id"], f["step_id"], f["pattern"], f["base"], f["kind"],
                     f["action"]))
        else:
            print("FAIL: %s %s %s" % (f["rule"], f.get("pattern", ""),
                                      f.get("detail", "")))
    print("%s: scanned_domains=%d distinct_patterns=%d stale=%d findings=%d"
          % (res["verdict"], res["scanned_domains"], res["distinct_patterns"],
             res["stale_domains"], res["error_count"]))
    payload = json.dumps(res, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        out = pathlib.Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(payload + "\n", encoding="utf-8")
    print(payload)
    return 0 if res["verdict"] == "PATH_DOMAIN_ANCHORS_PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
