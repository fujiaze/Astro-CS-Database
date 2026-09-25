#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-RETIRED-CODE：ENGINEERING_SPEC §2「历史实现处置」机器判据（CLEAN-401 验收门）。

权威依据
  - ENGINEERING_SPEC.md §2 逐字：
      「历史实现处置（二选一，没有第三种状态）：1. 直接删除……2. 保留则注释：
        确有参考价值需暂时保留的（如等待接线的实现、隔离实验），在代码上方用统一
        注释块写明：它是什么、为什么保留、现状（未接入生产/仅供什么实验）、什么
        条件下删除或接入、权威依据条款。」
      「无注释的死代码、被注释掉的旧逻辑、标注了废弃但没有原因与去向的代码，视为缺陷。」
  - ENGINEERING_SPEC.md §9（仓库整洁与治理工件清理）。
  - ENGINEERING_SPEC.md §10（机器一致性检查）：每项检查有正例与负例（能红能绿）；
    提供机器可执行负例入口（--self-test）；fail-closed；锚存活（硬编码引用的
    文件/目录必须存在，失效报 ANCHOR_STALE）。

统一注释块（唯一格式，逐字采用；C/C++ 用 //，Python 用 #）
  // ── RETIRED-CODE-RETAINED (ENGINEERING_SPEC §2 保留则注释) ─────────────
  // WHAT:      它是什么（一句话）
  // WHY-KEPT:  为什么保留
  // STATUS:    现状（未接入生产 / 仅供 XX 实验 / 非产品目标态）
  // EXIT:      什么条件下删除或接入（必须具体、可判定）
  // AUTHORITY: 权威依据条款（文档名 + 条款号）
  // ──────────────────────────────────────────────────────────────────────
  识别标记：同一注释块内必须同时出现 RETIRED-CODE-RETAINED、WHAT:、WHY-KEPT:、
  STATUS:、EXIT:、AUTHORITY: 六个 token（大小写敏感），且各字段非空
  （冒号后至少 2 个非空白字符）。

