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
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
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
    if os.path.isfile(oqp):
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
        if d.get("severity") == "blocker" and not doc.get("owner_decision_required"):
            errs.append("D5 %s is blocker but register lacks owner_decision_required" % did)
        if d.get("severity") == "blocker" and oq and did not in oq:
            errs.append("D7 %s is blocker but not escalated in OPEN_QUESTIONS.md" % did)
    return errs


def run(path):
    if not os.path.isfile(path):
        return ["register not found: %s" % path], None
    doc = json.load(io.open(path, encoding="utf-8"))
    return check_register(doc), doc


def _self_test():
    doc = json.load(io.open(REGISTER, encoding="utf-8"))
    cases = []
    cases.append(("S1-valid-green", check_register(doc) == []))

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

    bad = [n for n, g in cases if not g]
    for n, g in cases:
        print("SELFTEST " + ("PASS " if g else "FAIL ") + n)
    if bad:
        print("SELFTEST_FAIL: " + repr(bad), file=sys.stderr)
        return 1
    print("SELFTEST_PASS: %d/%d" % (len(cases), len(cases)))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--register", default=REGISTER)
    ap.add_argument("--json-out")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        rc = _self_test()
        # 注册表按 outputs 判 missing_output：自测也必须落机器可读证据。
        if getattr(args, "json_out", None):
            os.makedirs(os.path.dirname(args.json_out), exist_ok=True)
            io.open(args.json_out, "w", encoding="utf-8").write(json.dumps(
                {"tool": "block_flow_conformance", "mode": "self-test", "rc": rc,
                 "verdict": "PASS" if rc == 0 else "FAIL"}, ensure_ascii=False) + "\n")
        return rc
    errs, doc = run(args.register)
    verdict = "PASS" if not errs else "FAIL"
    n = len(doc["deviations"]) if doc else 0
    nb = sum(1 for d in (doc or {}).get("deviations", []) if d.get("severity") == "blocker")
    ne = sum(len(d.get("evidence") or []) for d in (doc or {}).get("deviations", []))
    print("BLOCK_FLOW_CONFORMANCE_%s: deviations=%d blockers=%d evidence=%d errors=%d"
          % (verdict, n, nb, ne, len(errs)))
    for e in errs[:40]:
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
