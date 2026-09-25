#!/usr/bin/env python3
"""块流一致性机器门（ARCH-505）：登记册 vs 代码事实的双向漂移检测。

它**不**判断「注册表该不该改」（那是负责人裁决，AGENTS §10）；它保证：

  D1  登记册结构合法：每条 deviation 有 id/severity/kind/registry_ref/summary/evidence；
  D2  每条 evidence 的文件存在、行号在范围内、且该行**确实包含**声明的 token
      —— token 消失说明代码改了（修好了或改坏了），登记册必须同步，否则判红；
  D3  evidence 不得指向空行/注释占位（防「用注释行凑证据」）；
  D4  id 唯一、severity 取值合法；
  D5  每条 blocker 必须带 owner_decision_required 标记（结构性变更不得静默）；
  D6  登记册不得为空（空登记册 = 未做一致性核查，判红）；
  D7  被登记为 blocker 的偏差必须出现在 owner 上报文件（工程控制/RELEASE-05/OPEN_QUESTIONS.md）
      的引用里，否则判红 —— 防止「登记了但没上呈」；
  D8  **符号级判据（抗行号漂移，GATE-FIX-01）**：每条 evidence 必须显式声明 symbol：
      * symbol = 函数名 ⇒ 该符号必须在本文件内被解析到**唯一**的函数体，且 token
        必须落在该函数体行区间内（包含性判据）。代码整体上下移动不再让门失真；
        token 被删除、被移出该函数、或该函数被改名/删除都判红。
      * symbol = "file" ⇒ 只做文件级判据（token 必须在文件内出现）。
        仅用于合法地位于任何函数体之外的锚点（如文件作用域的 ModuleDescriptor）。
      * 精确到「哪一行」仍由 D2 的行号判据负责：D8 管「token 在不在声明的函数里」
        （抗漂移/抗删除/抗移动），D2 管「声明的行是不是那一行」（抗错位）。
        两者独立，D8 为绿而 D2 为红 = 行号漂移，同步行号即可，不需要重新论证断言。
      * 不声明 symbol ⇒ 判红（D8）。这样「锚点是否有函数归属」是显式决定，
        不允许再退回「只有行号」的脆弱形态。

判据强度（防退化，AGENTS §9）：
  * D8 不是「文件存在即绿」：符号必须解析得到、token 必须在其函数体内；
  * D2 保留原行号判据，用于把「漂移」显式暴露出来要求同步（行号仍是可读的定位辅助）；
  * 负例覆盖：token 被删除（S3/S12/S18）、token 被移出声明符号（S13）、符号不存在（S14）、
    evidence 缺 symbol（S15）、符号在文件内不唯一（S16）；
  * 正例覆盖抗漂移语义：行号被人为改错时 D8 仍为绿、D2 判红（S17）——证明判据没有退化成
    「行号对齐即绿」，也证明「代码移动」与「断言失效」能被区分开。

用法：
  python3 eng/tools/quality/check_block_flow_conformance.py [--register <json>] [--json-out <json>]
  python3 eng/tools/quality/check_block_flow_conformance.py --self-test
"""
import argparse
import copy
import io
import json
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(REPO, "eng", "ci"))
from gate_trust import (  # noqa: E402  三态（PASS/FAIL/CRASH）口径的单一实现点
    KIND_CONTENT, KIND_DISCRIMINATING, KIND_EXEMPTION, KIND_PROTECTIVE, emit)
REGISTER = os.path.join(REPO, "eng/contracts/block_flow/conformance_deviations.json")
OPEN_QUESTIONS = os.path.join(REPO, "工程控制/RELEASE-05/OPEN_QUESTIONS.md")
SEVERITIES = ("blocker", "major", "minor", "info")
KINDS = ("producer_mismatch", "dead_port", "undeclared_input", "undeclared_output",
         "undeclared_module", "no_named_blocks_in_production", "unit_mismatch",
         "shape_mismatch")
FILE_SCOPE = "file"