────────────────────────────────────────────────────────────────────────────
判据（fail-closed；rc: 0 PASS / 1 违规 / 2 输入不可用）
────────────────────────────────────────────────────────────────────────────
R1 被注释掉的旧逻辑块：扫描面内连续 >=3 行注释，其中 >=2 行含代码样 token
   （; / { / } / if ( / for ( / return / = 赋值 / () 调用）⇒ 违规；
   除非该注释块内含 RETIRED-CODE-RETAINED（即属于保留件注释块）。
R2 文件级退役横幅无注释块（父代理 CLEAN-401 修订口径）：仅当文件**前 40 行内**
   出现**显式横幅 token**（逐字、大小写敏感；大写 ASCII 或中文全词）之一：
   DEPRECATED / RETIRED / LEGACY / OBSOLETE / SUPERSEDED / 已退役 / 已作废 /
   遗留实现 / 未接入生产 / 死代码，且该文件不含 RETIRED-CODE-RETAINED 块
   ⇒ 违规（每条给出 文件:行 + 命中词）。正文散文里的 "v6"/"legacy" 等小写提及
   不判红（实测：大小写不敏感的全文匹配会把 62% 的在役文件判红 = 恒红门）。
R3 保留块字段不全：出现 RETIRED-CODE-RETAINED 但同一注释块内 6 个 token 不全
   或字段空（冒号后 < 2 个非空白字符）⇒ 违规。
R4 生产可达性报表（只报告、不判红）：lib/**/*.cpp（排除 third_party/ archive/
   legacy/ eng/tests/ test/ tools/ oracle/ aio/**）中，文件名未出现在任何
   CMakeLists.txt 的文件清单 + 计数 → JSON 字段 unreferenced_sources /
   unreferenced_sources_count，stdout 打印前 20 条。R4 不参与 rc。
R5 锚存活：硬编码扫描根 lib/ eng/tests/ eng/ci/ 任一不存在 ⇒ rc=2 ANCHOR_STALE
   （fail-closed；与 §10「锚存活」一致）。
扫描面（默认）：lib/**/*.{cpp,h,hpp,cc,c,py} + eng/tests/**/*.{cpp,h,hpp,cc,c,py}
                + eng/ci/**/*.py。
排除：lib/infrastructure/aio/**（他域，白名单并打印计数）、third_party/、build/、
      run/、任何 */oracle/* 与 *_oracle.py（独立 Oracle 允许自带历史对照）、
      以及本检查器自身（eng/ci/check_retired_code.py）。
allowlist：--allowlist <file>（JSON 数组，元素为相对路径 glob）；默认
      eng/ci/retired_code_allowlist.json，不存在时按空表并打印提示，不判红；
      allowlist 只抑制 R2（"标记提及"），不抑制 R1/R3（硬缺陷），不做静默放宽。
      显式传入的 --allowlist 路径不存在 ⇒ rc=2（fail-closed）。

用法
  python3 eng/ci/check_retired_code.py                   # 扫真实仓库
  python3 eng/ci/check_retired_code.py --json-out run/CLEAN-401/retired_code/baseline_before.json
  python3 eng/ci/check_retired_code.py --quiet               # 只打印结论行
  python3 eng/ci/check_retired_code.py --root <repo>         # 指向其它仓库根
  python3 eng/ci/check_retired_code.py --allowlist <file>    # 显式 allowlist
  python3 eng/ci/check_retired_code.py --self-test           # tempfile 正例+负例（能红能绿）

退出码：0 PASS；1 FAIL（R1/R2/R3 命中；--self-test 有失败用例）；2 输入不可用
（ANCHOR_STALE / allowlist 非法 / 文件不可读；fail-closed）。
"""
from __future__ import annotations

import argparse
import contextlib
import fnmatch
import io
import json
import os
import re
import sys
import tempfile
from pathlib import Path, PurePosixPath

CHECK_ID = "CHK-RETIRED-CODE"

# ── 统一注释块（唯一格式） ──────────────────────────────────────────────────
MARKER = "RETIRED-CODE-RETAINED"
FIELD_TOKENS = ("WHAT:", "WHY-KEPT:", "STATUS:", "EXIT:", "AUTHORITY:")
REQUIRED_TOKENS = (MARKER,) + FIELD_TOKENS
MIN_FIELD_CHARS = 2

# ── 扫描面 / 锚 ─────────────────────────────────────────────────────────────
SCAN_ROOTS = ("lib", "tests", "eng/tests", "eng/ci")   # 候选扫描根（存在的才扫）
# 2026-09-21 根目录整合：ci/ → eng/ci/；二次整合：tests/ → eng/tests/。
# 同时登记新旧候选根，避免改名后静默漏扫；R5 的 fail-closed 语义由
# REQUIRED_ROOTS + REQUIRED_TEST_ROOTS 承担（缺一即 ANCHOR_STALE rc=2）。
REQUIRED_ROOTS = ("lib", "eng/ci")            # 必需锚：缺一即 rc=2
REQUIRED_TEST_ROOTS = ("tests", "eng/tests")  # 测试面锚：全缺即 rc=2
SRC_SUFFIXES = (".cpp", ".h", ".hpp", ".cc", ".c", ".py")
PRUNE_DIRS = ("build", "run", ".git", "__pycache__", "node_modules",
              ".venv", "third_party")
AIO_WHITELIST_PREFIX = "lib/infrastructure/aio/"
SELF_REL = "eng/ci/check_retired_code.py"
DEFAULT_ALLOWLIST_REL = "eng/ci/retired_code_allowlist.json"

# ── R2：文件级退役横幅（显式 token，逐字匹配；仅看文件前 BANNER_WINDOW 行） ──
BANNER_WINDOW = 40
BANNER_TOKENS = ("DEPRECATED", "RETIRED", "LEGACY", "OBSOLETE", "SUPERSEDED",
                 "已退役", "已作废", "遗留实现", "未接入生产", "死代码")
# CLEAN-401 校准（2026-09-21，父代理裁决）：横幅必须是**注释行上的标签**，不是散文提及。
# 依据：token-anywhere 口径在真实仓库得 44 命中，抽样 25 条全部是散文（如
# "# ROOT-008: …（旧 cli/ 已退役）"、"// - --inspect <hiss>: LEGACY 诊断"、
# 'self.assertNotIn("/archive/", … "archive 死代码不入清单")'）⇒ 属假阳性。
# 收窄为：① 只看**注释行**（// # /* * -- ! 起首）；② 去装饰（⚠ ! * - – — > # / | : ： 空白）后
# 以 token 起首，或形如「状态: TOKEN」。这样 "// DEPRECATED: B4-01 去重 shim"、
# "// ⚠ **状态: RETIRED（…）**" 命中，而散文提及不命中。
BANNER_RE = re.compile("|".join(re.escape(tok) for tok in BANNER_TOKENS))
_COMMENT_LINE_RE = re.compile(r"^\s*(?://+|#+|/\*+|\*|--|!)\s*(.*)$")
_BANNER_DECOR_RE = re.compile(r"^[\s\*⚠!\-–—>#/|]+")
BANNER_LABEL_RE = re.compile(
    r"^(?:状态\s*[:：]\s*)?[\s\*⚠!\-–—>#/|]*(?:%s)(?![0-9A-Za-z_])"
    % "|".join(re.escape(tok) for tok in BANNER_TOKENS))

# ── R4：生产可达性报表（只报告，不判红） ────────────────────────────────────
PROD_ROOT = "lib"
PROD_SUFFIX = ".cpp"
PROD_EXCLUDE_PARTS = ("third_party", "archive", "legacy", "tests", "test",
                      "tools", "oracle", "aio")
CMAKE_NAME = "CMakeLists.txt"

# ── R1：代码样 token（注释文本内匹配；>=2 行命中即算"旧逻辑块"） ────────────
# 判据给出的每个 token 都在此逐条对应，但要求它出现在"代码位"，否则中文/英文
# 说明文字里的分号、等号、括号会被误判成被注释掉的旧逻辑（实测：纯散文注释会
# 让 62% 的扫描文件变红，见 run/CLEAN-401/retired_code/README.md 校准记录）：
#   semicolon 语句终止符（行尾 ; 或 ; 后只余行内注释）
#   brace     块边界（行尾 { / 整行 } 或 }; / } else）
#   control   语句关键字位于行首且行尾为 ) { ; }
#   return    行首 return 且以 ; 结束
#   assign    行首左值（可带成员/下标）后接 = 赋值
#   call      整行就是一次调用 foo(...) 或 foo(...);
CODE_TOKEN_RES = (
    ("semicolon", re.compile(r";\s*(?://.*)?(?:\*/)?\s*$")),
    ("brace", re.compile(r"\{\s*(?://.*)?\s*$|^\s*\}\s*;?\s*$|^\s*\}\s*else\b")),
    ("control", re.compile(r"^\s*(?:if|for|while|switch|catch|do|else)\b.*[){;}]\s*$")),
    ("return", re.compile(r"^\s*return\b.*;\s*$")),
    ("assign", re.compile(r"^\s*[A-Za-z_][\w\.\[\]\->]*\s*(?:[-+*/%&|^]|<<|>>)?=(?!=)\s*\S")),
    ("call", re.compile(r"^\s*[A-Za-z_][\w\.\->:]*\s*\([^()]*\)\s*;?\s*$")),
)
MIN_COMMENT_RUN = 3      # 连续 >=3 行注释
MIN_CODE_LINES = 2       # 其中 >=2 行含代码样 token
# 分诊提示（**非判据**、不参与 rc）：brace/control/return/call = 强代码形状；
# 只有 assign/semicolon = 弱形状（公式/字段说明等文档注释也长这样，需人工甄别）。
R1_STRONG_TOKENS = ("brace", "control", "return", "call")
# CLEAN-401 校准（2026-09-21，父代理裁决）：R1 判红要求该注释块**至少 1 个强代码形状 token**。
# 依据：真实仓库基线实测 R1=347 块，其中仅 17 块含强形状；其余 330 块只由 assign/semicolon
# 触发，抽样确认是文档注释里的公式/字段说明（如 "// z_i = (v_i − center_i) / sigma_i"、
# "* module_id = astrocs.p1.calibration"），属散文假阳性 —— 让它们判红会把本门变成恒红门，
# 失去证据资格。校准**不放宽** token 清单与阈值（MIN_COMMENT_RUN/MIN_CODE_LINES 未动），
# 只是把"代码形状"从分诊提示提升为判据；weak 命中仍进 JSON（r1_weak_blocks）供人工分诊。
R1_REQUIRE_STRONG = True
CONSOLE_TOP_FILES = 40   # 控制台按文件聚合打印前 N


