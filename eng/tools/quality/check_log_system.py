#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-LOG-SYS：日志与错误系统判据（LOG-SYS-01 / 最高设计 §7.3）。

权威依据
  - ASTROCS_DESIGN.md §7.3（错误传播与运行日志：顶层约束）；
  - docs/design/LOG_AND_ERROR_SYSTEM.md（详细设计）；
  - docs/contracts/LOG_AND_ERROR_CONTRACT.md（日志行/落点/降级/退出码映射合同）；
  - ENGINEERING_SPEC.md §11（日志、诊断与错误）、§10（每项检查有正例与负例、fail-closed）。

五条判据（exit 0 = PASS；任一违例 => 非 0 + machine JSON verdict=FAIL）：

  R1 ERROR-SWALLOW    生产收敛面（cli/scheduler/pipeline/aio）的 catch 吞错点必须登记在
                      eng/ci/ledgers/log_system_ledger.json#error_swallow（只减不增）。
  R2 SILENT-DEGRADE   生产面的"条件回退点"（存在性探测的结果被返回 + 另有非标志返回值）必须在
                      #silent_degradation 登记，且函数体内写出 degraded_reason；否则判红。
  R3 LOG-LANDING      生产面的硬编码日志落点（字面量自带 logs/ 目录，或赋值给 log_dir/log_path/
                      log_file/event_dir 变量）必须派生自 output_dir 或在 #log_landing 登记。
  R4 LEDGER-INTEGRITY 台账锚存活（文件 + 锚串）、条目数不超过基线（只减不增）、扫描面非空
                      （scanned == 0 => 判红，fail-closed）。
  R5 CONTRACT-ANCHOR  台账 landing_contract.default_dir 必须同时出现在设计文档与合同文档，
                      且 landing_contract.anchors 列出的路径必须存在（改台账不改文档 => 判红）。

能力边界（capability_gaps，随 JSON 公开）
  - 静态扫描证明的是"未登记的吞错/回退/落点存在"，不证明运行期一定走到该分支；
  - R1 只覆盖生产收敛面（cli/scheduler/pipeline/aio）。算法模块的错误面是 C ABI 返回码，
    由模块合同门与 CHK-ALGO-WIRING 覆盖，本门不重复判定；
  - R2 是语法形态识别（探测调用 + 探测结果被返回 + 另一分支返回值），不做跨函数数据流分析；
    调用方决定回退的场景不在本门判定面内。

用法：
  python3 eng/tools/quality/check_log_system.py [--json-out <path>]
  python3 eng/tools/quality/check_log_system.py --root <dir> --ledger <path>   # 沙箱/夹具
  python3 eng/tools/quality/check_log_system.py --self-test                    # 真实仓库全绿 + 注入必红
退出码：0 PASS；1 FAIL；2 输入不可用（fail-closed，含台账缺失/损坏）。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import shutil
import sys
import tempfile

REPO_DEFAULT = pathlib.Path(__file__).resolve().parents[3]
LEDGER_DEFAULT = "eng/ci/ledgers/log_system_ledger.json"

# 生产收敛面：错误必须在这里上行到 CLI（R1）
CONVERGENCE_DIRS = (
    "lib/infrastructure/cli",
    "lib/infrastructure/scheduler",
    "lib/infrastructure/pipeline",
    "lib/infrastructure/aio",
    "lib/infrastructure/observability",
)
# 生产面（R2/R3）：排除生产不可达面与测试/工具面
PRODUCTION_EXCLUDES = (
    "lib/third_party/",
    "/acr/",
    "/tests/",
    "/test/",
    "/tools/",
    "/fixtures/",
)

DOC_ANCHOR_FILES = (
    "docs/design/LOG_AND_ERROR_SYSTEM.md",
    "docs/contracts/LOG_AND_ERROR_CONTRACT.md",
)