# ── 符号级解析（D8 用）───────────────────────────────────────────────────────
# 只做「顶层函数体」的粗粒度定位：把字符串/字符字面量与注释内容屏蔽为空格后按花括号
# 配平扫描。目的不是做完整 C++ 解析，而是给出「token 是否落在声明的函数体内」这一
# 抗漂移判据；解析失败（找不到符号/符号不唯一）一律判红，绝不静默退化为文件级。
_SIG_START = re.compile(
    r'^\s*(?:template\s*<[^;{}]*>\s*)?'
    r'(?:(?:static|inline|constexpr|extern|virtual|explicit)\s+)*'
    r'(?:[A-Za-z_][A-Za-z0-9_]*\s*(?:::\s*[A-Za-z_][A-Za-z0-9_]*\s*)*'
    r'(?:<[^;{}()]*>)?\s*[*&]*\s+)+')
_NAME_CALL = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*\(')
_KEYWORDS = frozenset((
    "if", "for", "while", "switch", "return", "sizeof", "catch", "do", "else",
    "namespace", "struct", "class", "union", "enum", "try", "new", "delete"))


def _mask_literals(lines):
    """把每行的字符串/字符字面量与注释内容替换为空格（长度不变）。"""
    out = []
    in_block = False
    for raw in lines:
        buf = []
        i, n = 0, len(raw)
        while i < n:
            c = raw[i]
            if in_block:
                if c == "*" and i + 1 < n and raw[i + 1] == "/":
                    buf.append("  "); i += 2; in_block = False; continue
                buf.append(" "); i += 1; continue
            if c == "/" and i + 1 < n and raw[i + 1] == "/":
                buf.append(" " * (n - i)); break
            if c == "/" and i + 1 < n and raw[i + 1] == "*":
                buf.append("  "); i += 2; in_block = True; continue
            if c == '"' or c == "'":
                quote = c
                buf.append(" "); i += 1
                while i < n:
                    if raw[i] == "\\" and i + 1 < n:
                        buf.append("  "); i += 2; continue
                    if raw[i] == quote:
                        buf.append(" "); i += 1; break
                    buf.append(" "); i += 1
                continue
            buf.append(c); i += 1
        out.append("".join(buf))
    return out


def symbol_ranges(lines):
    """返回 {symbol_name: [(start_line, end_line), ...]}（1-based，闭区间）。"""
    mask = _mask_literals(lines)
    found = {}
    i, n = 0, len(mask)
    while i < n:
        line = mask[i]
        if line.strip().startswith("#") or "{" not in line:
            i += 1; continue
        idx = line.index("{")
        if line[:idx].strip() and not _SIG_START.match(line[:idx] + " "):
            i += 1; continue
        head = line[:idx]
        start = i
        j = i - 1
        while j >= 0:
            prev = mask[j]
            cand = (prev[:prev.index("{")] if "{" in prev else prev) + " "
            if _SIG_START.match(cand) or prev.strip().startswith("//") or prev.strip().startswith("#"):
                head = prev + " " + head
                start = j
                j -= 1
                continue
            break
        name = None
        for m in _NAME_CALL.finditer(head):
            name = m.group(1)
        if not name or name in _KEYWORDS:
            i += 1; continue
        depth, end, k = 0, None, i
        while k < n:
            for ch in mask[k]:
                if ch == "{":
                    depth += 1
                elif ch == "}":
                    depth -= 1
                    if depth == 0:
                        end = k; break
            if end is not None:
                break
            k += 1
        if end is None:
            i += 1; continue
        found.setdefault(name, []).append((start + 1, end + 1))
        i = end + 1
    return found


def check_register(doc, repo=REPO):
    errs = []
    devs = doc.get("deviations")
    if not isinstance(devs, list) or not devs:
        return ["D6 register is empty or malformed (未做一致性核查 = 判红)"]
    seen = set()
    oq = ""
    oqp = os.path.join(repo, "工程控制/RELEASE-05/OPEN_QUESTIONS.md")
    # D7 的上呈锚点：**锚点缺失不得静默放行**（fail-closed）。旧实现用
    # `if os.path.isfile(oqp)` 把「文件不在」变成「D7 整条不判」——这正是失效形态
    # 「豁免面把该红的东西放过去了」：删掉锚点文件即可让 blocker 免于上呈判据。
    oq_missing = not os.path.isfile(oqp)
    if not oq_missing:
        oq = io.open(oqp, encoding="utf-8").read()
    # 每个文件只读/解析一次（文本行 + 符号表）
    cache = {}

    def load(rel):
        if rel not in cache:
            p = os.path.join(repo, rel)
            if not os.path.isfile(p):
                cache[rel] = None
            else:
                lines = io.open(p, encoding="utf-8", errors="replace").read().splitlines()
                cache[rel] = (lines, symbol_ranges(lines))
        return cache[rel]

    for d in devs:
        did = d.get("id", "?")
        if did in seen:
            errs.append("D4 duplicate deviation id %s" % did)
        seen.add(did)
        for k in ("severity", "kind", "registry_ref", "summary", "evidence"):
            if not d.get(k):
                errs.append("D1 %s missing field %s" % (did, k))
        if d.get("severity") not in SEVERITIES:
            errs.append("D4 %s invalid severity %r" % (did, d.get("severity")))
        if d.get("kind") not in KINDS:
            errs.append("D4 %s invalid kind %r" % (did, d.get("kind")))
        ev = d.get("evidence") or []
        if not ev:
            errs.append("D1 %s has no evidence" % did)
        for e in ev:
            f = e.get("file")
            ln = e.get("line")
            tok = e.get("contains")
            sym = e.get("symbol")
            if not f or not isinstance(ln, int) or not tok:
                errs.append("D1 %s malformed evidence %r" % (did, e))
                continue
            loaded = load(f)
            if loaded is None:
                errs.append("D2 %s evidence file missing: %s" % (did, f))
                continue
            lines, syms = loaded
            # ── D8：符号级判据（抗行号漂移）────────────────────────────────
            if not sym:
                errs.append("D8 %s evidence lacks 'symbol' (declare the enclosing function "
                            "name, or \"file\" for file-scope anchors): %s:%d" % (did, f, ln))
            elif sym == FILE_SCOPE:
                if not any(tok in t for t in lines):
                    errs.append("D8 %s token %r not found anywhere in %s (file-scope anchor "
                                "is dead)" % (did, tok, f))
            elif sym not in syms:
                errs.append("D8 %s declared symbol %r not found in %s -> function renamed/"
                            "deleted or symbol name is wrong" % (did, sym, f))
            elif len(syms[sym]) > 1:
                errs.append("D8 %s declared symbol %r is ambiguous in %s (%d definitions: %s) "
                            "-> qualify the anchor or use \"file\""
                            % (did, sym, f, len(syms[sym]),
                               ", ".join("%d-%d" % r for r in syms[sym])))
            else:
                s0, s1 = syms[sym][0]
                if not any(tok in lines[i - 1] for i in range(s0, min(s1, len(lines)) + 1)):
                    errs.append("D8 %s token %r is not inside declared symbol %s() (%s:%d-%d) "
                                "-> code moved out of the function or token was deleted"
                                % (did, tok, sym, f, s0, s1))
            # ── D2/D3：行号定位辅助（漂移必须显式暴露，要求同步）────────────
            if ln < 1 or ln > len(lines):
                errs.append("D2 %s evidence line out of range: %s:%d (file has %d lines)"
                            % (did, f, ln, len(lines)))
                continue
            text = lines[ln - 1]
            if tok not in text:
                errs.append("D2 %s evidence token %r not found at %s:%d -> %r (line drifted; "
                            "refresh the anchor)" % (did, tok, f, ln, text.strip()[:120]))
            stripped = text.strip()
            if not stripped or stripped.startswith("//") or stripped.startswith("#"):
                errs.append("D3 %s evidence points at empty/comment line %s:%d" % (did, f, ln))
        if d.get("severity") == "blocker":
            if not doc.get("owner_decision_required"):
                errs.append("D5 %s is blocker but register lacks owner_decision_required" % did)
            if oq_missing:
                errs.append("D7 %s is blocker but escalation anchor is MISSING: "
                            "工程控制/RELEASE-05/OPEN_QUESTIONS.md (fail-closed: "
                            "锚点缺失时 D7 无法成立，不得静默放行)" % did)
            elif did not in oq:
                errs.append("D7 %s is blocker but not escalated in "
                            "工程控制/RELEASE-05/OPEN_QUESTIONS.md" % did)
    return errs


def run(path):
    if not os.path.isfile(path):
        return ["register not found: %s" % path], None
    doc = json.load(io.open(path, encoding="utf-8"))
    return check_register(doc), doc


# 每条自检用例的**种类**（单一事实源，逐条显式列举）。
# 种类决定该用例失败时门给的是 rc=1（被判对象不合规）还是 rc=3（门自身不可信），
# 见 eng/ci/gate_trust.py 的三态口径。S1 是**内容断言**（真实册子必须干净），
# 不是判别力断言——把它与判别力用例混在一格里，正是「门自检不绿」被误读成
# 「门不可信」的根因。漏登记一条即判 CRASH（见 _self_test 末尾的覆盖性自检）。
CASE_KIND = {
    "S1-valid-green": KIND_CONTENT,
    "S2-empty-register-red": KIND_DISCRIMINATING,
    "S3-token-rot-red": KIND_DISCRIMINATING,
    "S4-line-out-of-range-red": KIND_DISCRIMINATING,
    "S5-missing-file-red": KIND_DISCRIMINATING,
    "S6-duplicate-id-red": KIND_DISCRIMINATING,
    "S7-bad-severity-red": KIND_DISCRIMINATING,
    "S8-blocker-without-owner-flag-red": KIND_DISCRIMINATING,
    "S9-comment-line-evidence-red": KIND_DISCRIMINATING,
    "S10-blocker-not-escalated-red": KIND_DISCRIMINATING,
    "S12-symbol-token-deleted-red": KIND_DISCRIMINATING,
    "S13-token-outside-declared-symbol-red": KIND_DISCRIMINATING,
    "S14-unknown-symbol-red": KIND_DISCRIMINATING,
    "S15-missing-symbol-red": KIND_DISCRIMINATING,
    "S16-ambiguous-symbol-red": KIND_DISCRIMINATING,
    "S18-file-scope-token-deleted-red": KIND_DISCRIMINATING,
    "S11-symbol-resolves-green": KIND_PROTECTIVE,
    "S17-line-drift-keeps-D8-green-D2-red": KIND_PROTECTIVE,
    # ↓ 落在本门**自身豁免分支**内的负例（见各用例处的注释）
    "X1-exempt-missing-escalation-anchor-red": KIND_EXEMPTION,
    "X2-exempt-severity-downgrade-cannot-launder-anchor-red": KIND_EXEMPTION,
}


def _self_test(json_out=None) -> int:
    doc = json.load(io.open(REGISTER, encoding="utf-8"))
    cases = []
    details = {}
    # S1 是**内容断言**（真实册子必须干净）：它红了说明**被判对象**需要维护，
    # 不说明门坏了。判词必须逐条带 文件:行 ⇒ 把 errs 全文挂到该用例的 detail 上。
    errs_s1 = check_register(doc)
    cases.append(("S1-valid-green", errs_s1 == []))
    details["S1-valid-green"] = "\n".join(errs_s1) or "(真实册子全绿)"

    d = copy.deepcopy(doc); d["deviations"] = []
    cases.append(("S2-empty-register-red", any(x.startswith("D6") for x in check_register(d))))

    d = copy.deepcopy(doc); d["deviations"][0]["evidence"][0]["contains"] = "NO_SUCH_TOKEN_XYZ"
    cases.append(("S3-token-rot-red", any(x.startswith("D2") for x in check_register(d))))

    d = copy.deepcopy(doc); d["deviations"][0]["evidence"][0]["line"] = 10 ** 9
    cases.append(("S4-line-out-of-range-red", any(x.startswith("D2") for x in check_register(d))))

    d = copy.deepcopy(doc); d["deviations"][0]["evidence"][0]["file"] = "no/such/file.cpp"
    cases.append(("S5-missing-file-red", any(x.startswith("D2") for x in check_register(d))))

    d = copy.deepcopy(doc); d["deviations"][1]["id"] = d["deviations"][0]["id"]
    cases.append(("S6-duplicate-id-red", any(x.startswith("D4") for x in check_register(d))))

    d = copy.deepcopy(doc); d["deviations"][0]["severity"] = "catastrophic"
    cases.append(("S7-bad-severity-red", any(x.startswith("D4") for x in check_register(d))))

    d = copy.deepcopy(doc); d["owner_decision_required"] = False
    cases.append(("S8-blocker-without-owner-flag-red",
                  any(x.startswith("D5") for x in check_register(d))))

    # D3：把证据指向注释行
    d = copy.deepcopy(doc)
    p = os.path.join(REPO, d["deviations"][0]["evidence"][0]["file"])
    lines = io.open(p, encoding="utf-8", errors="replace").read().splitlines()
    for i, t in enumerate(lines, 1):
        if t.strip().startswith("//"):
            d["deviations"][0]["evidence"][0]["line"] = i
            d["deviations"][0]["evidence"][0]["contains"] = t.strip()[:12]
            break
    cases.append(("S9-comment-line-evidence-red",
                  any(x.startswith("D3") for x in check_register(d))))

    # D7：删掉 OPEN_QUESTIONS 引用
    d = copy.deepcopy(doc)
    import tempfile, shutil
    tmp = tempfile.mkdtemp()
    os.makedirs(os.path.join(tmp, "工程控制/RELEASE-05"), exist_ok=True)
    io.open(os.path.join(tmp, "工程控制/RELEASE-05/OPEN_QUESTIONS.md"), "w",
            encoding="utf-8").write("no ids here\n")
    errs = check_register(d, repo=tmp)
    shutil.rmtree(tmp, ignore_errors=True)
    cases.append(("S10-blocker-not-escalated-red", any(x.startswith("D7") for x in errs)))

    # ── D8 符号级判据的正例/负例（GATE-FIX-01）────────────────────────────
    # 正例：所有 evidence 声明的 symbol 都解析到唯一函数体且 token 落在其中。
    cases.append(("S11-symbol-resolves-green",
                  not any(x.startswith("D8") for x in check_register(doc))))

    # 负例：token 从文件里彻底删除 ⇒ 符号级判据必须判红（不是「文件存在即绿」）。
    d = copy.deepcopy(doc)
    d["deviations"][0]["evidence"][0]["contains"] = "NO_SUCH_TOKEN_XYZ"
    cases.append(("S12-symbol-token-deleted-red",
                  any(x.startswith("D8") for x in check_register(d))))

    # 负例：token 真实存在于文件内，但被移出声明的函数体 ⇒ 判红（证明判据是函数体级，
    # 不是文件级；这正是「代码错位」与「文件里有这个词」的区别）。
    d = copy.deepcopy(doc)
    d["deviations"][0]["evidence"][0]["symbol"] = "p1_op_calibrate"
    cases.append(("S13-token-outside-declared-symbol-red",
                  any(x.startswith("D8") for x in check_register(d))))

    # 负例：声明的符号名不存在（函数改名/删除）⇒ 判红。
    d = copy.deepcopy(doc)
    d["deviations"][0]["evidence"][0]["symbol"] = "p1_op_no_such_function"
    cases.append(("S14-unknown-symbol-red",
                  any(x.startswith("D8") for x in check_register(d))))

    # 负例：evidence 不声明 symbol ⇒ 判红（不允许退回「只有行号」的脆弱形态）。
    d = copy.deepcopy(doc)
    d["deviations"][0]["evidence"][0].pop("symbol", None)
    cases.append(("S15-missing-symbol-red",
                  any(x.startswith("D8") for x in check_register(d))))

    # 负例：声明的符号在文件内有多个定义（无法判定归属）⇒ 判红。
    d = copy.deepcopy(doc)
    d["deviations"][0]["evidence"][0]["symbol"] = "last_error"
    cases.append(("S16-ambiguous-symbol-red",
                  any(x.startswith("D8") for x in check_register(d))))

    # 正例（抗漂移语义）：把某条 evidence 的行号改错但保持 token 与 symbol 不变 ⇒
    # D8 必须保持为绿（token 仍在声明的函数体内），而 D2 必须判红（行号需刷新）。
    # 这正是「代码移动」与「断言失效」的区分：漂移不再等价于断言不成立。
    d = copy.deepcopy(doc)
    d["deviations"][0]["evidence"][0]["line"] = 1
    errs_drift = check_register(d)
    cases.append(("S17-line-drift-keeps-D8-green-D2-red",
                  (not any(x.startswith("D8") for x in errs_drift)) and
                  any(x.startswith("D2") for x in errs_drift)))

    # 负例：file 级锚点的 token 被删除 ⇒ 判红（file 级也不是「文件存在即绿」）。
    d = copy.deepcopy(doc)
    for dev in d["deviations"]:
        if dev["id"] == "BFD-C1":
            dev["evidence"][0]["contains"] = "NO_SUCH_TOKEN_XYZ"
            break
    cases.append(("S18-file-scope-token-deleted-red",
                  any(x.startswith("D8") for x in check_register(d))))

    # ── 落在本门**自身豁免分支**内的负例（GATE-TRUST-01）────────────────────
    # 分支①：D7 的守卫原本是 `if ... and oq and did not in oq` —— 上呈锚点文件
    # 不存在时 `oq` 为空 ⇒ D7 整条不判。这是一条**静默豁免**：删掉锚点文件就能让
    # blocker 免于「登记了但没上呈」的判据。负例把违规喂进这条分支：临时 repo 里
    # **不放** OPEN_QUESTIONS.md，且册中 blocker 未上呈 ⇒ 必须判红。
    d = copy.deepcopy(doc)
    tmp2 = tempfile.mkdtemp()   # 故意不建 工程控制/RELEASE-05/OPEN_QUESTIONS.md
    errs_no_anchor = check_register(d, repo=tmp2)
    shutil.rmtree(tmp2, ignore_errors=True)
    cases.append(("X1-exempt-missing-escalation-anchor-red",
                  any(x.startswith("D7") for x in errs_no_anchor)))
    details["X1-exempt-missing-escalation-anchor-red"] = "\n".join(errs_no_anchor)

    # 分支②：D5/D7 只对 `severity == "blocker"` 生效 ⇒「非 blocker」是一条豁免面。
    # 负例把违规喂进这条分支：把一条 blocker 降级成合法非 blocker 值（major），
    # 同时把它的 evidence 锚点弄坏 —— **降级不得给坏锚点洗白**（D8/D2 与 severity
    # 正交，必须照常判红）。只验「blocker 的正确性」会漏掉这条。
    d = copy.deepcopy(doc)
    d["deviations"][0]["severity"] = "major"
    d["deviations"][0]["evidence"][0]["contains"] = "NO_SUCH_TOKEN_XYZ"
    errs_downgraded = check_register(d)
    cases.append(("X2-exempt-severity-downgrade-cannot-launder-anchor-red",
                  any(x.startswith("D8") for x in errs_downgraded)))
    details["X2-exempt-severity-downgrade-cannot-launder-anchor-red"] = \
        "\n".join(errs_downgraded)

    # 种类表覆盖性自检：漏登记一条用例的种类 ⇒ 三态会失真 ⇒ 本身就是门不可信。
    uncovered = [n for n, _g in cases if n not in CASE_KIND]
    if uncovered:
        cases.append(("S0-case-kind-table-must-cover-all-cases", False))

    items = [(n, g, CASE_KIND.get(n, KIND_DISCRIMINATING), details.get(n, ""))
             for n, g in cases]
    return emit(items, tool="check_block_flow_conformance", json_out=json_out,
                extra={"case_kind_table_covers_all": not uncovered,
                       "uncovered_cases": uncovered})


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--register", default=REGISTER)
    ap.add_argument("--json-out")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        # 注册表按 outputs 判 missing_output：自测也必须落机器可读证据（含三态 verdict）。
        return _self_test(json_out=args.json_out)
    errs, doc = run(args.register)
    verdict = "PASS" if not errs else "FAIL"
    n = len(doc["deviations"]) if doc else 0
    nb = sum(1 for d in (doc or {}).get("deviations", []) if d.get("severity") == "blocker")
    ne = sum(len(d.get("evidence") or []) for d in (doc or {}).get("deviations", []))
    print("BLOCK_FLOW_CONFORMANCE_%s: deviations=%d blockers=%d evidence=%d errors=%d"
          % (verdict, n, nb, ne, len(errs)))
    # 判词逐条全量打印，**不截断**（旧实现 `errs[:40]` 会让「共 N 条」只列 40 条，
    # 读者据此无法确认其余锚点是否也被放宽）。
    for e in errs:
        print("  " + e)
    if args.json_out:
        os.makedirs(os.path.dirname(args.json_out), exist_ok=True)
        io.open(args.json_out, "w", encoding="utf-8").write(json.dumps(
            {"tool": "check_block_flow_conformance", "register": args.register,
             "deviations": n, "blockers": nb, "evidence": ne, "errors": errs,
             "verdict": verdict}, ensure_ascii=False, indent=1) + "\n")
    return 0 if not errs else 1


if __name__ == "__main__":
    sys.exit(main())