class InputError(Exception):
    """输入不可用（锚缺失/非法 allowlist/不可读文件）—— fail-closed，映射 rc=2。"""


# ────────────────────────────────────────────────────────────────────────────
# 注释行分类（跨行块注释状态机；字符串内的注释符不计）
# ────────────────────────────────────────────────────────────────────────────
def _classify_c_like(text: str):
    """C/C++：// 行注释 + /* */ 块注释；返回 [(lineno, is_comment, comment_text)]。"""
    out = []
    in_block = False
    for lineno, raw in enumerate(text.splitlines(), start=1):
        i, n = 0, len(raw)
        comment_start = None
        code_seen = False
        while i < n:
            if in_block:
                end = raw.find("*/", i)
                if comment_start is None:
                    comment_start = i
                if end < 0:
                    i = n
                else:
                    in_block = False
                    i = end + 2
                continue
            ch = raw[i]
            if ch in "\"'":                       # 字符串/字符字面量：整体跳过
                quote, i = ch, i + 1
                while i < n:
                    if raw[i] == "\\":
                        i += 2
                        continue
                    if raw[i] == quote:
                        i += 1
                        break
                    i += 1
                code_seen = True
                continue
            if raw.startswith("//", i):
                if comment_start is None:
                    comment_start = i
                i = n
                continue
            if raw.startswith("/*", i):
                if comment_start is None:
                    comment_start = i
                in_block = True
                i += 2
                continue
            if not ch.isspace():
                code_seen = True
            i += 1
        if comment_start is not None and not code_seen:
            out.append((lineno, True, raw[comment_start:]))
        else:
            out.append((lineno, False, ""))
    return out


def _classify_python(text: str):
    """Python：# 行注释；三引号字符串与普通字符串内的 # 不计。"""
    out = []
    triple = None
    for lineno, raw in enumerate(text.splitlines(), start=1):
        i, n = 0, len(raw)
        comment_start = None
        # 多行字符串的续行是"字符串内容"（代码），不是注释 —— 不得计入注释块。
        code_seen = triple is not None
        while i < n:
            if triple is not None:
                end = raw.find(triple, i)
                if end < 0:
                    i = n
                else:
                    triple = None
                    i = end + 3
                continue
            ch = raw[i]
            if raw.startswith("'''", i) or raw.startswith('"""', i):
                if comment_start is None:
                    comment_start = i
                triple = raw[i:i + 3]
                i += 3
                continue
            if ch == "#":
                if comment_start is None:
                    comment_start = i
                i = n
                continue
            if ch in "\"'":
                quote, i = ch, i + 1
                while i < n:
                    if raw[i] == "\\":
                        i += 2
                        continue
                    if raw[i] == quote:
                        i += 1
                        break
                    i += 1
                code_seen = True
                continue
            if not ch.isspace():
                code_seen = True
            i += 1
        if comment_start is not None and not code_seen:
            out.append((lineno, True, raw[comment_start:]))
        else:
            out.append((lineno, False, ""))
    return out


def classify_lines(text: str, suffix: str):
    return _classify_python(text) if suffix == ".py" else _classify_c_like(text)


def comment_runs(lines):
    """把连续行号的注释行聚成"注释块"；非注释行/空行断开。"""
    runs, cur = [], []
    for lineno, is_comment, ctext in lines:
        if is_comment:
            if cur and lineno != cur[-1][0] + 1:
                runs.append(cur)
                cur = []
            cur.append((lineno, ctext))
        elif cur:
            runs.append(cur)
            cur = []
    if cur:
        runs.append(cur)
    return runs


def comment_body(comment_text: str) -> str:
    """剥掉行首注释符（// /* * #）与空白，得到注释正文（供代码位匹配）。"""
    body = comment_text
    for prefix in ("//", "/*", "*/", "*", "#"):
        if body.lstrip().startswith(prefix):
            body = body.lstrip()[len(prefix):]
            break
    return body


def code_token_hits(comment_text: str):
    body = comment_body(comment_text)
    return [name for name, rx in CODE_TOKEN_RES if rx.search(body)]


