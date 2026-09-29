#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-MUTATION-GATES｜eng/ci/mutation_gates.json 的「登记项必须仍有对象」门。

权威依据
  - ENGINEERING_SPEC.md §10「检查器在输入缺失、路径不存在、依赖不可用时判红；
    『文件不存在』按『无违规』通过视为假绿」；
  - docs/ci/01_CHECKS.md §1「锚存活」（硬编码引用的仓库路径必须存在，失效时显式
    失败并点名）与「fail-closed」（scanned == 0 ⇒ rc != 0）；
  - AGENTS.md §9「门禁/判据本身不合理时改进门禁本身」。

为什么另立本门（根因，一手实测）
  eng/ci/mutation_gates.json 是「近似 ↔ mutation 证明门」的人工登记册，其
  open_items 第 2 条自述「建立『近似 ↔ mutation 门』双向登记并由机器门校验
  （本文件是人工登记，尚无 checker）」。实测（run/DEFECT-REPRO-01）：
    * 全仓**零机器消费方**（grep 命中只在 run/ 与历史审计件里）⇒ 孤儿登记；
    * design_claim 引的 "docs/ASTROCS_DESIGN.md §11.1:565「每个近似有 mutation 证明门
      能红」" 在现行设计文档里**整条不存在**：docs/ASTROCS_DESIGN.md 共 770 行，§11 是
      「双平台发行与安装」（:651），全文零 "mutation" ⇒ 锚已死；
    * 21 条路径形态 token 中 10 条 GONE（run/v6/** 已被 eng/tools/run_gc.py 回收；
      eng/tests/contracts/product_family/evidence/mutations.json 从未入库）。
  孤儿登记 + 失效登记项无人知 ⇒ 本门把「登记项必须仍有对象」变成判据。

判据（任一 A 违规 ⇒ exit 1）
  A1 结构     顶层 schema_version/purpose/gates 齐备；gates 为非空数组；每 gate 的
              id 非空且唯一、approximation 非空、in_ctest/default_on 为布尔。
  A2 锚存活   design_claim 必须是可解析、可复核的引用：
              <doc>[ §<sec>][:<line>]「<claim>」
                - doc 必须存在；§<sec> 必须是 doc 里真实存在的标题号；:<line> 必须在
                  行数内且非空；「<claim>」的文本必须在该 doc 中出现。
              任一不成立 ⇒ MUTATION_GATE_CLAIM_STALE 点名（不得静默降级）。
  A3 登记项仍有对象  每条 gate 的结构化产物字段（driver / registration / evidence）
              里的路径形态 token 必须存在；不存在时必须在同一 gate 的
              gone_artifacts 映射里显式登记（path -> 非空 reason）。
              棘轮（只减不增）：gone_artifacts 的每个 key 必须①被该 gate 的
              driver/registration/evidence 引用，且②确实不存在；产物回来了 ⇒ 判红
              （GONE_ARTIFACT_OBSOLETE，应删除该登记）。与
              eng/ci/path_domain_reservations.json（A3）、
              eng/ci/ledgers/run_manifest_schema_deviations.json 同款。
  A4 扫描面   gates 条目数为 0 ⇒ 判红（fail-closed：空扫描不得判绿）。
  A5 输入     登记册缺失 / 不可解析 ⇒ exit 2 并点名（MUTATION_GATES_INPUT_UNAVAILABLE），
              不得 traceback。
  A6 ctest_name 绑定（FINAL-07 R1：此前该字段**零消费者**，登记了却无人读，改字段也
     无人知道；现在它被读且能判红）——核对面是 eng/ci/ctest_face.py 的两个**独立**面，
     都不依赖本登记册：
     A6a 行锚一致   `ctest_name` 必须真的由 `registration` 指向的文件:行区间里的
                    `add_test(NAME …)` 注册（行锚指向别的名字 ⇒ 判红；纯行漂移按
                    ctest_face.ANCHOR_SLACK 行容忍并在判词里点名）。
     A6b 存在性     给出 `ctest_name` 的条目，该名字必须是配置期注册面（面 A）上的真名；
                    `in_ctest=false` 的条目必须给出非空 `reason`。
     A6c 在跑性     面 B（实际配置面 = `ctest -N --show-only=json-v1` 的真实产出）可用时：
                    `in_ctest=true` ⇒ 名字必须在面 B 上，否则 MUTATION_GATE_NOT_REGISTERED
                    （登记称在跑、实际未注册 —— 发布面风险）；
                    `in_ctest=false` ⇒ 名字必须不在面 B 上，否则
                    MUTATION_GATE_STALE_DECLARATION（实际在跑、登记说没跑）。
                    面 B 不可用（无构建树）⇒ 记 not_evaluated 进 notes，**不**静默通过。
     A6d 未运行面   输出 `not_running`（in_ctest=false 且面 B 确认不在默认配置面）——
                    机器可读的**发布面风险面**，让「未运行」与「运行了但静默通过」严格分开。

消费面（报告项，不判红）
  JSON 输出带 consumers：仓库内引用本登记册的文件清单；带 ctest_name 绑定结论
  （bound / not_running / not_evaluated）——发布面风险的机器可读落点。

只读；仅 stdlib；无网络。
用法:
  python3 eng/ci/check_mutation_gates.py [--registry eng/ci/mutation_gates.json] [--json-out F]
  python3 eng/ci/check_mutation_gates.py --self-test
exit 0 = 全绿；1 = 判据命中；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_REGISTRY = os.path.join("eng", "ci", "mutation_gates.json")
# A6 的核对面单一实现点（eng/ci/ctest_face.py）：面 A 配置期注册面 + 面 B 实际配置面。
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ctest_face  # noqa: E402  (A6 核对面单一实现点)

# 结构化产物字段（只判这三处；reason/note 等散文里的路径不作判据，见 A3 说明）
ARTIFACT_FIELDS = ("driver", "registration", "evidence")
# 路径形态 token（首段必须是仓库顶层目录名，避免把 URL/命令当路径）
PATH_RE = re.compile(
    r"(?:^|[\s(（\[])((?:eng|lib|docs|run|artifacts|testdata|实验|工程控制)"
    r"/[A-Za-z0-9_./\u4e00-\u9fff-]+)")
# design_claim 形态: <doc>[ §<sec>][:<line>]「<claim>」
CLAIM_RE = re.compile(
    r"^\s*(?P<doc>[^\s§:]+)"
    r"(?:\s*§\s*(?P<sec>[^\s:]+))?"
    r"(?::(?P<line>\d+))?\s*"
    r"「(?P<claim>[^」]+)」\s*$")


def _paths_in(value: str) -> list[str]:
    out = []
    for m in PATH_RE.finditer(value or ""):
        out.append(m.group(1).rstrip(".,;，。、"))
    return out


def _headings(text: str) -> list[str]:
    return [m.group(1).strip() for m in
            re.finditer(r"^\s{0,3}#{1,6}\s+(.+?)\s*$", text, re.M)]


def check_claim(repo: str, claim: str) -> list[str]:
    """A2：design_claim 锚存活。返回点名列表（空 = 存活）。"""
    m = CLAIM_RE.match(claim or "")
    if not m:
        return ["MUTATION_GATE_CLAIM_STALE design_claim 形态不可解析（期望 "
                "<doc>[ §<sec>][:<line>]「<claim>」）: %r" % (claim,)]
    doc_rel = m.group("doc")
    sec = m.group("sec")
    line_no = m.group("line")
    want = m.group("claim")
    problems: list[str] = []
    full = os.path.join(repo, doc_rel)
    if not os.path.isfile(full):
        return ["MUTATION_GATE_CLAIM_STALE design_claim 引用的文档不存在: %s" % doc_rel]
    try:
        with open(full, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError as exc:
        return ["MUTATION_GATE_CLAIM_STALE design_claim 文档不可读 %s: %s" % (doc_rel, exc)]
    lines = text.splitlines()
    if sec:
        # 取每个标题的前导节号（如 "## 10. 机器一致性检查" -> "10"、"### 11.1 变异" -> "11.1"）
        # 与 §<sec> 精确比较；不能用"前缀包含"，否则 §10 会被 §10.5 蒙混过关。
        nums = set()
        for ln in lines:
            m2 = re.match(r"^\s{0,3}#{1,6}\s*([0-9]+(?:\.[0-9]+)*)", ln)
            if m2:
                nums.add(m2.group(1))
        if sec not in nums:
            problems.append("MUTATION_GATE_CLAIM_STALE design_claim 引用的章节不存在: "
                            "%s §%s（该文档标题里没有此节号）" % (doc_rel, sec))
    if line_no:
        n = int(line_no)
        if n < 1 or n > len(lines):
            problems.append("MUTATION_GATE_CLAIM_STALE design_claim 引用的行号越界: "
                            "%s:%d（文档共 %d 行）" % (doc_rel, n, len(lines)))
        elif not lines[n - 1].strip():
            problems.append("MUTATION_GATE_CLAIM_STALE design_claim 引用的行是空行: "
                            "%s:%d" % (doc_rel, n))
    if want and want not in text:
        problems.append("MUTATION_GATE_CLAIM_STALE design_claim 的引文在该文档中不存在: "
                        "%s 内找不到「%s」" % (doc_rel, want[:60]))
    return problems


def check_registry(repo: str, doc: dict) -> tuple[list[str], dict]:
    """A1/A2/A3/A4。返回 (problems, info)。"""
    problems: list[str] = []
    info: dict = {"gates": 0, "artifacts_checked": 0, "gone_registered": 0}
    if not isinstance(doc, dict):
        return ["A1 登记册顶层必须是对象"], info
    for key in ("schema_version", "purpose", "gates"):
        if key not in doc:
            problems.append("A1 顶层缺必需键: %s" % key)
    gates = doc.get("gates")
    if not isinstance(gates, list):
        return problems + ["A1 gates 必须是数组"], info
    if not gates:
        return problems + ["A4 gates 为空（fail-closed：空扫描不得判绿）"], info
    info["gates"] = len(gates)

    if "design_claim" in doc:
        problems += check_claim(repo, str(doc.get("design_claim")))

    seen_ids: set = set()
    for idx, g in enumerate(gates):
        where = "gates[%d]" % idx
        if not isinstance(g, dict):
            problems.append("A1 %s 必须是对象" % where)
            continue
        gid = str(g.get("id") or "").strip()
        if not gid:
            problems.append("A1 %s 缺 id" % where)
        elif gid in seen_ids:
            problems.append("A1 gate id 重复: %s" % gid)
        else:
            seen_ids.add(gid)
        where = "gates[%d](%s)" % (idx, gid or "?")
        if not str(g.get("approximation") or "").strip():
            problems.append("A1 %s 缺 approximation（该门守什么近似）" % where)
        for bk in ("in_ctest", "default_on"):
            if not isinstance(g.get(bk), bool):
                problems.append("A1 %s 的 %s 必须是布尔" % (where, bk))
        gone = g.get("gone_artifacts") or {}
        if not isinstance(gone, dict):
            problems.append("A1 %s 的 gone_artifacts 必须是对象（path -> reason）" % where)
            gone = {}
        referenced: set = set()
        for field in ARTIFACT_FIELDS:
            val = g.get(field)
            if not isinstance(val, str):
                continue
            for tok in _paths_in(val):
                referenced.add(tok)
                info["artifacts_checked"] += 1
                if os.path.exists(os.path.join(repo, tok)):
                    continue
                if tok in gone and str(gone.get(tok) or "").strip():
                    info["gone_registered"] += 1
                    continue
                problems.append(
                    "MUTATION_GATE_ARTIFACT_GONE %s 的 %s 指向不存在的产物 %s"
                    "（登记项已无对象：删除该登记，或在 gone_artifacts 里显式登记"
                    " path -> 原因）" % (where, field, tok))
        # 棘轮：gone_artifacts 的 key 必须仍被引用且确实不存在
        for tok, reason in gone.items():
            if not str(reason or "").strip():
                problems.append("A1 %s 的 gone_artifacts[%s] 缺原因" % (where, tok))
            if tok not in referenced:
                problems.append(
                    "MUTATION_GATE_GONE_UNREFERENCED %s 的 gone_artifacts[%s] 不再被"
                    " driver/registration/evidence 引用（登记项失效，须删除）" % (where, tok))
            elif os.path.exists(os.path.join(repo, tok)):
                problems.append(
                    "MUTATION_GATE_GONE_OBSOLETE %s 的 gone_artifacts[%s] 指向的产物已"
                    "重新存在（棘轮只减不增，须删除该登记）" % (where, tok))
    return problems, info


# ------------------------------------------------------------------- A6 ----

def check_ctest_name_binding(repo, doc, face) -> tuple:
    """A6：gates[].ctest_name 的绑定核对（FINAL-07 R1 —— 该字段此前零消费者）。

    核对面 = eng/ci/ctest_face.py 的合并面，与本登记册**无依赖关系**（不自指）。
    返回 (problems, info)；info 带 bound / not_running / not_evaluated 三个报告项。
    """
    import pathlib
    problems = []
    info = {"bound": [], "not_running": [], "not_evaluated": [],
            "face": face.as_dict() if face is not None else None}
    gates = doc.get("gates") if isinstance(doc, dict) else None
    if not isinstance(gates, list):
        return problems, info
    for idx, g in enumerate(gates):
        if not isinstance(g, dict):
            continue
        gid = str(g.get("id") or "?")
        where = "gates[%d](%s)" % (idx, gid)
        name = g.get("ctest_name")
        in_ctest = g.get("in_ctest")
        if name is None:
            if in_ctest is True:
                problems.append(
                    "MUTATION_GATE_CTEST_NAME_MISSING %s 的 in_ctest=true 但没有 ctest_name"
                    "（登记称进 ctest 却给不出目标名）" % where)
            continue
        name = str(name).strip()
        if not name:
            problems.append("MUTATION_GATE_CTEST_NAME_EMPTY %s 的 ctest_name 为空串" % where)
            continue
        info["bound"].append({"id": gid, "ctest_name": name, "in_ctest": in_ctest})
        # A6a 行锚一致：行锚必须真的覆盖注册该名字的 add_test
        anchor = g.get("registration")
        parsed = ctest_face.find_anchor(str(anchor or ""))
        if not anchor or not parsed:
            problems.append(
                "MUTATION_GATE_ANCHOR_UNUSABLE %s 的 ctest_name=%s 需要一个 file:line"
                " 行锚的 registration（现值 %r）——行锚是可复核的落点，不接受散文登记"
                % (where, name, anchor))
        else:
            hit, detail = ctest_face.anchor_hits_add_test(
                pathlib.Path(repo), str(anchor), name)
            if not hit:
                problems.append(
                    "MUTATION_GATE_ANCHOR_STALE %s 的 ctest_name=%s 与行锚 registration=%s"
                    " 不符：%s" % (where, name, anchor, detail))
            elif "漂移" in detail:
                info.setdefault("anchor_drift", []).append(
                    {"id": gid, "ctest_name": name, "detail": detail})
        # A6b 存在性：必须是配置期注册面（面 A）上的真名
        if face is not None and not face.has(name):
            problems.append(
                "MUTATION_GATE_CTEST_NAME_UNKNOWN %s 的 ctest_name=%s 不在配置期注册面"
                "（面 A 静态名 ∪ 面 B 实际配置面）——引用了一个不存在的 ctest 目标"
                % (where, name))
            continue
        # A6c 在跑性：面 B 可用时，登记的 in_ctest 必须与实际配置面一致
        on_cfg = face.on_configured(name) if face is not None else None
        if on_cfg is None:
            info["not_evaluated"].append(
                {"id": gid, "ctest_name": name, "in_ctest": in_ctest,
                 "reason": "面 B 不可用（无可用构建树 / ctest -N 失败）"})
        elif in_ctest is True and not on_cfg:
            problems.append(
                "MUTATION_GATE_NOT_REGISTERED %s 登记 in_ctest=true（默认在 ctest 面），"
                "但 %s 不在实际配置面（ctest -N 的真实产出）——该变异门**未运行**"
                "（发布面风险：门不会跑，却没有任何红）" % (where, name))
        elif in_ctest is False and on_cfg:
            problems.append(
                "MUTATION_GATE_STALE_DECLARATION %s 登记 in_ctest=false，但 %s 实际在"
                "配置面上（会被 ctest 执行）——登记与实况不符" % (where, name))
        elif in_ctest is False:
            # A6d 未运行面：登记与实况一致地声明「默认不跑」——机器可读的发布面风险
            if not str(g.get("reason") or "").strip():
                problems.append(
                    "MUTATION_GATE_REASON_MISSING %s 的 in_ctest=false 必须给出 reason"
                    "（为什么默认不跑）——否则「未运行」无从复核" % where)
            info["not_running"].append(
                {"id": gid, "ctest_name": name,
                 "face": face.configured_source if face is not None else None})
    return problems, info


# 消费方扫描面：只扫工程/文档/源码面，避开只读数据集（gaia/、testdata/）与产物区，
# 否则 walk 会拖到分钟级（实测 gaia/ 体量足以让 300s 超时）。
CONSUMER_SCAN_ROOTS = ("eng", "docs", "lib")
CONSUMER_MAX_BYTES = 2 * 1024 * 1024

# Windows 保留设备名（NUL/CON/PRN/AUX/COM1-9/LPT1-9）。工作树里存在这类条目时，
# ntpath.relpath 会把路径解析成设备名并抛 ValueError
# （path is on mount ...nul, start on mount 'F:'）⇒ 检查器抛 Traceback 而
# **没有 verdict**。门崩与判红必须可区分（GATE-TRIAGE-01）。
RESERVED_DEVICE_RE = re.compile(
    r"^(?:nul|con|prn|aux|com[1-9]|lpt[1-9])(?:\..*)?$", re.IGNORECASE)


def rel_path(repo: str, full: str):
    """相对仓库根的 POSIX 风格路径；不可解析（设备名/跨卷/符号环）返回 None。

    不静默丢弃：None 由调用方登记进 CONSUMER_SCAN_UNRESOLVABLE 报告项。
    """
    try:
        return os.path.relpath(full, repo).replace(os.sep, "/")
    except (ValueError, OSError):
        return None


def find_consumers(repo: str):
    """(消费方, 不可解析路径) 二元组；后者非空即 fail-closed 判红。

    消费方 = 仓库内引用本登记册的文件（限 CONSUMER_SCAN_ROOTS）。
    """
    hits: list[str] = []
    unresolvable: list[str] = []
    skip = {".git", "build", "run", "artifacts", "__pycache__", ".dsh-code-index",
            "gaia", "testdata", "实验", "工程控制"}
    for root in CONSUMER_SCAN_ROOTS:
        base = os.path.join(repo, root)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames
                           if d not in skip and not RESERVED_DEVICE_RE.match(d)]
            for fn in filenames:
                if RESERVED_DEVICE_RE.match(fn):
                    unresolvable.append(os.path.join(dirpath, fn))
                    continue
                full = os.path.join(dirpath, fn)
                rel = rel_path(repo, full)
                if rel is None:
                    unresolvable.append(full)
                    continue
                if rel == "eng/ci/mutation_gates.json":
                    continue
                try:
                    if os.path.getsize(full) > CONSUMER_MAX_BYTES:
                        continue
                    with open(full, encoding="utf-8", errors="ignore") as fh:
                        if "mutation_gates.json" in fh.read():
                            hits.append(rel)
                except OSError:
                    continue
    return sorted(hits), sorted(unresolvable)


def _self_test() -> int:
    """可执行正/负例面（A1/A2/A3/A4/A5 逐条）。"""
    import shutil
    import tempfile

    problems: list[str] = []
    tmp = tempfile.mkdtemp(prefix="astrocs_mg_")
    try:
        os.makedirs(os.path.join(tmp, "docs"))
        os.makedirs(os.path.join(tmp, "eng", "tests", "x"))
        with open(os.path.join(tmp, "docs", "DESIGN.md"), "w", encoding="utf-8") as fh:
            fh.write("# D\n\n## 11.1 变异\n\n每个近似有 mutation 证明门能红\n")
        with open(os.path.join(tmp, "eng", "tests", "x", "run.py"), "w",
                  encoding="utf-8") as fh:
            fh.write("print(1)\n")
        # A6 的行锚核对面需要「提交树」版本：建一个最小 git 仓库当 fixture 仓
        import subprocess as _sp2
        _sp2.run(["git", "init", "-q"], cwd=tmp, capture_output=True)
        _sp2.run(["git", "config", "user.email", "t@t"], cwd=tmp, capture_output=True)
        _sp2.run(["git", "config", "user.name", "t"], cwd=tmp, capture_output=True)
        with open(os.path.join(tmp, "eng", "tests", "x", "CMakeLists.txt"), "w",
                  encoding="utf-8") as fh:
            fh.write("add_test(NAME real_on COMMAND t)\n"
                     "option(ENABLE_MUT OFF)\n"
                     "if(ENABLE_MUT)\n"
                     "  add_test(NAME real_off COMMAND t)\n"
                     "endif()\n")
        _sp2.run(["git", "add", "-A"], cwd=tmp, capture_output=True)
        _sp2.run(["git", "commit", "-qm", "a6"], cwd=tmp, capture_output=True)
        # 面 B 由内存 fixture 注入（不依赖真构建树）
        _fj = os.path.join(tmp, "face.json")
        with open(_fj, "w", encoding="utf-8") as fh:
            json.dump({"version": {"major": 1},
                       "tests": [{"name": "real_on"}]}, fh)
        face = ctest_face.resolve(__import__("pathlib").Path(tmp), json_path=_fj,
                                  run_ctest=False)

        def doc(claim="docs/DESIGN.md §11.1:5「每个近似有 mutation 证明门能红」",
                driver="eng/tests/x/run.py", gone=None, gid="g1", dup=False,
                gates_empty=False):
            g = {"id": gid, "approximation": "x", "in_ctest": False,
                 "default_on": True, "driver": driver}
            if gone:
                g["gone_artifacts"] = gone
            gs = [g]
            if dup:
                gs.append(dict(g))
            return {"schema_version": 1, "purpose": "p",
                    "design_claim": claim, "gates": [] if gates_empty else gs}

        def case(name, d, want_rc, token=None, raw=None):
            if raw is None:
                p, _info = check_registry(tmp, d)
            else:
                p = raw
            rc = 1 if p else 0
            blob = " ".join(p)
            ok = (rc == want_rc) and (token is None or token in blob)
            problems.append("%s rc=%d want=%d %s | %s"
                            % (name, rc, want_rc, "OK" if ok else "MISMATCH", blob[:120]))

        case("pos_live", doc(), 0)
        case("neg_claim_text_absent",
             doc(claim="docs/DESIGN.md §11.1:5「这句话不存在」"), 1,
             "MUTATION_GATE_CLAIM_STALE")
        case("neg_claim_section_absent",
             doc(claim="docs/DESIGN.md §99.9:5「每个近似有 mutation 证明门能红」"), 1,
             "章节不存在")
        case("neg_claim_line_oob",
             doc(claim="docs/DESIGN.md §11.1:9999「每个近似有 mutation 证明门能红」"), 1,
             "行号越界")
        case("neg_claim_doc_absent",
             doc(claim="docs/NOPE.md「x」"), 1, "文档不存在")
        case("neg_artifact_gone_unregistered",
             doc(driver="eng/tests/x/gone.py"), 1, "MUTATION_GATE_ARTIFACT_GONE")
        case("pos_artifact_gone_registered",
             doc(driver="eng/tests/x/gone.py",
                 gone={"eng/tests/x/gone.py": "run/ 一次性脚本，已随 run_gc 回收"}), 0)
        case("neg_gone_obsolete",
             doc(driver="eng/tests/x/run.py",
                 gone={"eng/tests/x/run.py": "误登记"}), 1, "MUTATION_GATE_GONE_OBSOLETE")
        case("neg_gone_unreferenced",
             doc(gone={"eng/tests/x/other.py": "无关"}), 1,
             "MUTATION_GATE_GONE_UNREFERENCED")
        case("neg_dup_id", doc(dup=True), 1, "id 重复")
        case("neg_empty_gates", doc(gates_empty=True), 1, "gates 为空")
        case("neg_missing_required_key",
             {"schema_version": 1, "gates": []}, 1, "顶层缺必需键")

        # A6：ctest_name 绑定（内存 fixture 仓 + 注入面 B）
        cm = "eng/tests/x/CMakeLists.txt"

        def gdoc(name, in_ctest, anchor, reason="注入原因", gid="g6"):
            return {"schema_version": 1, "purpose": "p",
                    "gates": [{"id": gid, "approximation": "x",
                               "in_ctest": in_ctest, "default_on": bool(in_ctest),
                               "ctest_name": name, "registration": anchor,
                               "reason": reason, "driver": "eng/tests/x/run.py"}]}

        def a6case(label, d, want_rc, token=None):
            p, _i = check_ctest_name_binding(tmp, d, face)
            rc = 1 if p else 0
            blob = " ".join(p)
            ok = (rc == want_rc) and (token is None or token in blob)
            problems.append("%s rc=%d want=%d %s | %s"
                            % (label, rc, want_rc, "OK" if ok else "MISMATCH", blob[:120]))

        a6case("a6_pos_on", gdoc("real_on", True, "%s:1" % cm), 0)
        a6case("a6_pos_off", gdoc("real_off", False, "%s:3-4" % cm), 0)
        a6case("a6_neg_ghost_name", gdoc("no_such_target_zzz", True, "%s:1" % cm), 1,
               "MUTATION_GATE_ANCHOR_STALE")
        a6case("a6_neg_anchor_points_elsewhere",
               gdoc("real_on", True, "%s:3-4" % cm), 1, "MUTATION_GATE_ANCHOR_STALE")
        a6case("a6_neg_anchor_prose", gdoc("real_on", True, "见 CMakeLists"), 1,
               "MUTATION_GATE_ANCHOR_UNUSABLE")
        a6case("a6_neg_lie_running", gdoc("real_off", True, "%s:3-4" % cm), 1,
               "MUTATION_GATE_NOT_REGISTERED")
        a6case("a6_neg_lie_not_running", gdoc("real_on", False, "%s:1" % cm), 1,
               "MUTATION_GATE_STALE_DECLARATION")
        a6case("a6_neg_reason_missing", gdoc("real_off", False, "%s:3-4" % cm, reason=" "),
               1, "MUTATION_GATE_REASON_MISSING")
        a6case("a6_neg_name_missing_but_in_ctest",
               {"schema_version": 1, "purpose": "p",
                "gates": [{"id": "g7", "approximation": "x", "in_ctest": True,
                           "default_on": True, "driver": "eng/tests/x/run.py"}]},
               1, "MUTATION_GATE_CTEST_NAME_MISSING")
        _p, _i = check_ctest_name_binding(tmp, gdoc("real_off", False, "%s:3-4" % cm), face)
        problems.append("a6_disc_off_gate_in_not_running %s"
                        % ("OK" if not _p and not face.on_configured("real_off")
                           else "MISMATCH"))

        # A5：登记册缺失/不可解析
        for name, path, want_rc, token in (
                ("neg_input_missing", os.path.join(tmp, "nope.json"), 2, "INPUT_UNAVAILABLE"),
                ("neg_input_bad_json", None, 2, "INPUT_UNAVAILABLE")):
            if path is None:
                path = os.path.join(tmp, "bad.json")
                with open(path, "w", encoding="utf-8") as fh:
                    fh.write("{not json")
            rc, blob = _load_and_check(tmp, path)
            ok = (rc == want_rc) and (token in blob)
            problems.append("%s rc=%d want=%d %s | %s"
                            % (name, rc, want_rc, "OK" if ok else "MISMATCH", blob[:120]))

        # A6（GATE-TRIAGE-01 负例）：Windows 保留设备名/跨卷路径不得让门崩。
        # 复现原缺陷：os.walk 在工作树里遇到设备名条目时 ntpath.relpath 抛
        #   ValueError: path is on mount '<device>', start on mount 'F:'
        # 原实现直接 Traceback（rc=1 但**没有 verdict**，与「判红」不可区分）。
        # 本负例在任意平台可跑：monkeypatch relpath 对探针路径抛同样的异常，
        # 断言 ①find_consumers 不抛 ②该路径进 unresolvable（不静默丢弃）。
        probe_dir = os.path.join(tmp, "eng", "probe")
        os.makedirs(probe_dir, exist_ok=True)
        probe = os.path.join(probe_dir, "consumer.py")
        with open(probe, "w", encoding="utf-8") as fh:
            fh.write("# mutation_gates.json\n")
        real_relpath = os.path.relpath
        real_rel_path = globals()["rel_path"]

        def boom(path, start=None):
            if os.path.abspath(str(path)) == os.path.abspath(probe):
                raise ValueError("path is on mount '<device>', start on mount 'F:'")
            return real_relpath(path, start) if start is not None else real_relpath(path)

        os.path.relpath = boom
        try:
            hits, unres = find_consumers(tmp)
            crashed = False
        except Exception:  # noqa: BLE001 - 崩溃即负例失败
            hits, unres, crashed = [], [], True
        finally:
            os.path.relpath = real_relpath
        ok = (not crashed) and any("consumer.py" in u for u in unres)
        problems.append("neg_device_path_no_crash %s | unresolvable=%s"
                        % ("OK" if ok else "MISMATCH", unres[:3]))

        # A6b（判别力自证）：把 rel_path 换回修复前的裸 os.path.relpath，
        # 同一输入必须抛 ValueError —— 证明 A6 不是恒真门。
        def raw_rel_path(repo_, full_):
            return os.path.relpath(full_, repo_).replace(os.sep, "/")

        globals()["rel_path"] = raw_rel_path
        os.path.relpath = boom
        try:
            find_consumers(tmp)
            pre_fix_raised = False
        except ValueError:
            pre_fix_raised = True
        except Exception:  # noqa: BLE001
            pre_fix_raised = False
        finally:
            os.path.relpath = real_relpath
            globals()["rel_path"] = real_rel_path
        problems.append("neg_device_path_pre_fix_raises %s"
                        % ("OK" if pre_fix_raised else "MISMATCH"))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    bad = [p for p in problems if "OK" not in p]
    for p in problems:
        print("[selftest] %s" % p)
    print("[selftest] %d cases, %s" % (len(problems), "ALL OK" if not bad else "FAILED"))
    return 0 if not bad else 1


def _load_and_check(repo: str, path: str) -> tuple[int, str]:
    if not os.path.isfile(path):
        return 2, "MUTATION_GATES_INPUT_UNAVAILABLE 登记册缺失: %s" % path
    try:
        with open(path, encoding="utf-8") as fh:
            doc = json.load(fh)
    except (OSError, ValueError) as exc:
        return 2, "MUTATION_GATES_INPUT_UNAVAILABLE 登记册不可解析 %s: %s" % (path, exc)
    problems, _info = check_registry(repo, doc)
    return (1 if problems else 0), " ".join(problems)


def _resolve_face(repo, json_path=None, run_ctest=True):
    """取 A6 的核对面。不可用 ⇒ 返回异常对象（A6 记 not_evaluated，不静默通过）。"""
    import pathlib
    try:
        return ctest_face.resolve(pathlib.Path(repo), json_path=json_path,
                                  run_ctest=run_ctest)
    except ctest_face.Unavailable as exc:
        return exc


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--registry", default=None,
                    help="登记册路径（默认 eng/ci/mutation_gates.json）")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--ctest-face", default=None,
                    help="A6 核对面 B 的已抓取产物（ctest -N --show-only=json-v1 的 JSON）")
    ap.add_argument("--no-ctest-probe", action="store_true",
                    help="不跑 ctest -N（面 B 记 not_evaluated；自检用）")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    ap.add_argument("--fault-inject", default=None,
                    help="真仓注入负例：all / ghost / anchor / notrun / stale / noreason")
    args = ap.parse_args(argv)
    if args.self_test:
        return _self_test()
    if args.fault_inject:
        return _fault_inject(args.fault_inject)
    path = args.registry or os.path.join(REPO, DEFAULT_REGISTRY)
    if not os.path.isfile(path):
        print("MUTATION_GATES_INPUT_UNAVAILABLE 登记册缺失: %s" % path, file=sys.stderr)
        return 2
    try:
        with open(path, encoding="utf-8") as fh:
            doc = json.load(fh)
    except (OSError, ValueError) as exc:
        print("MUTATION_GATES_INPUT_UNAVAILABLE 登记册不可解析 %s: %s" % (path, exc),
              file=sys.stderr)
        return 2
    problems, info = check_registry(REPO, doc)
    # A6：ctest_name 绑定（核对面 = 独立枚举面，eng/ci/ctest_face.py）
    face = _resolve_face(REPO, json_path=args.ctest_face,
                         run_ctest=not args.no_ctest_probe)
    a6_notes = []
    if isinstance(face, Exception):
        problems.append("MUTATION_GATE_FACE_UNAVAILABLE A6 核对面不可用：%s" % face)
        a6_info = {"bound": [], "not_running": [], "not_evaluated": [], "face": None}
    else:
        a6_problems, a6_info = check_ctest_name_binding(REPO, doc, face)
        problems = problems + a6_problems
        a6_notes = list(face.notes)
        problems = problems + ["MUTATION_GATE_FACE_ERROR %s" % e for e in face.errors]
    consumers, unresolvable = find_consumers(REPO)
    if unresolvable:
        # fail-closed：路径归不到仓库根（Windows 保留设备名/跨卷/符号环）时
        # 不静默丢弃，具名判红 —— 否则扫描面被悄悄缩小而门仍报绿。
        problems = problems + [
            "CONSUMER_SCAN_UNRESOLVABLE 消费方扫描面有 %d 条路径无法归到仓库根：%s"
            % (len(unresolvable), unresolvable[:5])]
    out = {
        "checker": "eng/ci/check_mutation_gates.py",
        "registry": os.path.relpath(path, REPO).replace(os.sep, "/"),
        "gates": info["gates"],
        "artifacts_checked": info["artifacts_checked"],
        "gone_registered": info["gone_registered"],
        "consumers": consumers,
        "consumers_unresolvable": unresolvable,
        "ctest_name_binding": a6_info,
        "ctest_face_notes": a6_notes,
        "problems": problems,
        "verdict": "MUTATION_GATES_RED" if problems else "MUTATION_GATES_OK",
    }
    text = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        parent = os.path.dirname(os.path.abspath(args.json_out))
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            fh.write(text + "\n")
    if problems:
        print("MUTATION_GATES_RED problems=%d" % len(problems))
        for p in problems:
            print("  [%s] %s" % (p.split(" ", 1)[0], p.split(" ", 1)[1] if " " in p else ""))
        print("  消费方（报告项）: %s" % (consumers or "无（孤儿登记）"))
        return 1
    print("MUTATION_GATES_OK gates=%d artifacts=%d gone_registered=%d consumers=%d"
          % (info["gates"], info["artifacts_checked"], info["gone_registered"],
             len(consumers)))
    print("  ctest_name 绑定: bound=%d 未运行(in_ctest=false 且确认不在默认配置面)=%d "
          "未判定(面 B 不可用)=%d"
          % (len(a6_info["bound"]), len(a6_info["not_running"]),
             len(a6_info["not_evaluated"])))
    for row in a6_info["not_running"]:
        print("    [未运行] %s (%s)：默认不在 ctest 配置面 —— 发布面风险"
              % (row["id"], row["ctest_name"]))
    for row in a6_info["not_evaluated"]:
        print("    [未判定] %s (%s)：%s" % (row["id"], row["ctest_name"], row["reason"]))
    for row in a6_info.get("anchor_drift", []):
        print("    [行锚漂移] %s：%s" % (row["id"], row["detail"]))
    return 0


def _fault_inject(which: str) -> int:
    """真仓注入负例：每例必须判红（能红能绿，ENGINEERING_SPEC §10）。

    注入只改内存里的 doc，不落盘；跑完即弃。
    """
    import copy
    path = os.path.join(REPO, DEFAULT_REGISTRY)
    with open(path, encoding="utf-8") as fh:
        base = json.load(fh)
    face = _resolve_face(REPO)
    if isinstance(face, Exception):
        print("FAULT_INJECT_UNAVAILABLE 核对面不可用：%s" % face, file=sys.stderr)
        return 2

    def pick(off=False):
        for x in base["gates"]:
            if not x.get("ctest_name"):
                continue
            if off and x.get("in_ctest") is not False:
                continue
            return x.get("id")
        return None

    on_id = pick(off=False)
    off_id = pick(off=True)
    if on_id is None:
        print("FAULT_INJECT_UNAVAILABLE 登记册里没有带 ctest_name 的条目", file=sys.stderr)
        return 2
    cases = {}

    def mut(gid, **kw):
        doc = copy.deepcopy(base)
        g = next(x for x in doc["gates"] if x.get("id") == gid)
        g.update(kw)
        return doc

    # 负例 1：ctest_name 指向一个两面都不存在的名字 ⇒ A6b 判红
    cases["ghost"] = (mut(on_id, ctest_name="ctest_ghost_name_zzz_not_registered"),
                       "MUTATION_GATE_CTEST_NAME_UNKNOWN")
    # 负例 2：行锚指向别的 add_test 名字 ⇒ A6a 判红
    cases["anchor"] = (mut(on_id, ctest_name="aio_oracle"),
                        "MUTATION_GATE_ANCHOR_STALE")
    if off_id is not None:
        # 负例 3：把默认不跑的条目标成在跑 ⇒ A6c 判红（发布面风险的判红能力）
        cases["notrun"] = (mut(off_id, in_ctest=True, default_on=True),
                           "MUTATION_GATE_NOT_REGISTERED")
        # 负例 4：声明不跑却不给 reason ⇒ A6d 判红
        cases["noreason"] = (mut(off_id, reason="   "),
                             "MUTATION_GATE_REASON_MISSING")
    # 负例 5：在跑的条目谎称不在跑 ⇒ A6c 反向判红
    cases["stale"] = (mut(on_id, in_ctest=False, reason="注入"),
                      "MUTATION_GATE_STALE_DECLARATION")

    if which not in cases and which not in ("all", "*"):
        print("FAULT_INJECT_UNKNOWN 可用注入: %s" % sorted(cases), file=sys.stderr)
        return 2
    sel = sorted(cases) if which in ("all", "*") else [which]
    bad = 0
    for name in sel:
        doc, token = cases[name]
        problems, _info = check_ctest_name_binding(REPO, doc, face)
        hit = any(token in p for p in problems)
        print("  [%s] 注入 %s ⇒ 期望 %s" % ("PASS" if hit else "FAIL", name, token))
        if not hit:
            bad += 1
            print("      实得: %s" % (problems or "（无问题）"))
    problems, _info = check_ctest_name_binding(REPO, base, face)
    print("  [%s] 正例 真仓未注入 ⇒ 无 A6 问题（problems=%d）"
          % ("PASS" if not problems else "FAIL", len(problems)))
    if problems:
        bad += 1
    print("FAULT_INJECT %s (%d 例)" % ("PASS" if bad == 0 else "FAIL", len(sel) + 1))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