CATCH_RE = re.compile(r"\bcatch\s*\([^)]*\)\s*\{")
FUNC_RE = re.compile(
    r"(?:^|[;}{\n])\s*(?:template\s*<[^;{}]*>\s*)?"
    r"(?:[A-Za-z_][\w:<>,\s\*&]*?[\s\*&])([A-Za-z_]\w*)\s*\(([^;{}()]*)\)\s*"
    r"(?:const\s*)?(?:noexcept\s*)?\{",
    re.MULTILINE,
)
KEYWORDS = {"if", "for", "while", "switch", "catch", "return", "else", "do", "sizeof"}
# 存在性/可读性探测族：命中即"优先用该产物"，是条件回退点的语法标志。
# fopen 是 C stdio 的产物探测惯用法（探测结果经 .c_str() 传入，返回值是原变量）。
PROBE_CALL_RE = re.compile(
    r"\b(?:exists|access|stat|is_regular_file|is_directory|fopen)\s*\(\s*"
    r"([\w\.\->\[\]]+)(?:\(\s*\))?\s*[,)]")
RETURN_RE = re.compile(r"\breturn\b([^;]*);")
FLAG_RETURN_RE = re.compile(r"^\s*(?:true|false|0|1|-1|nullptr|NULL|\{\}|void)?\s*$")
NONVALUE_RE = re.compile(r"(::|\bError\s*\(|\bfail\s*\(|\bthrow\b|==|!=|<=|>=|[<>])")
DEGRADED_RE = re.compile(r"degraded_reason")
LOG_LITERAL_RE = re.compile(r'"([^"\n]*)"')
LOGDIR_LITERAL_RE = re.compile(r"(?:^|[/\\])logs[/\\]")
LOGFILE_SUFFIX_RE = re.compile(r"\.(?:log|jsonl)$")
DERIVE_RE = re.compile(r"\b(?:output_dir|out_dir|log_dir|log_dir_|logdir)\b")
LOGNAME_RE = re.compile(r"\b(?:log_dir|log_path|log_file|event_dir|logdir)\b")
SWALLOW_BODY_RE = re.compile(r"^\s*(?:return\s+(?:0|true)\s*;|return\s*;|continue\s*;|break\s*;)?\s*$")


class Fail(Exception):
    """输入不可用（fail-closed）。"""


def mask_cpp(text: str) -> str:
    """注释与字符串/字符字面量置空（保留长度与换行），供语法扫描使用。"""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                out[i] = " "
                i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            out[i] = out[i + 1] = " "
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                if text[i] != "\n":
                    out[i] = " "
                i += 1
            if i < n:
                out[i] = " "
                if i + 1 < n:
                    out[i + 1] = " "
                i += 2
        elif c in ("\"", "'"):
            quote = c
            out[i] = " "
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\" and i + 1 < n:
                    out[i] = out[i + 1] = " "
                    i += 2
                    continue
                if text[i] != "\n":
                    out[i] = " "
                i += 1
            if i < n:
                out[i] = " "
                i += 1
        else:
            i += 1
    return "".join(out)


def mask_comments(text: str) -> str:
    """只掩注释（保留字符串字面量），供落点扫描读取真实字面量。"""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                out[i] = " "
                i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            out[i] = out[i + 1] = " "
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                if text[i] != "\n":
                    out[i] = " "
                i += 1
            if i < n:
                out[i] = " "
                if i + 1 < n:
                    out[i + 1] = " "
                i += 2
        elif c in ("\"", "'"):
            quote = c
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\" and i + 1 < n:
                    i += 2
                    continue
                i += 1
            i += 1
        else:
            i += 1
    return "".join(out)