# ────────────────────────────────────────────────────────────────────────────
# allowlist / 文件收集
# ────────────────────────────────────────────────────────────────────────────
def load_allowlist(root: Path, arg):
    """返回 (patterns, info)。默认文件缺失 ⇒ 空表 + 提示（不判红）；显式缺失 ⇒ rc=2。"""
    if arg:
        path = Path(arg)
        if not path.is_absolute():
            path = root / path
        explicit = True
    else:
        path = root / DEFAULT_ALLOWLIST_REL
        explicit = False
    if not path.is_file():
        if explicit:
            raise InputError("ANCHOR_STALE: 显式 --allowlist 不存在: %s" % path)
        return [], {"path": str(path), "exists": False, "patterns": [],
                    "note": "默认 allowlist 不存在 → 按空表（不判红）；"
                            "如需登记合法的退役标记提及，请创建 %s" % DEFAULT_ALLOWLIST_REL}
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001 - 解析失败 fail-closed
        raise InputError("ALLOWLIST_UNPARSABLE: %s: %s" % (path, exc))
    # CLEAN-401：接受两种形态 —— ①JSON 数组（非空相对路径 glob 字符串）；
    # ②带台账头的对象 {"entries":[{"pattern":...,"rule":"R1|R2","reason":...}]}，
    # 使每条豁免都必须写明理由（ENGINEERING_SPEC §10「豁免显式登记且只减不增」）。
    reasons = {}
    if isinstance(doc, dict):
        raw_entries = doc.get("entries")
        if not isinstance(raw_entries, list) or not raw_entries:
            raise InputError(
                "ALLOWLIST_INVALID: %s（对象形态必须含非空 entries 数组）" % path)
        patterns = []
        for ent in raw_entries:
            if not isinstance(ent, dict):
                raise InputError("ALLOWLIST_INVALID: %s（entries 元素必须是对象）" % path)
            pat = ent.get("pattern")
            rsn = ent.get("reason")
            if not (isinstance(pat, str) and pat.strip()):
                raise InputError("ALLOWLIST_INVALID: %s（entry.pattern 必须是非空字符串）" % path)
            if not (isinstance(rsn, str) and len(rsn.strip()) >= 8):
                raise InputError(
                    "ALLOWLIST_INVALID: %s（entry.reason 必须写明理由，≥8 字符）: %s"
                    % (path, pat))
            patterns.append(pat.strip())
            reasons[pat.strip()] = rsn.strip()
    elif isinstance(doc, list) and all(
            isinstance(item, str) and item.strip() for item in doc):
        patterns = [item.strip() for item in doc]
    else:
        raise InputError(
            "ALLOWLIST_INVALID: %s（需为 JSON 数组或含 entries 的对象；"
            "元素为非空相对路径 glob 字符串）" % path)
    return patterns, {"path": str(path), "exists": True, "patterns": patterns,
                      "reasons": reasons}


def allowlisted(rel: str, patterns) -> bool:
    for pat in patterns:
        if rel == pat or fnmatch.fnmatchcase(rel, pat) or PurePosixPath(rel).match(pat):
            return True
    return False


def collect_files(root: Path):
    """返回 (files, stats)；files = [(abs_path, rel_posix)]，确定性排序。"""
    files, stats = [], {"aio_whitelist": 0, "oracle": 0, "self": 0,
                        "pruned_dirs": list(PRUNE_DIRS)}
    for scan_root in SCAN_ROOTS:
        base = root / scan_root
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = sorted(d for d in dirnames if d not in PRUNE_DIRS)
            for fn in sorted(filenames):
                if not fn.endswith(SRC_SUFFIXES):
                    continue
                path = Path(dirpath) / fn
                rel = path.relative_to(root).as_posix()
                if rel == SELF_REL:
                    stats["self"] += 1
                    continue
                if rel.startswith(AIO_WHITELIST_PREFIX):
                    stats["aio_whitelist"] += 1
                    continue
                parts = rel.split("/")[:-1]
                if "oracle" in parts or fn.endswith("_oracle.py"):
                    stats["oracle"] += 1
                    continue
                files.append((path, rel))
    files.sort(key=lambda item: item[1])
    return files, stats


