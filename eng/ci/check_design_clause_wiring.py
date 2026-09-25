#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-DESIGN-CLAUSE-WIRING —— 最高设计条文 ↔ 生产实现 的映射完整性门。

防的复发缺口：**设计里写下的行为约束在生产中零实现，而没有任何门能发现这个差**。
现有门锚在「制品形态」上（声明有没有消费者、配置键有没有被读、有没有构建目标、
dlopen 宿主可不可达），而设计条文是自然语言 —— 门与规范之间没有映射。
本门补的就是这张映射：台账 eng/ci/ledgers/design_clauses.json 把
ASTROCS_DESIGN.md 中被监视标题范围内的**每一条 bullet** 逐条登记为一条 clause，
写明它对应哪一处生产实现（或如实标 unwired + 具名解除条件）。

判据（全部 fail-closed；每条都有 --self-test 负例）：
  C1 条文覆盖面：watched_sections 指定的标题范围内**每一条 bullet** 都必须有且只有
     一条 clause 条目；缺条目 ⇒ CLAUSE_MISSING；条目 sha 与范围不符 ⇒
     CLAUSE_TEXT_CHANGED（= 条文被改过而映射未复核）；anchor 行号漂移 ⇒
     CLAUSE_ANCHOR_DRIFT（并打印该 sha 的**实际**行号，便于重新锚定）。
  C2 生产锚存活：production_anchor 里每个 file:line 必须可解析、路径必须是仓库相对
     路径、行号必须在 [1, 文件行数] 内；文件被删或行号越界 ⇒ PRODUCTION_ANCHOR_*。
  C3 生产锚不得是测试：路径落在非生产面（eng/tests/**、任意 tests/ 或 test/
     组件、test_*.cpp / *_test.* 文件名、文件名含 oracle/selfcheck/selftest、
     testdata/、oracle/、fixtures/、archive/、legacy/、third_party/）⇒
     PRODUCTION_ANCHOR_NOT_PRODUCTION。
     这条正是为了堵住「测试自建对象所以永远绿」的失效形态。
     残留（已知、显式登记不掩盖）：C3 是**路径判据**，判不出「文件在盘上但从未
     进入任何生产 target / 生产二进制」这一类（例：lib/algorithms/projection/
     p3_projection.cpp 未出现在任何 CMake 源列表，其模块符号已被
     eng/ci/ledgers/dormant_algorithms.json 登记为不在生产链）。可达性由
     CHK-ALGO-WIRING / CHK-PROD-WIRING / dormant 台账负责；本门不重复造。
  C4 状态诚实：status 只能是 wired / unwired（其余 ⇒ CLAUSE_STATUS_UNKNOWN）。
     wired 必须**同时**有非测试 production_anchor 与 assertion，且不得再带
     reason/exit_condition（有东西要解释就不是 wired）；
     unwired 必须**同时**有 reason 与具名 exit_condition（含可核对的落点：路径、
     file:line、反引号标识符或 CHK-* 门号），且不得带 assertion，也不得带
     production_anchor（有生产锚就是在主张已接线；半接线把证据写进 reason）。
     两侧都禁「以后再说 / TODO / 待定 / 将来」这类无判据的话。
  C5 生产锚内容一致：每个 production_anchor 必须在 anchor_tokens 里给出该行上
     必须出现的字面 token；token 不在该行 ⇒ ANCHOR_TOKEN_MISMATCH（打印 token
     的**实际**行号作为重新锚定提示）。防止行号漂移后锚点静默指向别的语句。
  C6 生产锚必须受版本控制：仓库是 git work tree 时，production_anchor 指向的文件
     必须已被跟踪；未跟踪（= 在制、随时可能消失的草稿）⇒
     PRODUCTION_ANCHOR_UNTRACKED。非 work tree（自测夹具）时本判据显式登记为
     不适用，并在 PASS/FAIL 行与 JSON 里打印 tracked_check 状态，不静默跳过。
  C7 三态输出：PASS(0) / FAIL(1) / CRASH(2，独立退出码)。崩溃绝不计成 PASS，
     也不计成 FAIL —— 判词带文件名与行号、**全量打印不截断**。
  C8 同轮读数一致：一次 evaluate 内每个文件只从磁盘读一次（Snapshot 缓存 +
     读计数），所以同一轮内的两次读数不可能因并发改动而翻转；--self-test 逐例断言
     「每个路径恰好落盘读一次且缓存确实被复用」与「同轮两次读数逐字节相同」。

台账结构（eng/ci/ledgers/design_clauses.json）：
  { "ledger_schema": "astrocs.design-clause-wiring/v1",
    "watched_sections": [{"file","heading","bullet_scope_marker"?,"scope_note"?}],
    "clauses": [{"id","anchor","text_sha256","requirement","status",
                 "production_anchor":[], "anchor_tokens":{}, "assertion",
                 "reason","exit_condition"}] }
  规范化（与 text_sha256 的定义同源，禁止两处各写一套）：
    剥行内 markdown（链接取文字、`` __ ` * 与单词边界上的孤立 _）→ 空白折叠为单空格
    → 去首尾空白 → UTF-8 sha256。

用法：
  python3 eng/ci/check_design_clause_wiring.py [--repo ROOT] [--ledger PATH]
                                              [--json-out F] [--self-test]
exit 0 = 映射完整且诚实；exit 1 = 有 finding；exit 2 = 锚点/台账不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import contextlib
import hashlib
import io
import json
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gate_common as gc  # noqa: E402

CHECK_ID = "CHK-DESIGN-CLAUSE-WIRING"
LEDGER_REL = "eng/ci/ledgers/design_clauses.json"
LEDGER_SCHEMA = "astrocs.design-clause-wiring/v1"

STATUS_WIRED = "wired"
STATUS_UNWIRED = "unwired"

# C4：无判据的话一律不接受（wired 的 assertion 与 unwired 的 exit_condition 同面）。
VAGUE_PHRASES = (
    "以后再说", "以后在说", "待定", "未定", "将来", "后续再", "稍后", "todo",
    "tbd", "暂不", "再说", "视情况",
)
# C4：unwired 的 exit_condition 必须带一个可核对的落点。
NAMED_EXIT_HINT_RE = re.compile(
    r"`[^`]+`"                          # 反引号标识符
    r"|[A-Za-z0-9_./-]+\.[A-Za-z0-9]+:[0-9]+"    # file.ext:line
    r"|CHK-[A-Z0-9-]+"                          # 门号
    r"|§\s*[0-9]"                                # 条款引用
    r"|[A-Za-z0-9_./-]+/[A-Za-z0-9_./-]+"        # 仓库路径
)
MIN_TEXT = 16
MIN_ASSERTION = 24

# C3：非生产面（不在这些路径上的文件才算「生产实现」）。
NON_PRODUCTION_COMPONENTS = frozenset((
    "tests", "test", "testdata", "oracle", "oracles", "fixtures", "fixture",
    "archive", "legacy", "third_party", "__pycache__",
))
# 文件名形态：test_* / *_test.*（任务点名）+ 名字里带 oracle/selfcheck 的
# 自证/对拍件（与 oracle/ 组件同面：它们是验证件，不是生产实现）。
NON_PRODUCTION_BASENAME_RE = re.compile(
    r"^(?:test_[^/]*|[^/]*_test)\.(?:cpp|cc|cxx|c|h|hpp|py)$"
)
NON_PRODUCTION_NAME_HINT_RE = re.compile(r"oracle|selfcheck|self_check|selftest|self_test")

# ── 规范化：本模块是 text_sha256 的唯一定义处 ─────────────────────────────
_LINK_RE = re.compile(r"\[([^\]]*)\]\([^)]*\)")
_WS_RE = re.compile(r"\s+")
_LONE_UNDERSCORE_RE = re.compile(r"(?<![0-9A-Za-z])_(?![0-9A-Za-z])")


def normalize_clause(text: str) -> str:
    """条文规范化：剥行内 markdown 标记 → 折叠空白 → 去首尾空白。"""
    t = _LINK_RE.sub(r"\1", text)
    t = t.replace("**", "").replace("__", "").replace("`", "")
    t = _LONE_UNDERSCORE_RE.sub("", t)
    t = t.replace("*", "")
    t = _WS_RE.sub(" ", t)
    return t.strip()


def clause_sha256(text: str) -> str:
    return hashlib.sha256(normalize_clause(text).encode("utf-8")).hexdigest()


def norm_rel(path: str) -> str:
    return path.strip().replace("\\", "/").lstrip("./")


# ── 快照：一轮内每个文件只落盘读一次（C8） ───────────────────────────────
class Snapshot:
    """一次 evaluate 的文件读取面：读一次、缓存、分别记「请求次数/落盘次数」。"""

    def __init__(self, repo: pathlib.Path) -> None:
        self.repo = pathlib.Path(repo)
        self._text: dict = {}
        self.reads: dict = {}
        self.disk_reads: dict = {}

    def text(self, rel: str) -> str:
        rel = norm_rel(rel)
        self.reads[rel] = self.reads.get(rel, 0) + 1
        if rel not in self._text:
            path = self.repo / rel
            if not path.is_file():
                raise gc.GateError("ANCHOR_MISSING: %s" % rel)
            try:
                body = path.read_text(encoding="utf-8", errors="replace")
            except OSError as exc:  # pragma: no cover - 权限/IO 异常
                raise gc.GateError("ANCHOR_UNREADABLE: %s: %s" % (rel, exc))
            self._text[rel] = body
            self.disk_reads[rel] = self.disk_reads.get(rel, 0) + 1
        return self._text[rel]

    def lines(self, rel: str):
        return self.text(rel).splitlines()


def is_production_path(rel: str):
    """C3：返回 (是否生产面, 命中的非生产理由)。"""
    parts = [p for p in norm_rel(rel).split("/") if p]
    if not parts:
        return False, "空路径"
    for part in parts[:-1]:
        if part in NON_PRODUCTION_COMPONENTS:
            return False, "路径含非生产组件 '%s/'" % part
    base = parts[-1]
    if base in NON_PRODUCTION_COMPONENTS:
        return False, "路径含非生产组件 '%s/'" % base
    if NON_PRODUCTION_BASENAME_RE.match(base):
        return False, "文件名为测试形态 '%s'" % base
    if NON_PRODUCTION_NAME_HINT_RE.search(base):
        return False, "文件名含验证件字样 '%s'（oracle/selfcheck 属验证面）" % base
    return True, ""


# ── bullet 扫描 ───────────────────────────────────────────────────────────
_HEAD_RE = re.compile(r"^(#{1,6})\s+(.*?)\s*$")
_BULLET_RE = re.compile(r"^(\s*)[-*+]\s+(.*)$")


class Bullet:
    __slots__ = ("line", "text", "sha256", "norm")

    def __init__(self, line: int, text: str) -> None:
        self.line = line
        self.text = text
        self.norm = normalize_clause(text)
        self.sha256 = clause_sha256(text)

    def extend(self, extra: str) -> None:
        self.text = self.text + " " + extra
        self.norm = normalize_clause(self.text)
        self.sha256 = clause_sha256(self.text)


def _parse_heading_spec(spec: str):
    s = (spec or "").strip()
    m = _HEAD_RE.match(s) if s.startswith("#") else None
    if m:
        return len(m.group(1)), _WS_RE.sub(" ", m.group(2).strip())
    return None, _WS_RE.sub(" ", s)


def locate_heading(lines, spec: str) -> int:
    want_level, want_text = _parse_heading_spec(spec)
    hits = []
    for i, raw in enumerate(lines):
        m = _HEAD_RE.match(raw)
        if not m:
            continue
        if _WS_RE.sub(" ", m.group(2).strip()) != want_text:
            continue
        if want_level is not None and len(m.group(1)) != want_level:
            continue
        hits.append(i)
    if not hits:
        raise gc.GateError("SECTION_HEADING_MISSING: 设计文档内找不到标题 %r" % spec)
    if len(hits) > 1:
        raise gc.GateError("SECTION_HEADING_AMBIGUOUS: 标题 %r 命中 %d 处" % (spec, len(hits)))
    return hits[0]


def scan_scope(lines, heading: str, marker: str = ""):
    """标题范围内（可选 marker 之后起）逐条 bullet（含多行续行合并）。"""
    hi = locate_heading(lines, heading)
    level = len(_HEAD_RE.match(lines[hi]).group(1))
    end = len(lines)
    for j in range(hi + 1, len(lines)):
        m = _HEAD_RE.match(lines[j])
        if m and len(m.group(1)) <= level:
            end = j
            break
    start = hi + 1
    if marker:
        hit = None
        for j in range(hi + 1, end):
            if marker in lines[j]:
                hit = j
                break
        if hit is None:
            raise gc.GateError(
                "SECTION_MARKER_MISSING: 标题 %r 范围内找不到 bullet_scope_marker %r"
                % (heading, marker))
        start = hit + 1
    bullets = []
    cur = None
    for j in range(start, end):
        raw = lines[j]
        m = _BULLET_RE.match(raw)
        if m:
            if cur is not None:
                bullets.append(cur)
            cur = Bullet(j + 1, m.group(2))
            continue
        if cur is None:
            continue
        if raw.strip() == "" or _HEAD_RE.match(raw):
            bullets.append(cur)
            cur = None
            continue
        cur.extend(raw.strip())
    if cur is not None:
        bullets.append(cur)
    return bullets


# ── 台账 ─────────────────────────────────────────────────────────────────
CLAUSE_FIELDS = ("id", "anchor", "text_sha256", "requirement", "status")
_ANCHOR_RE = re.compile(r"^(?P<path>[^:]+):(?P<line>[0-9]+)$")


class Ledger:
    def __init__(self, path: pathlib.Path, raw_text: str, doc: dict) -> None:
        self.path = path
        self.raw_text = raw_text
        self.doc = doc
        self._line_cache: dict = {}

    def line_of(self, clause_id: str):
        """台账文件里该 clause id 所在行号（判词要带文件名与行号）。"""
        if clause_id not in self._line_cache:
            hit = None
            for i, line in enumerate(self.raw_text.splitlines(), start=1):
                if clause_id in line and '"id"' in line:
                    hit = i
                    break
            self._line_cache[clause_id] = hit
        return self._line_cache[clause_id]


def load_ledger(path: pathlib.Path) -> Ledger:
    if not path.is_file():
        raise gc.GateError("LEDGER_MISSING: %s" % path)
    try:
        raw = path.read_text(encoding="utf-8")
        doc = json.loads(raw)
    except Exception as exc:  # noqa: BLE001 - 任何解析失败都 fail-closed
        raise gc.GateError("LEDGER_UNPARSABLE: %s: %s" % (path, exc))
    if not isinstance(doc, dict):
        raise gc.GateError("LEDGER_NOT_OBJECT: %s" % path)
    if doc.get("ledger_schema") != LEDGER_SCHEMA:
        raise gc.GateError(
            "LEDGER_SCHEMA_INVALID: %s（需 ledger_schema=%s，实得 %r）"
            % (path, LEDGER_SCHEMA, doc.get("ledger_schema")))
    for key in ("watched_sections", "clauses"):
        val = doc.get(key)
        if not isinstance(val, list) or not val:
            raise gc.GateError("LEDGER_%s_INVALID: %s（需非空数组）" % (key.upper(), path))
    return Ledger(path, raw, doc)


def _led(led: Ledger, clause_id: str) -> str:
    ln = led.line_of(clause_id)
    return "%s:%s" % (led.path.as_posix(), ln if ln else "?")


def _is_vague(text: str) -> bool:
    low = text.lower()
    return any(p in low for p in VAGUE_PHRASES)


# ── 判据主面 ─────────────────────────────────────────────────────────────
def evaluate(repo, ledger_path=None):
    repo = pathlib.Path(repo).resolve()
    snap = Snapshot(repo)
    ledger_path = pathlib.Path(ledger_path) if ledger_path else (repo / LEDGER_REL)
    led = load_ledger(ledger_path)
    findings: list = []

    def bad(code: str, where: str, detail: str) -> None:
        findings.append("%s %s — %s" % (code, where, detail))

    # ── 扫描面 ──
    sections = []
    scope_bullets: dict = {}
    bullet_total = 0
    for idx, sec in enumerate(led.doc["watched_sections"]):
        if not isinstance(sec, dict):
            raise gc.GateError("LEDGER_WATCHED_SECTION_NOT_OBJECT: index %d" % idx)
        file_rel = norm_rel(str(sec.get("file", "")))
        heading = str(sec.get("heading", ""))
        marker = str(sec.get("bullet_scope_marker", "") or "")
        if not file_rel or not heading:
            raise gc.GateError(
                "LEDGER_WATCHED_SECTION_FIELD_MISSING: index %d（需 file/heading）" % idx)
        lines = snap.lines(file_rel)          # fail-closed：设计文档必须存在
        bullets = scan_scope(lines, heading, marker)
        if not bullets:
            raise gc.GateError(
                "SECTION_SCOPE_EMPTY: %s %r%s 范围内 0 条 bullet"
                % (file_rel, heading, (" (marker=%r)" % marker) if marker else ""))
        sections.append({"file": file_rel, "heading": heading, "marker": marker,
                         "bullets": bullets})
        bullet_total += len(bullets)
        for b in bullets:
            scope_bullets.setdefault((file_rel, b.sha256), []).append(b)

    if bullet_total == 0:
        raise gc.GateError("SCOPE_EMPTY: 全部 watched_sections 合计 0 条 bullet")

    watched_files = {s["file"] for s in sections}
    clause_at: dict = {}
    seen_ids: dict = {}
    clauses = []

    for i, cl in enumerate(led.doc["clauses"]):
        if not isinstance(cl, dict):
            bad("CLAUSE_NOT_OBJECT", "%s:?" % led.path.as_posix(),
                "clauses[%d] 不是对象" % i)
            continue
        raw_id = cl.get("id")
        cid = raw_id if isinstance(raw_id, str) and raw_id.strip() else "clauses[%d]" % i
        where = _led(led, cid)
        if cid in seen_ids:
            bad("CLAUSE_DUPLICATE_ID", where, "id 与 clauses[%d] 重复" % seen_ids[cid])
        seen_ids.setdefault(cid, i)
        missing = [f for f in CLAUSE_FIELDS
                   if not isinstance(cl.get(f), str) or not cl.get(f).strip()]
        if missing:
            bad("CLAUSE_FIELD_MISSING", where, "缺字段/非字符串：%s" % ", ".join(missing))
            continue
        clauses.append(cl)

        # C1：条文 sha 必须在范围内，且 anchor 行必须是该 sha 的 bullet 起点
        sha = cl["text_sha256"].strip().lower()
        anchor = cl["anchor"].strip()
        m = _ANCHOR_RE.match(anchor)
        if not m:
            bad("CLAUSE_ANCHOR_MALFORMED", where,
                "anchor=%r 必须是 '文件:行'（仓库相对路径）" % anchor)
            continue
        afile, aline = norm_rel(m.group("path")), int(m.group("line"))
        if afile not in watched_files:
            bad("CLAUSE_ANCHOR_FILE_NOT_WATCHED", where,
                "anchor 文件 %s 不在 watched_sections（%s）" % (afile, sorted(watched_files)))
            continue
        key = (afile, sha)
        if key not in scope_bullets:
            bad("CLAUSE_TEXT_CHANGED", where,
                "text_sha256=%s 在 %s 的监视范围内不存在任何 bullet"
                "（条文已改或条目过期 ⇒ 复核条文并更新 text_sha256）" % (sha[:16], afile))
            continue
        n_lines = len(snap.lines(afile))
        if aline < 1 or aline > n_lines:
            bad("CLAUSE_ANCHOR_OUT_OF_RANGE", where,
                "anchor %s:%d 越界（该文件共 %d 行）" % (afile, aline, n_lines))
            continue
        actual = sorted(b.line for b in scope_bullets[key])
        if aline not in actual:
            bad("CLAUSE_ANCHOR_DRIFT", where,
                "anchor %s:%d 不是该条文的 bullet 起点；实际行号 = %s（重新锚定）"
                % (afile, aline, actual))
        clause_at.setdefault((afile, sha, aline), []).append(cid)

        # C2/C3/C5/C6：生产锚
        anchors = cl.get("production_anchor")
        if not isinstance(anchors, list) or any(not isinstance(a, str) for a in anchors):
            bad("CLAUSE_PRODUCTION_ANCHOR_NOT_LIST", where,
                "production_anchor 必须是字符串数组（可为 []）")
            anchors = []
        tokens = cl.get("anchor_tokens")
        if tokens is None:
            tokens = {}
        if not isinstance(tokens, dict):
            bad("CLAUSE_ANCHOR_TOKENS_NOT_OBJECT", where, "anchor_tokens 必须是对象")
            tokens = {}
        for a in anchors:
            am = _ANCHOR_RE.match(a.strip())
            if not am:
                bad("PRODUCTION_ANCHOR_MALFORMED", where,
                    "production_anchor=%r 必须是 '文件:行'" % a)
                continue
            pfile, pline = norm_rel(am.group("path")), int(am.group("line"))
            if pfile.startswith("/") or ".." in pfile.split("/"):
                bad("PRODUCTION_ANCHOR_NOT_REPO_RELATIVE", where,
                    "production_anchor %s 必须是仓库相对路径（不得绝对路径/..）" % pfile)
                continue
            awhere = "%s @ %s" % (where, a)
            ok_prod, why = is_production_path(pfile)
            if not ok_prod:
                bad("PRODUCTION_ANCHOR_NOT_PRODUCTION", awhere,
                    "锚点落在非生产面：%s（测试/夹具/归档不得充当生产实现）" % why)
            try:
                ptext = snap.text(pfile)
            except gc.GateError as exc:
                bad("PRODUCTION_ANCHOR_MISSING", awhere, str(exc))
                continue
            plines = ptext.splitlines()
            if pline < 1 or pline > len(plines):
                bad("PRODUCTION_ANCHOR_OUT_OF_RANGE", awhere,
                    "行号越界（%s 共 %d 行）" % (pfile, len(plines)))
                continue
            token = tokens.get(a)
            if not isinstance(token, str) or not token.strip():
                bad("ANCHOR_TOKEN_MISSING", awhere,
                    "anchor_tokens 必须为该锚点给出 '文件:行' → 该行必须出现的字面 token")
            else:
                tok = token.strip()
                if tok not in plines[pline - 1]:
                    found = [k + 1 for k, ln in enumerate(plines) if tok in ln]
                    bad("ANCHOR_TOKEN_MISMATCH", awhere,
                        "第 %d 行不含 token %r；该 token 实际出现在行 %s（行号已漂移 ⇒ 重新锚定）"
                        % (pline, tok, found if found else "（整份文件都没有）"))

        # C4：状态诚实
        status = cl["status"].strip()
        assertion = str(cl.get("assertion") or "").strip()
        reason = str(cl.get("reason") or "").strip()
        exit_condition = str(cl.get("exit_condition") or "").strip()
        if status not in (STATUS_WIRED, STATUS_UNWIRED):
            bad("CLAUSE_STATUS_UNKNOWN", where,
                "status=%r 只允许 wired / unwired（未知状态不得冒充 unwired）" % status)
        elif status == STATUS_WIRED:
            if not anchors:
                bad("CLAUSE_WIRED_WITHOUT_PRODUCTION_ANCHOR", where,
                    "status=wired 必须给出非测试 production_anchor")
            if len(assertion) < MIN_ASSERTION:
                bad("CLAUSE_WIRED_WITHOUT_ASSERTION", where,
                    "status=wired 必须给出 assertion（≥%d 字，写清该锚点上实现了什么）"
                    % MIN_ASSERTION)
            elif _is_vague(assertion):
                bad("CLAUSE_WIRED_VAGUE_ASSERTION", where, "assertion 含无判据措辞")
            elif assertion == cl["requirement"].strip():
                bad("CLAUSE_WIRED_ASSERTION_COPIES_REQUIREMENT", where,
                    "assertion 只是抄 requirement，未说明锚点实现了什么")
            if reason or exit_condition:
                bad("CLAUSE_WIRED_WITH_REASON", where,
                    "status=wired 不得再带 reason/exit_condition"
                    "（有东西要解释就不是 wired，请如实标 unwired）")
        else:
            if anchors:
                bad("CLAUSE_UNWIRED_WITH_PRODUCTION_ANCHOR", where,
                    "status=unwired 不得带 production_anchor=%s"
                    "（有生产锚就是在主张已接线；半接线请如实标 unwired 并把证据写进 reason）"
                    % anchors)
            if len(reason) < MIN_TEXT:
                bad("CLAUSE_UNWIRED_WITHOUT_REASON", where,
                    "status=unwired 必须给出 reason（≥%d 字，写清差在哪）" % MIN_TEXT)
            elif _is_vague(reason):
                bad("CLAUSE_UNWIRED_VAGUE_REASON", where, "reason 含无判据措辞")
            if len(exit_condition) < MIN_TEXT:
                bad("CLAUSE_UNWIRED_WITHOUT_EXIT_CONDITION", where,
                    "status=unwired 必须给出具名 exit_condition（解除条件）")
            elif _is_vague(exit_condition):
                bad("CLAUSE_UNWIRED_VAGUE_EXIT_CONDITION", where,
                    "exit_condition 含「以后再说」这类无判据措辞")
            elif not NAMED_EXIT_HINT_RE.search(exit_condition):
                bad("CLAUSE_EXIT_CONDITION_NOT_NAMED", where,
                    "exit_condition 必须带可核对的落点（路径 / file:line / "
                    "反引号标识符 / CHK-* 门号 / §条款）")
            if assertion:
                bad("CLAUSE_UNWIRED_WITH_ASSERTION", where,
                    "status=unwired 不得带 assertion（未接线就没有可断言的行为）")

    # C1：覆盖面 —— 每条 bullet 恰好一条条目
    for s in sections:
        for b in s["bullets"]:
            ids = clause_at.get((s["file"], b.sha256, b.line), [])
            where = "%s:%d" % (s["file"], b.line)
            if not ids:
                bad("CLAUSE_MISSING", where,
                    "范围内该 bullet 无台账条目（sha256=%s；设计新增/改动策略后必须建映射）"
                    % b.sha256[:16])
            elif len(ids) > 1:
                bad("CLAUSE_DUPLICATE_FOR_BULLET", where,
                    "该 bullet 被 %d 条条目认领：%s" % (len(ids), ", ".join(sorted(ids))))

    # C6：tracked 判据（适用性显式登记，不静默跳过）
    tracked_mode = "not-a-worktree"
    try:
        probe = subprocess.run(
            ["git", "-C", str(repo), "rev-parse", "--is-inside-work-tree"],
            capture_output=True, text=True, timeout=30)
        if probe.returncode == 0 and probe.stdout.strip() == "true":
            tracked_mode = "enforced"
    except (OSError, subprocess.SubprocessError):
        tracked_mode = "git-unavailable"
    if tracked_mode == "enforced":
        for cl in clauses:
            for a in (cl.get("production_anchor") or []):
                am = _ANCHOR_RE.match(a.strip())
                if not am:
                    continue
                pfile = norm_rel(am.group("path"))
                if not (repo / pfile).is_file():
                    continue
                if not _git_tracked(repo, pfile):
                    bad("PRODUCTION_ANCHOR_UNTRACKED", "%s @ %s" % (_led(led, cl["id"]), a),
                        "锚点文件未被 git 跟踪（在制草稿不是生产实现；先提交再接线）")

    wired = sum(1 for c in clauses if c.get("status") == STATUS_WIRED)
    unwired = sum(1 for c in clauses if c.get("status") == STATUS_UNWIRED)
    extra = {
        "ledger": led.path.as_posix(),
        "ledger_schema": LEDGER_SCHEMA,
        "scope_bullet_count": bullet_total,
        "clause_count": len(clauses),
        "wired_count": wired,
        "unwired_count": unwired,
        "unwired_ids": sorted(c["id"] for c in clauses
                              if c.get("status") == STATUS_UNWIRED),
        "production_anchor_count": sum(len(c.get("production_anchor") or []) for c in clauses),
        "watched_sections": [
            {"file": s["file"], "heading": s["heading"],
             "bullet_scope_marker": s["marker"], "bullet_count": len(s["bullets"])}
            for s in sections],
        "tracked_check": tracked_mode,
        "reads": dict(sorted(snap.reads.items())),
        "disk_reads": dict(sorted(snap.disk_reads.items())),
    }
    return sorted(set(findings)), extra


_TRACKED_CACHE: dict = {}


def _git_tracked(repo: pathlib.Path, rel: str) -> bool:
    key = (str(repo), rel)
    if key not in _TRACKED_CACHE:
        try:
            r = subprocess.run(
                ["git", "-C", str(repo), "ls-files", "--error-unmatch", "--", rel],
                capture_output=True, text=True, timeout=30)
            _TRACKED_CACHE[key] = (r.returncode == 0)
        except (OSError, subprocess.SubprocessError):
            _TRACKED_CACHE[key] = True   # git 不可用：不据此判红（tracked_check 已登记）
    return _TRACKED_CACHE[key]


def print_findings(findings) -> None:
    """全量打印，不截断（判词带文件名与行号）。"""
    print("%s_FAIL: %d finding(s)" % (CHECK_ID, len(findings)))
    for item in findings:
        print("  - %s" % item)


# ── 自检夹具 ─────────────────────────────────────────────────────────────
_DOC_HEAD = "# 设计\n\n"
_DOC_SECTION = (
    "### 8.3 调度器\n"
    "\n"
    "**编排策略**——按以下策略编排：\n"
    "\n"
    "- **静态预算**：从输入数据静态估算每个模块的内存需求。\n"
    "- **可丢弃重跑**：内存仍不足时丢弃进度最低的工作流并释放其占用。\n"
    "\n"
    "本小节到此结束\n"
    "\n"
    "### 8.4 下一节\n"
    "\n"
    "- 这一条不在 marker 之后的范围里。\n"
)
_PROD_LINES = [
    "// fixture production source",
    "int estimate_budget(void) { return 1; }",
    "int discard_lowest(void) { return 2; }",
    "int unused_line(void) { return 3; }",
]


def _bullets_of(doc_text: str, marker: str = "编排策略"):
    return scan_scope((_DOC_HEAD + doc_text).splitlines(), "### 8.3 调度器", marker)


def _clause(bullet, **kw) -> dict:
    cl = {
        "id": "DESIGN-8-ORCH-BUDGET",
        "anchor": "ASTROCS_DESIGN.md:%d" % bullet.line,
        "text_sha256": bullet.sha256,
        "requirement": bullet.norm,
        "status": STATUS_WIRED,
        "production_anchor": ["lib/infra/sched.cpp:2"],
        "anchor_tokens": {"lib/infra/sched.cpp:2": "estimate_budget"},
        "assertion": "sched.cpp:2 的 estimate_budget() 是调度决策前的静态内存估算入口。",
        "reason": "",
        "exit_condition": "",
    }
    cl.update(kw)
    return cl


def _good_ledger(doc_text: str = _DOC_SECTION) -> dict:
    bulls = _bullets_of(doc_text)
    return {
        "ledger_schema": LEDGER_SCHEMA,
        "ledger_id": "design_clauses",
        "purpose": "fixture",
        "watched_sections": [{"file": "ASTROCS_DESIGN.md", "heading": "### 8.3 调度器",
                              "bullet_scope_marker": "编排策略"}],
        "clauses": [
            _clause(bulls[0]),
            _clause(bulls[1], id="DESIGN-8-ORCH-DISCARD-RERUN",
                    production_anchor=[], anchor_tokens={}, assertion="",
                    status=STATUS_UNWIRED,
                    reason="生产零实现：无丢弃最低进度工作流的实现面。",
                    exit_condition="`discard_lowest` 出现在生产调度路径"
                                   "（lib/infra/sched.cpp）后改 wired。"),
        ],
    }


def _write_fixture(root: pathlib.Path, doc: str = _DOC_SECTION, prod=None,
                   ledger: dict = None) -> pathlib.Path:
    root.mkdir(parents=True, exist_ok=True)
    (root / "ASTROCS_DESIGN.md").write_text(_DOC_HEAD + doc, encoding="utf-8")
    prod = _PROD_LINES if prod is None else prod
    src = root / "lib" / "infra"
    src.mkdir(parents=True, exist_ok=True)
    (src / "sched.cpp").write_text("\n".join(prod) + "\n", encoding="utf-8")
    led = _good_ledger() if ledger is None else ledger
    lp = root / "eng" / "ci" / "ledgers" / "design_clauses.json"
    lp.parent.mkdir(parents=True, exist_ok=True)
    lp.write_text(json.dumps(led, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return lp


def _selftest(json_out=None) -> int:
    failures: list = []
    cases = [0]
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="chk_design_clause_"))

    def fresh(name: str) -> pathlib.Path:
        repo = tmp / name
        if repo.exists():
            shutil.rmtree(repo)
        return repo

    def run(name, expect_fail, maker):
        cases[0] += 1
        try:
            findings, _extra = evaluate(fresh(name), maker(fresh(name)))
        except gc.GateError as exc:
            failures.append("%s: unexpected GateError: %s" % (name, exc))
            return
        got = bool(findings)
        if got != expect_fail:
            failures.append("%s: expected_fail=%s got=%s findings=%s"
                            % (name, expect_fail, got, findings[:3]))
        else:
            print("SELFTEST_PASS %s (expect_fail=%s, findings=%d)"
                  % (name, expect_fail, len(findings)))

    def run_crash(name, maker):
        cases[0] += 1
        try:
            evaluate(fresh(name), maker(fresh(name)))
        except gc.GateError as exc:
            print("SELFTEST_PASS %s (fail-closed GateError: %s)" % (name, exc))
            return
        failures.append("%s: expected GateError (fail-closed), got a verdict" % name)

    def mk(mutate=None, doc=_DOC_SECTION, prod=None):
        def _m(repo):
            led = _good_ledger(doc)
            if mutate:
                mutate(led, _bullets_of(doc))
            return _write_fixture(repo, doc=doc, prod=prod, ledger=led)
        return _m

    try:
        # ── 正例 ──
        run("green_complete_mapping", False, mk())
        run("green_all_unwired_honest", False, mk(mutate=lambda led, b: led.update(
            clauses=[dict(c, status=STATUS_UNWIRED, production_anchor=[], anchor_tokens={},
                          assertion="", reason="生产零实现：fixture 未接线。",
                          exit_condition="`estimate_budget` 被生产调度路径调用后改 wired。")
                     for c in led["clauses"]])))

        doc_no_marker = _DOC_SECTION.replace("**编排策略**——按以下策略编排：\n\n", "")

        def mk_no_marker(repo):
            bulls = _bullets_of(doc_no_marker, marker="")
            led = {"ledger_schema": LEDGER_SCHEMA, "ledger_id": "x",
                   "watched_sections": [{"file": "ASTROCS_DESIGN.md",
                                         "heading": "### 8.3 调度器"}],
                   "clauses": [_clause(b, id="DESIGN-8-NOMARK-%d" % i)
                               for i, b in enumerate(bulls)]}
            return _write_fixture(repo, doc=doc_no_marker, ledger=led)

        run("green_no_marker_scope", False, mk_no_marker)

        # ── C1 负例 ──
        run("red_missing_clause_entry", True,
            mk(mutate=lambda led, b: led.update(clauses=led["clauses"][:1])))
        run("red_text_sha256_wrong", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0], text_sha256="0" * 64), led["clauses"][1]])))
        run("red_anchor_line_drift", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0], anchor="ASTROCS_DESIGN.md:%d" % b[1].line),
                     led["clauses"][1]])))
        run("red_duplicate_clause_for_bullet", True, mk(mutate=lambda led, b: led.update(
            clauses=[led["clauses"][0], dict(led["clauses"][0], id="DESIGN-DUP"),
                     led["clauses"][1]])))
        run("red_duplicate_clause_id", True, mk(mutate=lambda led, b: led.update(
            clauses=[led["clauses"][0],
                     dict(led["clauses"][1], id=led["clauses"][0]["id"])])))
        run("red_extra_stale_clause", True, mk(mutate=lambda led, b: led.update(
            clauses=led["clauses"] + [dict(led["clauses"][0], id="DESIGN-GONE",
                                           text_sha256="f" * 64)])))
        run("red_anchor_file_not_watched", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0], anchor="lib/infra/sched.cpp:2"),
                     led["clauses"][1]])))

        # ── C2/C3 负例 ──
        run("red_production_anchor_missing_file", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0], production_anchor=["lib/infra/gone.cpp:1"],
                          anchor_tokens={"lib/infra/gone.cpp:1": "estimate_budget"}),
                     led["clauses"][1]])))
        run("red_production_anchor_line_out_of_range", True,
            mk(mutate=lambda led, b: led.update(
                clauses=[dict(led["clauses"][0], production_anchor=["lib/infra/sched.cpp:99999"],
                              anchor_tokens={"lib/infra/sched.cpp:99999": "estimate_budget"}),
                         led["clauses"][1]])))
        run("red_production_anchor_in_tests_dir", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0],
                          production_anchor=["eng/tests/unit/sched_test.cpp:1"],
                          anchor_tokens={"eng/tests/unit/sched_test.cpp:1": "estimate_budget"}),
                     led["clauses"][1]])))
        run("red_production_anchor_test_file_name", True,
            mk(mutate=lambda led, b: led.update(
                clauses=[dict(led["clauses"][0],
                              production_anchor=["lib/infra/test_sched.cpp:3"],
                              anchor_tokens={"lib/infra/test_sched.cpp:3": "estimate_budget"}),
                         led["clauses"][1]])))
        run("red_production_anchor_oracle_dir", True,
            mk(mutate=lambda led, b: led.update(
                clauses=[dict(led["clauses"][0],
                              production_anchor=["lib/infra/oracle/selfcheck.cpp:5"],
                              anchor_tokens={"lib/infra/oracle/selfcheck.cpp:5": "estimate_budget"}),
                         led["clauses"][1]])))
        run("red_production_anchor_malformed", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0], production_anchor=["lib/infra/sched.cpp"],
                          anchor_tokens={}), led["clauses"][1]])))

        # ── C5 负例 ──
        run("red_anchor_token_mismatch", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0], production_anchor=["lib/infra/sched.cpp:4"],
                          anchor_tokens={"lib/infra/sched.cpp:4": "estimate_budget"}),
                     led["clauses"][1]])))
        run("red_anchor_token_missing_entry", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0], anchor_tokens={}), led["clauses"][1]])))

        # ── C4 负例 ──
        run("red_wired_without_assertion", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0], assertion=""), led["clauses"][1]])))
        run("red_wired_without_production_anchor", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0], production_anchor=[], anchor_tokens={}),
                     led["clauses"][1]])))
        run("red_wired_with_reason", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0], reason="半实现，还有一半没接。",
                          exit_condition="补完后移除该条登记。"), led["clauses"][1]])))
        run("red_unwired_without_reason", True, mk(mutate=lambda led, b: led.update(
            clauses=[led["clauses"][0], dict(led["clauses"][1], reason="")])))
        run("red_unwired_vague_exit_condition", True, mk(mutate=lambda led, b: led.update(
            clauses=[led["clauses"][0],
                     dict(led["clauses"][1], exit_condition="以后再说的那种，先这样。")])))
        run("red_unwired_not_named_exit_condition", True, mk(mutate=lambda led, b: led.update(
            clauses=[led["clauses"][0],
                     dict(led["clauses"][1], exit_condition="等实现好了并且经过验证就可以了。")])))
        run("red_unwired_with_assertion", True, mk(mutate=lambda led, b: led.update(
            clauses=[led["clauses"][0],
                     dict(led["clauses"][1], assertion="这条其实也能断言点东西。")])))
        run("red_unwired_with_production_anchor", True, mk(mutate=lambda led, b: led.update(
            clauses=[led["clauses"][0], dict(led["clauses"][1],
                                             production_anchor=["lib/infra/sched.cpp:3"],
                                             anchor_tokens={"lib/infra/sched.cpp:3":
                                                            "discard_lowest"})])))
        run("red_production_anchor_selfcheck_name", True,
            mk(mutate=lambda led, b: led.update(
                clauses=[dict(led["clauses"][0],
                              production_anchor=["lib/infra/sched_selfcheck.cpp:2"],
                              anchor_tokens={"lib/infra/sched_selfcheck.cpp:2":
                                             "estimate_budget"}),
                         led["clauses"][1]])))
        run("red_unknown_status", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0], status="partial"), led["clauses"][1]])))
        run("red_assertion_copies_requirement", True, mk(mutate=lambda led, b: led.update(
            clauses=[dict(led["clauses"][0], assertion=led["clauses"][0]["requirement"]),
                     led["clauses"][1]])))

        # ── C6 负例（真实 git work tree） ──
        def mk_git(repo, tracked: bool):
            led = _good_ledger()
            lp = _write_fixture(repo, ledger=led)
            for name in ("tracked.cpp", "draft.cpp"):
                (repo / "lib" / "infra" / name).write_text(
                    "\n".join(_PROD_LINES) + "\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(repo), "init", "-q"],
                           check=True, capture_output=True)
            subprocess.run(["git", "-C", str(repo), "add", "ASTROCS_DESIGN.md", "eng",
                            "lib/infra/tracked.cpp"], check=True, capture_output=True)
            doc = json.loads(lp.read_text(encoding="utf-8"))
            doc["clauses"][0]["production_anchor"] = ["lib/infra/draft.cpp:2"]
            doc["clauses"][0]["anchor_tokens"] = {"lib/infra/draft.cpp:2": "estimate_budget"}
            lp.write_text(json.dumps(doc, ensure_ascii=False, indent=2), encoding="utf-8")
            if tracked:
                subprocess.run(["git", "-C", str(repo), "add", "-A"],
                               check=True, capture_output=True)
            return lp

        run("red_production_anchor_untracked", True,
            lambda repo: mk_git(repo, tracked=False))
        run("green_production_anchor_tracked", False,
            lambda repo: mk_git(repo, tracked=True))

        # ── C7 fail-closed（rc=2 面） ──
        def mk_no_doc(repo):
            lp = _write_fixture(repo)
            (repo / "ASTROCS_DESIGN.md").unlink()
            return lp

        run_crash("crash_missing_design_doc", mk_no_doc)

        def mk_bad_heading(repo):
            led = _good_ledger()
            led["watched_sections"][0]["heading"] = "### 9.9 不存在的标题"
            return _write_fixture(repo, ledger=led)

        run_crash("crash_heading_missing", mk_bad_heading)

        def mk_empty_scope(repo):
            # marker 之后没有任何 bullet（marker 落在小节末尾）⇒ 扫描面为 0，
            # 「scanned == 0 ⇒ rc != 0」必须 fail-closed，而不是静默判绿。
            led = _good_ledger()
            led["watched_sections"][0]["bullet_scope_marker"] = "本小节到此结束"
            return _write_fixture(repo, ledger=led)

        run_crash("crash_empty_scope", mk_empty_scope)

        def mk_bad_schema(repo):
            lp = _write_fixture(repo)
            doc = json.loads(lp.read_text(encoding="utf-8"))
            doc["ledger_schema"] = "nope/v0"
            lp.write_text(json.dumps(doc, ensure_ascii=False), encoding="utf-8")
            return lp

        run_crash("crash_ledger_schema", mk_bad_schema)

        # ── C8 同轮读数一致 ──
        cases[0] += 1
        repo = fresh("det_single_read")
        _f1, ex1 = evaluate(repo, _write_fixture(repo))
        reread = [p for p, n in ex1["disk_reads"].items() if n != 1]
        reused = [p for p, n in ex1["reads"].items() if n > 1]
        if reread:
            failures.append("det_single_read: 同轮重复落盘读取 %s" % reread)
        elif not reused:
            failures.append("det_single_read: 缓存从未被复用 ⇒ 判据空转（vacuous）")
        else:
            print("SELFTEST_PASS det_single_read (paths=%d, 每路径落盘读 1 次, "
                  "缓存复用 %d 处)" % (len(ex1["disk_reads"]), len(reused)))

        cases[0] += 1
        repo = fresh("det_two_reads")
        lp = _write_fixture(repo)
        fa, ea = evaluate(repo, lp)
        fb, eb = evaluate(repo, lp)
        if (fa, json.dumps(ea, sort_keys=True, ensure_ascii=False)) != \
           (fb, json.dumps(eb, sort_keys=True, ensure_ascii=False)):
            failures.append("det_two_reads: 同轮两次读数不一致")
        else:
            print("SELFTEST_PASS det_two_reads (identical verdict, findings=%d)" % len(fa))

        # ── 判词不截断 ──
        cases[0] += 1
        many = ["FINDING-%03d lib/x.cpp:%d — 第 %d 条判词" % (i, i, i) for i in range(200)]
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            print_findings(many)
        out = buf.getvalue()
        if out.count("FINDING-") != 200 or "more)" in out:
            failures.append("no_truncation: 200 条判词未全量打印（%d 条）"
                            % out.count("FINDING-"))
        else:
            print("SELFTEST_PASS no_truncation (200/200 printed, no ellipsis)")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print("SELFTEST cases=%d" % cases[0])
    if cases[0] < 12:
        failures.append("selftest_cases=%d < 12" % cases[0])
    if failures:
        print("SELFTEST_FAIL:")
        for item in failures:
            print("  " + item)
        gc.report("FAIL", CHECK_ID + "-SELFTEST", failures, {"cases": cases[0]}, json_out)
        return 1
    print("SELFTEST_PASS: all %d cases match expectation" % cases[0])
    gc.report("PASS", CHECK_ID + "-SELFTEST", [], {"cases": cases[0],
               "positive_cases": 4, "negative_cases": 30, "crash_cases": 4}, json_out)
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-DESIGN-CLAUSE-WIRING 设计条文映射门")
    ap.add_argument("--repo", default=str(gc.repo_root()))
    ap.add_argument("--ledger", default=None,
                    help="台账路径（缺省 = <repo>/%s）" % LEDGER_REL)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    try:
        # 自检也必须走同一三态出口：自检自身崩溃同样是 CRASH(2)，不得记成 FAIL。
        if args.self_test:
            return _selftest(args.json_out)
        findings, extra = evaluate(pathlib.Path(args.repo).resolve(), args.ledger)
        if findings:
            print_findings(findings)
            gc.report("FAIL", CHECK_ID, findings, extra, args.json_out)
            return 1
        print("%s_PASS scope_bullets=%d clauses=%d wired=%d unwired=%d anchors=%d "
              "tracked_check=%s"
              % (CHECK_ID, extra["scope_bullet_count"], extra["clause_count"],
                 extra["wired_count"], extra["unwired_count"],
                 extra["production_anchor_count"], extra["tracked_check"]))
        if extra["unwired_count"]:
            print("  unwired（如实登记、可见，不等于绿）：%s"
                  % ", ".join(extra["unwired_ids"]))
        gc.report("PASS", CHECK_ID, [], extra, args.json_out)
        return 0
    except gc.GateError as exc:
        print("%s_CRASH: %s" % (CHECK_ID, exc), file=sys.stderr)
        return 2
    except Exception as exc:  # noqa: BLE001 - 崩溃必须独立退出码，不得记成 FAIL/PASS
        import traceback
        traceback.print_exc()
        print("%s_CRASH: unexpected %s: %s" % (CHECK_ID, type(exc).__name__, exc),
              file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