def match_brace(masked: str, open_idx: int) -> int:
    depth = 0
    for i in range(open_idx, len(masked)):
        if masked[i] == "{":
            depth += 1
        elif masked[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    return -1


# ── 判定面 = 版本库面（GATE-TRIAGE-01）─────────────────────────────────────
# 原实现扫**工作区文件系统**：任何未跟踪/被忽略的 .cpp（例如 .gitignore:37 的
# `archive/` 下的历史快照、并发写者写到一半的文件）都会被当生产面判红。
# 实测（Windows 节点）：lib/plate_solve/archive/vector_method/cpp/v4_4/experiment/
# exp_relvec_core.cpp 触发 R3 未登记 ⇒ 门把「工作区快照」当判据（假红），
# 而该目录按 .gitignore:37（`archive/`，注释「归档目录（GOV-002 起归档文件须受
# Git 跟踪）」）**不是交付面**。
# 口径与 eng/tools/quality/check_ctest_registration.py 的 W4-A3 一致：判定面收敛到
# git ls-files；未跟踪源单独计数并在证据 JSON 的 untracked_cpp_files 字段留痕
# （不静默丢弃）。git 不可用 ⇒ Fail（rc=2，fail-closed），不得当成「全部未跟踪」。
def git_tracked_set(root: pathlib.Path):
    """git ls-files 的 tracked 集合（POSIX 相对路径）；不可用 ⇒ Fail。"""
    import subprocess
    try:
        out = subprocess.run(["git", "-C", str(root), "ls-files", "-z"],
                             capture_output=True, timeout=120)
    except (OSError, subprocess.SubprocessError) as exc:
        raise Fail("git 调用失败（%s: %s）" % (exc.__class__.__name__, exc)) from None
    if out.returncode != 0:
        last = (out.stderr.decode("utf-8", "replace").strip().splitlines() or [""])[-1]
        raise Fail("git ls-files rc=%d: %s" % (out.returncode, last[:140]))
    tracked = {p for p in out.stdout.decode("utf-8", "replace").split("\0") if p}
    if not tracked:
        raise Fail("git ls-files 在 %s 返回空版本库面（空面不得当成「全部未跟踪」）" % root)
    return tracked


def iter_cpp_files(root: pathlib.Path, subdirs, tracked=None, skipped=None):
    """产出 (path, rel) —— 判定面限定为 tracked（tracked=None ⇒ 全收，夹具面用）。

    未跟踪的 .cpp 记入 skipped 列表（留痕，不静默丢弃）。
    """
    for base in subdirs:
        d = root / base
        if not d.is_dir():
            continue
        for p in sorted(d.rglob("*")):
            if p.suffix not in (".cpp", ".h", ".hpp", ".cc"):
                continue
            rel = p.relative_to(root).as_posix()
            if any(x in ("/" + rel) for x in PRODUCTION_EXCLUDES):
                continue
            if tracked is not None and rel not in tracked:
                if skipped is not None:
                    skipped.append(rel)
                continue
            yield p, rel


def line_of(text: str, idx: int) -> int:
    return text.count("\n", 0, idx) + 1


def find_functions(masked: str):
    fns = []
    for m in FUNC_RE.finditer(masked):
        name = m.group(1)
        if name in KEYWORDS:
            continue
        open_idx = masked.find("{", m.end() - 1)
        if open_idx < 0:
            continue
        close_idx = match_brace(masked, open_idx)
        if close_idx < 0:
            continue
        fns.append((name, open_idx, close_idx))
    return fns


def enclosing_function(fns, idx):
    best = None
    for name, a, b in fns:
        if a <= idx <= b and (best is None or (b - a) < (best[2] - best[1])):
            best = (name, a, b)
    return best


def scan_error_swallow(root: pathlib.Path, tracked=None, skipped=None):
    """R1：生产收敛面 catch 吞错点。tracked/skipped 语义见 iter_cpp_files。"""
    hits, scanned = [], 0
    for p, rel in iter_cpp_files(root, CONVERGENCE_DIRS, tracked, skipped):
        text = p.read_text(encoding="utf-8", errors="replace")
        masked = mask_cpp(text)
        scanned += 1
        fns = find_functions(masked)
        seq = {}
        for m in CATCH_RE.finditer(masked):
            open_idx = masked.find("{", m.end() - 1)
            close_idx = match_brace(masked, open_idx)
            if close_idx < 0:
                continue
            body = masked[open_idx + 1:close_idx]
            if not SWALLOW_BODY_RE.match(body):
                continue
            fn = enclosing_function(fns, m.start())
            symbol = fn[0] if fn else "<global>"
            seq[symbol] = seq.get(symbol, 0) + 1
            hits.append({
                "rule": "R1",
                "file": rel,
                "line": line_of(text, m.start()),
                "symbol": symbol,
                "key": "catch#%d" % seq[symbol],
                "anchor": "catch",
                "snippet": text[m.start():min(close_idx + 1, m.start() + 80)].split("\n")[0].strip(),
            })
    return hits, scanned


def scan_silent_degrade(root: pathlib.Path, tracked=None, skipped=None):
    """R2：生产面条件回退点。tracked/skipped 语义见 iter_cpp_files。"""
    hits, scanned = [], 0
    for p, rel in iter_cpp_files(root, ("lib",), tracked, skipped):
        text = p.read_text(encoding="utf-8", errors="replace")
        masked = mask_cpp(text)
        scanned += 1
        for name, a, b in find_functions(masked):
            body = masked[a + 1:b]
            # .c_str()/.data() 是"探测结果传参"惯用法：归一化回原变量名再比对返回值
            probed = {re.sub(r"\.(?:c_str|data|string)$", "", x.strip())
                      for x in PROBE_CALL_RE.findall(body)}
            if not probed:
                continue
            returns = [rm.group(1).strip() for rm in RETURN_RE.finditer(body)]
            if not any(r in probed for r in returns):
                continue
            others = [r for r in returns
                      if r not in probed and r and not FLAG_RETURN_RE.match(r)
                      and not NONVALUE_RE.search(r)]
            if not others:
                continue
            if DEGRADED_RE.search(body):
                continue
            hits.append({
                "rule": "R2",
                "file": rel,
                "line": line_of(text, a),
                "symbol": name,
                "key": "fallback",
                "anchor": name + "(",
                "snippet": "%s: 探测 %s 命中即返回，另一分支返回 %s"
                           % (name, sorted(probed)[:2], others[:2]),
            })
    return hits, scanned


def scan_log_landing(root: pathlib.Path, tracked=None, skipped=None):
    """硬编码日志落点：字面量自带 logs/ 目录，或赋值给 log_dir/log_path/log_file/event_dir。"""
    hits, scanned = [], 0
    for p, rel in iter_cpp_files(root, ("lib",), tracked, skipped):
        text = p.read_text(encoding="utf-8", errors="replace")
        if not text:
            continue
        masked = mask_comments(text)
        lines = masked.split("\n")
        scanned += 1
        seq = 0
        for ln_no, raw in enumerate(lines, start=1):
            lits = [m.group(1) for m in LOG_LITERAL_RE.finditer(raw)]
            if not lits:
                continue
            stmt = statement_context(lines, ln_no)
            rhs = strip_lhs(stmt)
            for lit in lits:
                norm = lit.replace("\\\\", "/")
                is_dir_landing = bool(LOGDIR_LITERAL_RE.search(norm))
                is_logvar = bool(LOGNAME_RE.search(stmt.split("=")[0])) and \
                    bool(LOGFILE_SUFFIX_RE.search(norm))
                if not (is_dir_landing or is_logvar):
                    continue
                if DERIVE_RE.search(rhs):
                    continue
                seq += 1
                hits.append({
                    "rule": "R3",
                    "file": rel,
                    "line": ln_no,
                    "symbol": "<literal>",
                    "key": "literal#%d" % seq,
                    "anchor": lit,
                    "snippet": raw.strip()[:140],
                })
    return hits, scanned


def statement_context(lines, ln_no: int) -> str:
    i = ln_no - 1
    parts = [lines[i] if i < len(lines) else ""]
    j = i - 1
    guard = 0
    while j >= 0 and guard < 6:
        prev = lines[j]
        parts.insert(0, prev)
        if ";" in prev or "{" in prev or "}" in prev:
            break
        j -= 1
        guard += 1
    return " ".join(parts)


def strip_lhs(stmt: str) -> str:
    if "=" in stmt:
        lhs, rhs = stmt.split("=", 1)
        if LOGNAME_RE.search(lhs):
            return rhs
    return stmt


# ── 台账 ─────────────────────────────────────────────────────────────────────
SECTIONS = ("error_swallow", "silent_degradation", "log_landing")


def load_ledger(path: pathlib.Path):
    if not path.is_file():
        raise Fail("台账不存在: %s" % path)
    try:
        led = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        raise Fail("台账不可解析: %s (%s)" % (path, exc))
    for s in SECTIONS:
        sec = led.get(s)
        if not isinstance(sec, dict) or not isinstance(sec.get("entries"), list):
            raise Fail("台账缺段 %s（fail-closed）" % s)
        if not isinstance(sec.get("max_entries"), int):
            raise Fail("台账段 %s 缺 max_entries（只减不增基线）" % s)
    if not isinstance(led.get("landing_contract"), dict):
        raise Fail("台账缺 landing_contract")
    return led


def key_of(hit):
    return (hit["file"], hit["symbol"], hit["key"])


def entry_key(e):
    return (e.get("file"), e.get("symbol"), e.get("key"))


def check_registered(hits, section, rule_id, problems):
    reg = {entry_key(e) for e in section["entries"]}
    for h in hits:
        if key_of(h) not in reg:
            problems.append("%s 未登记: %s:%d [%s/%s] %s"
                            % (rule_id, h["file"], h["line"], h["symbol"], h["key"], h["snippet"]))
    if len(section["entries"]) > section["max_entries"]:
        problems.append("%s 台账条目 %d > 基线 %d（只减不增）"
                        % (rule_id, len(section["entries"]), section["max_entries"]))


def check_anchors(root: pathlib.Path, section, rule_id, problems):
    for e in section["entries"]:
        f = root / e.get("file", "")
        if not f.is_file():
            problems.append("ANCHOR_STALE: %s 条目 %s 的文件不存在: %s"
                            % (rule_id, e.get("id"), e.get("file")))
            continue
        anchor = e.get("anchor")
        if anchor and anchor not in f.read_text(encoding="utf-8", errors="replace"):
            problems.append("ANCHOR_STALE: %s 条目 %s 的锚串 %r 不在 %s"
                            % (rule_id, e.get("id"), anchor, e.get("file")))


def check_contract_anchor(root: pathlib.Path, led, problems):
    lc = led["landing_contract"]
    default_dir = lc.get("default_dir")
    if not isinstance(default_dir, str) or not default_dir:
        problems.append("R5 landing_contract.default_dir 缺失")
        return
    for rel in DOC_ANCHOR_FILES:
        f = root / rel
        if not f.is_file():
            problems.append("ANCHOR_STALE: R5 文档不存在: %s" % rel)
            continue
        if default_dir not in f.read_text(encoding="utf-8", errors="replace"):
            problems.append("R5 落点默认值 %r 未出现在 %s（台账与文档不一致）" % (default_dir, rel))
    for rel in lc.get("anchors", []):
        if not (root / rel).is_file():
            problems.append("ANCHOR_STALE: R5 引用路径不存在: %s" % rel)


def run_checks(root: pathlib.Path, ledger_path: pathlib.Path):
    problems = []
    led = load_ledger(ledger_path)

    # 判定面 = 版本库面：真仓库（有 .git）走 git ls-files；夹具沙箱（无 .git）
    # 按全收，否则自检夹具无法构造。git 不可用 ⇒ Fail（fail-closed）。
    tracked = git_tracked_set(root) if (root / ".git").exists() else None
    skipped = []
    r1_hits, r1_scanned = scan_error_swallow(root, tracked, skipped)
    r2_hits, r2_scanned = scan_silent_degrade(root, tracked, skipped)
    r3_hits, r3_scanned = scan_log_landing(root, tracked, skipped)

    if r1_scanned == 0:
        problems.append("R1 fail-closed: 生产收敛面扫描文件数为 0")
    if r2_scanned == 0 or r3_scanned == 0:
        problems.append("R2/R3 fail-closed: 生产面扫描文件数为 0")

    check_registered(r1_hits, led["error_swallow"], "R1", problems)
    check_registered(r2_hits, led["silent_degradation"], "R2", problems)
    check_registered(r3_hits, led["log_landing"], "R3", problems)

    for section, rid in (("error_swallow", "R1"), ("silent_degradation", "R2"),
                         ("log_landing", "R3")):
        check_anchors(root, led[section], rid, problems)

    check_contract_anchor(root, led, problems)

    return {
        "check": "CHK-LOG-SYS",
        "verdict": "FAIL" if problems else "PASS",
        "root": str(root),
        "scanned": {"convergence_files": r1_scanned, "production_files": r2_scanned},
        "surface": ("git ls-files（版本库面）" if tracked is not None else "工作区全收（无 .git 夹具面）"),
        "untracked_cpp_files": sorted(set(skipped)),
        "hits": {"R1_error_swallow": len(r1_hits), "R2_silent_degrade": len(r2_hits),
                 "R3_log_landing": len(r3_hits)},
        "problems": problems,
        "capability_gaps": [
            "静态扫描只证明未登记点存在，不证明运行期走到该分支",
            "R1 仅覆盖生产收敛面 cli/scheduler/pipeline/aio（算法模块错误面由 C ABI 返回码门覆盖）",
            "R2 是语法形态识别（探测调用 + 探测结果被返回 + 另一分支返回值），不含跨函数数据流分析",
            "判定面 = git ls-files（版本库面）；未跟踪/被忽略的 .cpp 不计入判定，"
            "逐条列在 untracked_cpp_files 留痕（口径同 check_ctest_registration.py W4-A3）",
        ],
    }


def self_test(root: pathlib.Path):
    """真实仓库必须全绿；沙箱注入必须逐条判红；干净沙箱必须判绿。"""
    problems = []
    real = run_checks(root, root / LEDGER_DEFAULT)
    if real["verdict"] != "PASS":
        # 判词不截断（GATE-TRIAGE-01）：原实现 [:5] 会把「哪几条没绿」吞掉，
        # 自检失败时读者拿不到可定位的全貌。
        problems.append("真实仓库未全绿: %s" % (real["problems"] or []))

    tmp = pathlib.Path(tempfile.mkdtemp(prefix="logsys-selftest-"))
    injections = []
    try:
        def sandbox(name):
            d = tmp / name
            for rel in ("lib/infrastructure/cli", "lib/infrastructure/scheduler/src",
                        "lib/infrastructure/aio/src", "docs/design", "docs/contracts",
                        "eng/ci/ledgers"):
                (d / rel).mkdir(parents=True, exist_ok=True)
            for rel in DOC_ANCHOR_FILES:
                (d / rel).write_text("> 上游：ASTROCS_DESIGN.md §7.3\n\n默认落点 <output_dir>/logs\n",
                                     encoding="utf-8")
            return d

        def ledger(d, *, swallow=(), degrade=(), landing=(), default_dir="<output_dir>/logs",
                   anchors=()):
            doc = {
                "ledger_schema": "astrocs.ci-ledger/v1",
                "ledger_id": "log-system",
                "landing_contract": {"default_dir": default_dir, "anchors": list(anchors)},
                "error_swallow": {"max_entries": len(swallow), "entries": list(swallow)},
                "silent_degradation": {"max_entries": len(degrade), "entries": list(degrade)},
                "log_landing": {"max_entries": len(landing), "entries": list(landing)},
            }
            p = d / LEDGER_DEFAULT
            p.write_text(json.dumps(doc, ensure_ascii=False, indent=2), encoding="utf-8")
            return p

        # 基线：干净沙箱必须判绿（能绿）
        d = sandbox("clean")
        (d / "lib/infrastructure/cli/ok.cpp").write_text(
            "int f(){ try { return g(); } catch (...) { return 7; } }\n", encoding="utf-8")
        (d / "lib/infrastructure/aio/src/ok.cpp").write_text(
            'int h(const char* out_dir){ std::string p = std::string(out_dir) + "/logs/run_1.jsonl";'
            ' return w(p); }\n', encoding="utf-8")
        r = run_checks(d, ledger(d))
        if r["verdict"] != "PASS":
            problems.append("干净沙箱未判绿（假红）: %s" % r["problems"][:3])

        # 注入 1：空 catch 吞错（R1 必红）
        d = sandbox("inject_swallow")
        (d / "lib/infrastructure/cli/bad.cpp").write_text(
            "int f(){ try { g(); } catch (...) {} return 0; }\n", encoding="utf-8")
        injections.append(("R1 空 catch 吞错", run_checks(d, ledger(d))["verdict"] == "FAIL"))

        # 注入 2：静默降级（R2 必红）
        d = sandbox("inject_degrade")
        (d / "lib/infrastructure/scheduler/src/bad.cpp").write_text(
            "std::string pick(const Json& doc, const std::string& light) {\n"
            "  const std::string cand = out_dir + \"/calibrated_\" + light;\n"
            "  if (fs::exists(cand)) return cand;\n"
            "  return light;\n}\n", encoding="utf-8")
        injections.append(("R2 静默降级", run_checks(d, ledger(d))["verdict"] == "FAIL"))

        # 注入 3：日志落点非 output_dir（R3 必红）
        d = sandbox("inject_landing")
        (d / "lib/infrastructure/aio/src/bad.cpp").write_text(
            'void f(){ g_aio_log_file = std::fopen("lib/infrastructure/aio/logs/x.log", "a"); }\n',
            encoding="utf-8")
        injections.append(("R3 日志错落", run_checks(d, ledger(d))["verdict"] == "FAIL"))

        # 注入 4：台账锚失效（R4 必红）
        d = sandbox("inject_anchor")
        (d / "lib/infrastructure/cli/ok.cpp").write_text("int f(){ return 0; }\n", encoding="utf-8")
        p = ledger(d, swallow=[{"id": "X", "file": "lib/infrastructure/cli/ok.cpp", "symbol": "f",
                                "key": "catch#1", "anchor": "not_in_file(",
                                "reason": "r", "owner": "o", "exit_condition": "e"}])
        injections.append(("R4 台账锚失效", run_checks(d, p)["verdict"] == "FAIL"))

        # 注入 5：台账落点默认值与文档不一致（R5 必红）
        d = sandbox("inject_contract")
        (d / "lib/infrastructure/cli/ok.cpp").write_text("int f(){ return 0; }\n", encoding="utf-8")
        injections.append(("R5 合同锚漂移", run_checks(d, ledger(d, default_dir="run/logs"))["verdict"] == "FAIL"))

        # 注入 6：扫描面为空（fail-closed 必红）
        d = tmp / "inject_empty"
        (d / "eng/ci/ledgers").mkdir(parents=True, exist_ok=True)
        for rel in DOC_ANCHOR_FILES:
            (d / rel).parent.mkdir(parents=True, exist_ok=True)
            (d / rel).write_text("<output_dir>/logs\n", encoding="utf-8")
        injections.append(("R0 扫描面为空 fail-closed", run_checks(d, ledger(d))["verdict"] == "FAIL"))

        # 注入 7（GATE-TRIAGE-01）：判定面 = 版本库面。
        #     有 .git 的沙箱里，**未跟踪**的 .cpp 带 R3 违规 ⇒ 不得判红（假红），
        #     且必须逐条列进 untracked_cpp_files 留痕（不静默丢弃）。
        #     判别力自证：同一份文件 git add 之后必须判红（负例不是恒真门）。
        import subprocess as _sp

        def _git(d, *cmd):
            return _sp.run(["git"] + list(cmd), cwd=str(d), capture_output=True, text=True)

        d = sandbox("surface_tracked")
        _git(d, "init", "-q")
        _git(d, "config", "user.email", "b12@example.invalid")
        _git(d, "config", "user.name", "B12")
        (d / "lib/infrastructure/aio/src/ok.cpp").write_text(
            "int f(){ return 0; }\n", encoding="utf-8")
        (d / "lib/infrastructure/aio/src/untracked_bad.cpp").write_text(
            'void g(){ h = std::fopen("lib/infrastructure/aio/logs/x.log", "a"); }\n',
            encoding="utf-8")
        p = ledger(d)
        _git(d, "add", "-A")
        _git(d, "commit", "-qm", "tracked-baseline")
        # 基线：文件已被跟踪 ⇒ 判红（R3 有牙）
        tracked_red = run_checks(d, p)["verdict"] == "FAIL"
        # 删除索引项（保留工作区文件）⇒ 变成未跟踪 ⇒ 不得判红，且要留痕
        _git(d, "rm", "--cached", "-q", "lib/infrastructure/aio/src/untracked_bad.cpp")
        untracked = run_checks(d, p)
        injections.append(("S1 tracked 面判红（正控）", tracked_red))
        injections.append(("S2 untracked 不判红 + 留痕",
                           untracked["verdict"] == "PASS"
                           and "lib/infrastructure/aio/src/untracked_bad.cpp"
                           in untracked["untracked_cpp_files"]))

        for name, ok in injections:
            if not ok:
                problems.append("负例注入未判红: %s" % name)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    return problems, injections


def main(argv=None):
    ap = argparse.ArgumentParser(description="CHK-LOG-SYS 日志与错误系统判据")
    ap.add_argument("--root", default=str(REPO_DEFAULT))
    ap.add_argument("--ledger", default=None)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)

    root = pathlib.Path(args.root).resolve()
    ledger_path = pathlib.Path(args.ledger) if args.ledger else (root / LEDGER_DEFAULT)

    try:
        if args.self_test:
            problems, injections = self_test(root)
            payload = {
                "check": "CHK-LOG-SYS",
                "mode": "self-test",
                "verdict": "FAIL" if problems else "PASS",
                "injections": [{"name": n, "red": ok} for n, ok in injections],
                "problems": problems,
            }
        else:
            payload = run_checks(root, ledger_path)
    except Fail as exc:
        print(json.dumps({"check": "CHK-LOG-SYS", "verdict": "FAIL", "error": str(exc)},
                         ensure_ascii=False))
        return 2

    text = json.dumps(payload, ensure_ascii=False, indent=2)
    if args.json_out:
        out = pathlib.Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text + "\n", encoding="utf-8")
    print(text)
    return 0 if payload["verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