# ────────────────────────────────────────────────────────────────────────────
# 判据主体
# ────────────────────────────────────────────────────────────────────────────
def evaluate(root: Path, allowlist_arg=None):
    """返回 (findings, extra)；输入不可用抛 InputError（rc=2）。"""
    root = Path(root)
    if not root.is_dir():
        raise InputError("ANCHOR_STALE: --root 不存在或不是目录: %s" % root)
    missing = [name for name in REQUIRED_ROOTS if not (root / name).is_dir()]
    if not any((root / name).is_dir() for name in REQUIRED_TEST_ROOTS):
        missing.append("(%s 全缺)" % "|".join(REQUIRED_TEST_ROOTS))
    if missing:
        raise InputError(
            "ANCHOR_STALE: 必需扫描根不存在: %s（在 %s 下）"
            % (", ".join(missing), root))

    patterns, allow_info = load_allowlist(root, allowlist_arg)
    files, stats = collect_files(root)

    findings = []
    suppressed = 0
    weak_blocks = []   # R1 弱形状（只报告，见 R1_REQUIRE_STRONG 校准说明）
    for path, rel in files:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            raise InputError("ANCHOR_UNREADABLE: %s: %s" % (rel, exc))
        lines = classify_lines(text, path.suffix)
        runs = comment_runs(lines)
        has_marker = MARKER in text

        # R3：保留块字段不全（同一注释块内 6 token 齐全 + 字段非空）
        for run in runs:
            joined = "\n".join(ctext for _, ctext in run)
            if MARKER not in joined:
                continue
            missing_tokens = [tok for tok in REQUIRED_TOKENS if tok not in joined]
            empty_fields = []
            for lineno, ctext in run:
                for tok in FIELD_TOKENS:
                    pos = ctext.find(tok)
                    if pos < 0:
                        continue
                    value = ctext[pos + len(tok):].strip()
                    if len(re.sub(r"\s", "", value)) < MIN_FIELD_CHARS:
                        empty_fields.append("%s@%d" % (tok, lineno))
            if missing_tokens or empty_fields:
                findings.append({
                    "rule": "R3", "file": rel, "line": run[0][0],
                    "detail": "RETIRED-CODE-RETAINED 块字段不全：缺 token=%s；"
                              "空字段=%s" % (missing_tokens or "无", empty_fields or "无"),
                    "missing_tokens": missing_tokens, "empty_fields": empty_fields,
                })

        # R1：被注释掉的旧逻辑块（保留件注释块内的代码样注释豁免）
        for run in runs:
            joined = "\n".join(ctext for _, ctext in run)
            if MARKER in joined or len(run) < MIN_COMMENT_RUN:
                continue
            code_lines = [(lineno, ctext) for lineno, ctext in run
                          if code_token_hits(ctext)]
            if len(code_lines) >= MIN_CODE_LINES:
                tokens = sorted({tok for _, ctext in code_lines
                                 for tok in code_token_hits(ctext)})
                if allowlisted(rel, patterns):
                    # CLEAN-401 校准：R1 假阳性（文档注释里的用法示例/公式/文件头说明块）
                    # 逐条登记在 eng/ci/retired_code_allowlist.json（每条带 reason，只减不增）。
                    suppressed += 1
                    continue
                if R1_REQUIRE_STRONG and not (set(tokens) & set(R1_STRONG_TOKENS)):
                    weak_blocks.append({
                        "file": rel, "line": run[0][0], "tokens": tokens,
                        "run_lines": len(run), "code_lines": len(code_lines),
                        "snippet": code_lines[0][1].strip()[:160],
                    })
                    continue
                findings.append({
                    "rule": "R1", "file": rel, "line": run[0][0],
                    "detail": "被注释掉的旧逻辑块：连续 %d 行注释中 %d 行含代码样 token %s"
                              % (len(run), len(code_lines), tokens),
                    "run_lines": len(run), "code_lines": len(code_lines),
                    "tokens": tokens,
                    "code_shape": ("strong" if set(tokens) & set(R1_STRONG_TOKENS)
                                   else "weak"),
                    "snippet": code_lines[0][1].strip()[:160],
                })

        # R2：文件级退役横幅无注释块（前 BANNER_WINDOW 行；allowlist 只抑制本规则）
        if not has_marker:
            for lineno, raw in enumerate(text.splitlines()[:BANNER_WINDOW], start=1):
                cm = _COMMENT_LINE_RE.match(raw)
                if not cm:
                    continue                      # 非注释行（代码/字符串）不判
                body = cm.group(1)
                lm = BANNER_LABEL_RE.match(body)
                if not lm:
                    continue                      # 散文提及不算横幅
                word = lm.group(0)
                for tok in BANNER_TOKENS:
                    if tok in word:
                        word = tok
                        break
                if allowlisted(rel, patterns):
                    suppressed += 1
                    continue
                findings.append({
                    "rule": "R2", "file": rel, "line": lineno, "word": word,
                    "detail": "文件级退役横幅 \"%s\"（前 %d 行内，注释标签位）但文件无 "
                              "RETIRED-CODE-RETAINED 块" % (word, BANNER_WINDOW),
                    "snippet": raw.strip()[:160],
                })

    findings.sort(key=lambda f: (f["file"], f["line"], f["rule"]))
    rules = {"R1": 0, "R2": 0, "R3": 0}
    shape = {"strong": 0, "weak": 0}
    for item in findings:
        rules[item["rule"]] += 1
        if item["rule"] == "R1":
            shape[item["code_shape"]] += 1
    by_file = {}
    for item in findings:
        entry = by_file.setdefault(item["file"], {"file": item["file"], "count": 0,
                                                  "rules": set()})
        entry["count"] += 1
        entry["rules"].add(item["rule"])
    files_by_count = sorted(
        ({"file": e["file"], "count": e["count"], "rules": sorted(e["rules"])}
         for e in by_file.values()),
        key=lambda e: (-e["count"], e["file"]))
    by_rule = {"R1": {}, "R2": {}, "R3": {}}
    for item in findings:
        by_rule[item["rule"]][item["file"]] = by_rule[item["rule"]].get(item["file"], 0) + 1
    files_by_rule = {
        rule: [{"file": name, "count": count}
               for name, count in sorted(counter.items(), key=lambda kv: (-kv[1], kv[0]))]
        for rule, counter in by_rule.items()}
    unreferenced, prod_scan = production_reachability(root)
    extra = {
        "root": str(root),
        "rules": rules,
        "files_by_count": files_by_count,
        "files_by_rule": files_by_rule,
        "scan": {"files_scanned": len(files), "excluded": stats,
                 "scan_roots": list(SCAN_ROOTS), "suffixes": list(SRC_SUFFIXES)},
        "allowlist": dict(allow_info, suppressed=suppressed),
        "anchor_roots": list(SCAN_ROOTS),
        "r1_shape_hint": shape,
        "r1_weak_blocks": weak_blocks,
        "r1_weak_blocks_count": len(weak_blocks),
        "unreferenced_sources": unreferenced,
        "unreferenced_sources_count": len(unreferenced),
        "production_scan": prod_scan,
    }
    return findings, extra


def production_reachability(root: Path):
    """R4：lib/**/*.cpp 中文件名未出现在任何 CMakeLists.txt 的清单（只报告，不判红）。"""
    cmake_texts = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if d not in PRUNE_DIRS)
        if CMAKE_NAME in filenames:
            try:
                cmake_texts.append(
                    (Path(dirpath) / CMAKE_NAME).read_text(encoding="utf-8",
                                                           errors="replace"))
            except OSError as exc:
                raise InputError("ANCHOR_UNREADABLE: %s: %s"
                                 % (Path(dirpath) / CMAKE_NAME, exc))
    joined = "\n".join(cmake_texts)
    candidates, unreferenced = 0, []
    base = root / PROD_ROOT
    for dirpath, dirnames, filenames in os.walk(base):
        dirnames[:] = sorted(d for d in dirnames if d not in PRUNE_DIRS)
        for fn in sorted(filenames):
            if not fn.endswith(PROD_SUFFIX):
                continue
            path = Path(dirpath) / fn
            rel = path.relative_to(root).as_posix()
            parts = rel.split("/")[:-1]
            if any(part in PROD_EXCLUDE_PARTS for part in parts):
                continue
            candidates += 1
            if fn not in joined:
                unreferenced.append(rel)
    unreferenced.sort()
    return unreferenced, {"candidates_scanned": candidates,
                          "cmake_files_scanned": len(cmake_texts),
                          "excluded_parts": list(PROD_EXCLUDE_PARTS)}


# ────────────────────────────────────────────────────────────────────────────
# 输出
# ────────────────────────────────────────────────────────────────────────────
def write_json_atomic(path, doc):
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    tmp = target.with_name(target.name + ".tmp")
    tmp.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n",
                   encoding="utf-8")
    os.replace(tmp, target)


def print_report(findings, extra, quiet=False):
    rules = extra["rules"]
    print("%s_FAIL: %d finding(s) [R1=%d R2=%d R3=%d]"
          % (CHECK_ID, len(findings), rules["R1"], rules["R2"], rules["R3"]))
    scan = extra["scan"]
    print("  扫描面: files=%d excluded=%s" % (scan["files_scanned"],
                                             json.dumps(scan["excluded"], ensure_ascii=False)))
    allow = extra["allowlist"]
    if allow.get("exists"):
        print("  allowlist: %s patterns=%d suppressed=%d"
              % (allow["path"], len(allow["patterns"]), allow["suppressed"]))
    else:
        print("  allowlist: %s（不存在 → 空表；suppressed=0）" % allow.get("note", ""))
    print("  R1 分诊提示（非判据）: strong=%d weak=%d"
          % (extra["r1_shape_hint"]["strong"], extra["r1_shape_hint"]["weak"]))
    print("  R4 生产可达性（只报告不判红）: lib/**/*.cpp 候选=%d，"
          "CMakeLists.txt=%d，未被任何 CMakeLists.txt 引用=%d"
          % (extra["production_scan"]["candidates_scanned"],
             extra["production_scan"]["cmake_files_scanned"],
             extra["unreferenced_sources_count"]))
    if quiet:
        return
    for rel in extra["unreferenced_sources"][:20]:
        print("    R4 未引用: %s" % rel)
    if extra["unreferenced_sources_count"] > 20:
        print("    ... (R4 其余 %d 条见 JSON unreferenced_sources)"
              % (extra["unreferenced_sources_count"] - 20))
    top = extra["files_by_count"][:CONSOLE_TOP_FILES]
    print("  ── 命中文件聚合（按命中数降序，前 %d / 共 %d 个文件） ──"
          % (len(top), len(extra["files_by_count"])))
    for entry in top:
        print("  %6d  %s  [%s]" % (entry["count"], entry["file"], ",".join(entry["rules"])))
    for rule in ("R1", "R2", "R3"):
        entries = extra["files_by_rule"][rule]
        print("  ── %s 命中文件（前 %d / 共 %d，按命中数降序） ──"
              % (rule, min(15, len(entries)), len(entries)))
        for entry in entries[:15]:
            print("  %6d  %s" % (entry["count"], entry["file"]))
    print("  ── findings（前 80 / 共 %d） ──" % len(findings))
    for item in findings[:80]:
        print("  %s %s:%d %s | %s" % (item["rule"], item["file"], item["line"],
                                      item["detail"], item.get("snippet", "")))
    if len(findings) > 80:
        print("  ... (%d more)" % (len(findings) - 80))


# ────────────────────────────────────────────────────────────────────────────
# --self-test：tempfile 夹具（不依赖仓库当前内容），逐条 [SELFTEST] ... PASS/FAIL
# ────────────────────────────────────────────────────────────────────────────
FULL_BLOCK = """\
// ── RETIRED-CODE-RETAINED (ENGINEERING_SPEC §2 保留则注释) ─────────────
// WHAT:      旧版求和的标量循环实现（已被向量化实现取代）
// WHY-KEPT:  仅供 SCI-A 数值对照实验，作为 legacy 参照路径保留
// STATUS:    未接入生产（生产走 vector_sum）
// EXIT:      SCI-A 对照实验报告归档后删除；或接入前必须补齐 Oracle 用例
// AUTHORITY: ENGINEERING_SPEC.md §2
// ──────────────────────────────────────────────────────────────────────
// 块内允许保留的旧逻辑注释（属于保留件注释块，R1 豁免）：
//   scalar_sum(a, b);
//   total = total + 1;
int scalar_sum(int a, int b) { return a + b; }
"""


def _fixture(root: Path, rel: str, body: str):
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(body, encoding="utf-8")


def _case_green_full_block(root: Path):
    for name in SCAN_ROOTS:
        (root / name).mkdir(parents=True, exist_ok=True)
    _fixture(root, "lib/good/retained.cpp", FULL_BLOCK)
    _fixture(root, "eng/ci/good_tool.py", "VALUE = 1\n")


def _case_green_short_run(root: Path):
    for name in SCAN_ROOTS:
        (root / name).mkdir(parents=True, exist_ok=True)
    _fixture(root, "lib/good/notes.cpp",
             "// total = total + 1;\n"
             "// scalar_sum(a, b);\n"
             "int f() { return 1; }\n")


def _case_red_commented_out_logic(root: Path):
    for name in SCAN_ROOTS:
        (root / name).mkdir(parents=True, exist_ok=True)
    _fixture(root, "lib/bad/old_logic.cpp",
             "// void old_step(Frame& f) {\n"
             "//     f.total = f.total + 1;\n"
             "//     commit(f);\n"
             "// }\n"
             "int f() { return 1; }\n")


def _case_red_marker_without_block(root: Path):
    for name in SCAN_ROOTS:
        (root / name).mkdir(parents=True, exist_ok=True)
    _fixture(root, "lib/bad/marked.cpp",
             "// DEPRECATED: 旧路径（未接入生产）\n"
             "int f() { return 1; }\n")


def _case_red_block_missing_exit(root: Path):
    for name in SCAN_ROOTS:
        (root / name).mkdir(parents=True, exist_ok=True)
    _fixture(root, "lib/bad/incomplete.cpp",
             "// ── RETIRED-CODE-RETAINED (ENGINEERING_SPEC §2 保留则注释) ──\n"
             "// WHAT:      旧实现\n"
             "// WHY-KEPT:  参考\n"
             "// STATUS:    未接入生产\n"
             "// AUTHORITY: ENGINEERING_SPEC.md §2\n"
             "// ──────────────────────────────────────────────────────────\n"
             "int f() { return 1; }\n")


def _case_red_anchor_missing(root: Path):
    (root / "tests").mkdir(parents=True, exist_ok=True)
    (root / "eng/ci").mkdir(parents=True, exist_ok=True)
    _fixture(root, "eng/ci/tool.py", "VALUE = 1\n")


def _case_green_allowlisted_marker(root: Path):
    for name in SCAN_ROOTS:
        (root / name).mkdir(parents=True, exist_ok=True)
    _fixture(root, "lib/ok/legacy_note.cpp",
             "// LEGACY 名称保留：本文件是对照实现，无退役注释块需求（夹具）\n"
             "int f() { return 1; }\n")
    _fixture(root, DEFAULT_ALLOWLIST_REL, json.dumps(["lib/ok/legacy_note.cpp"]) + "\n")


def _case_green_lowercase_prose(root: Path):
    """修订 R2 的负向对照：散文里的小写 legacy/v6/obsolete 不构成退役横幅。"""
    for name in SCAN_ROOTS:
        (root / name).mkdir(parents=True, exist_ok=True)
    _fixture(root, "lib/ok/prose.cpp",
             "// 本文件属于 v6 现役链路；下方 legacy 命名是历史沿革说明，非退役横幅\n"
             "int f() { return 1; }\n")


def _case_r4_reachability(root: Path):
    """R4 只报告：未被任何 CMakeLists.txt 引用的 lib/**/*.cpp 必须进报表。"""
    for name in SCAN_ROOTS:
        (root / name).mkdir(parents=True, exist_ok=True)
    _fixture(root, "lib/prod/orphan.cpp", "int orphan() { return 0; }\n")
    _fixture(root, "lib/prod/wired.cpp", "int wired() { return 0; }\n")
    _fixture(root, "lib/prod/aio/skipped.cpp", "int skipped() { return 0; }\n")
    _fixture(root, "CMakeLists.txt", "add_library(prod lib/prod/wired.cpp)\n")


SELFTEST_CASES = (
    # 正例（必绿）：完整保留件注释块 / 未达阈值的短注释 / allowlist / 散文小写提及
    {"name": "green_full_retired_block", "build": _case_green_full_block,
     "rc": 0, "rules": [], "verdict": "PASS", "suppressed": 0, "unreferenced": None},
    {"name": "green_short_comment_run", "build": _case_green_short_run,
     "rc": 0, "rules": [], "verdict": "PASS", "suppressed": 0, "unreferenced": None},
    # allowlist 必须真的抑制掉命中（suppressed=1 而非"恰好没命中"），否则是假绿
    {"name": "green_allowlisted_marker", "build": _case_green_allowlisted_marker,
     "rc": 0, "rules": [], "verdict": "PASS", "suppressed": 1, "unreferenced": None},
    {"name": "green_lowercase_prose", "build": _case_green_lowercase_prose,
     "rc": 0, "rules": [], "verdict": "PASS", "suppressed": 0, "unreferenced": None},
    # 负例（必红）
    {"name": "red_commented_out_logic_R1", "build": _case_red_commented_out_logic,
     "rc": 1, "rules": ["R1"], "verdict": "FAIL", "suppressed": 0, "unreferenced": None},
    {"name": "red_banner_without_block_R2", "build": _case_red_marker_without_block,
     "rc": 1, "rules": ["R2"], "verdict": "FAIL", "suppressed": 0, "unreferenced": None},
    {"name": "red_block_missing_exit_R3", "build": _case_red_block_missing_exit,
     "rc": 1, "rules": ["R3"], "verdict": "FAIL", "suppressed": 0, "unreferenced": None},
    {"name": "red_anchor_missing_rc2", "build": _case_red_anchor_missing,
     "rc": 2, "rules": [], "verdict": "ERROR", "suppressed": 0, "unreferenced": None},
    # R4 报表（只报告不判红，但必须非空且排除 aio/**）
    {"name": "green_r4_reachability", "build": _case_r4_reachability,
     "rc": 0, "rules": [], "verdict": "PASS", "suppressed": 0,
     "unreferenced": ["lib/prod/orphan.cpp"]},
)


def _run_case(root: Path, json_out: Path):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(buf):
        rc = main(["--root", str(root), "--quiet", "--json-out", str(json_out)])
    doc = {}
    if json_out.is_file():
        try:
            doc = json.loads(json_out.read_text(encoding="utf-8"))
        except Exception:  # noqa: BLE001
            doc = {}
    return rc, doc, buf.getvalue()


def selftest() -> int:
    failures = []
    total = len(SELFTEST_CASES)
    with tempfile.TemporaryDirectory(prefix="chk_retired_code_selftest_") as td:
        base = Path(td)
        for index, case in enumerate(SELFTEST_CASES, start=1):
            name = case["name"]
            root = base / ("case_%d_%s" % (index, name))
            root.mkdir(parents=True, exist_ok=True)
            case["build"](root)
            rc, doc, captured = _run_case(root, base / ("out_%d.json" % index))
            got_rules = sorted(r for r, n in (doc.get("rules") or {}).items() if n)
            verdict = doc.get("verdict", "<no-json>")
            got_suppressed = (doc.get("allowlist") or {}).get("suppressed", 0)
            got_unref = doc.get("unreferenced_sources")
            problems = []
            if rc != case["rc"]:
                problems.append("rc expect=%d got=%d" % (case["rc"], rc))
            if got_rules != sorted(case["rules"]):
                problems.append("rules expect=%s got=%s"
                                % (sorted(case["rules"]), got_rules))
            if verdict != case["verdict"]:
                problems.append("verdict expect=%s got=%s" % (case["verdict"], verdict))
            if got_suppressed != case["suppressed"]:
                problems.append("suppressed expect=%d got=%s"
                                % (case["suppressed"], got_suppressed))
            if case["unreferenced"] is not None and got_unref != case["unreferenced"]:
                problems.append("unreferenced_sources expect=%s got=%s"
                                % (case["unreferenced"], got_unref))
            ok = not problems
            if not ok:
                failures.append("%s: %s；输出=%s"
                                % (name, "；".join(problems), captured.strip()[:400]))
            print("[SELFTEST] %d/%d %-28s expect=rc%d/%s got=rc%d/%s %s"
                  % (index, total, name, case["rc"],
                     ",".join(case["rules"]) or "-", rc,
                     ",".join(got_rules) or "-", "PASS" if ok else "FAIL"))
    if failures:
        print("[SELFTEST] FAIL: %d/%d case(s) 未达预期" % (len(failures), total))
        for item in failures:
            print("  - " + item)
        return 1
    print("[SELFTEST] PASS: %d/%d case(s)（正例必绿 + 负例必红，含 rc=2 ANCHOR_STALE）"
          % (total, total))
    return 0


# ────────────────────────────────────────────────────────────────────────────
def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="CHK-RETIRED-CODE：ENGINEERING_SPEC §2 历史实现处置机器判据")
    parser.add_argument("--root", default=str(Path(__file__).resolve().parent.parent.parent),
                        help="仓库根（默认本检查器所在仓库）")
    parser.add_argument("--json-out", default=None, help="机器可读结果输出路径（原子写）")
    parser.add_argument("--self-test", action="store_true", dest="self_test",
                        help="tempfile 夹具正例+负例（不依赖仓库当前内容）")
    parser.add_argument("--quiet", action="store_true", help="只打印结论行")
    parser.add_argument("--allowlist", default=None,
                        help="JSON 数组（相对路径 glob）；默认 %s" % DEFAULT_ALLOWLIST_REL)
    args = parser.parse_args(argv)

    if args.self_test:
        return selftest()

    try:
        findings, extra = evaluate(Path(args.root).resolve(), args.allowlist)
    except InputError as exc:
        doc = {"check_id": CHECK_ID, "verdict": "ERROR", "error": str(exc),
               "findings": [], "rules": {"R1": 0, "R2": 0, "R3": 0},
               "root": str(Path(args.root).resolve())}
        print("%s_ERROR(rc=2): %s" % (CHECK_ID, exc), file=sys.stderr)
        if args.json_out:
            try:
                write_json_atomic(args.json_out, doc)
            except OSError as write_exc:
                print("%s_ERROR: 无法写 --json-out: %s" % (CHECK_ID, write_exc),
                      file=sys.stderr)
        return 2

    if findings:
        print_report(findings, extra, quiet=args.quiet)
        if args.json_out:
            write_json_atomic(args.json_out, dict(
                {"check_id": CHECK_ID, "verdict": "FAIL", "findings": findings}, **extra))
        return 1

    print("%s_PASS files=%d excluded=%s R1=0 R2=0 R3=0 allowlist_suppressed=%d"
          % (CHECK_ID, extra["scan"]["files_scanned"],
             json.dumps(extra["scan"]["excluded"], ensure_ascii=False),
             extra["allowlist"]["suppressed"]))
    print("  R4 生产可达性（只报告不判红）: lib/**/*.cpp 候选=%d，未被任何 "
          "CMakeLists.txt 引用=%d（前 20 条）"
          % (extra["production_scan"]["candidates_scanned"],
             extra["unreferenced_sources_count"]))
    if not args.quiet:
        for rel in extra["unreferenced_sources"][:20]:
            print("    R4 未引用: %s" % rel)
    if args.json_out:
        # conclusion-anchor: 只有走到这里才是「零 findings」的成功路径（findings 已在上文
        # 逐规则算出）；verdict 是计算结果的字面编码，不是写死的结论。
        write_json_atomic(args.json_out, dict(
            {"check_id": CHECK_ID, "verdict": "PASS", "findings": []}, **extra))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
