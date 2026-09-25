#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_conclusion_truth.py —— 结论真实性门（一页纸 S2-B：状态词与覆盖度结论失真）。

权威依据
  - ASTROCS_DESIGN.md 12.5 状态阶梯（唯一口径）—— 状态词的**唯一**词汇来源；
  - docs/ci/01_CHECKS.md 1（eng/ci/checks.json 唯一注册表）；
  - ENGINEERING_SPEC.md 8（fail-closed；每项能绿能红）；
  - AGENTS.md 6（不用 waiver 盖红灯）。

要修的缺陷（审查节点原话）
  1. DELIVERED 是**造词**：最高设计的唯一状态阶梯里没有这个词；
  2. waivers.json 被引用而全仓不存在（真实豁免面是 eng/ci/exemptions.json）；
  3. tool_missing=true 与 coverage_ok=true 并存（自相矛盾）、分母 713 全仓无定义；
  4. 某门在 checks.json 零登记而对外快照停在旧版本。

四条判据（对应 S2-B「判完成」）
  T1 vocab     ：状态词只取阶梯内词汇（阶梯**逐字**解析自 ASTROCS_DESIGN.md 12.5，
                 registry 里的快照必须与解析结果相等 ⇒ 词汇表无法单方面漂移）；越词即红。
  T2 surface   ：豁免/权威引用必须解析到**真实载体**；登记的别名（如 waivers.json）
                 出现在在役面即红；载体文件缺失、不可解析或零引用同样红。
  T3 same_src  ：覆盖度结论的分子与分母必须由**同一条命令**产出，且能被该命令**复算**；
                 并禁止「工具缺失却覆盖达标」这类自相矛盾并存键。
  T4 snapshot  ：被登记的门必须在 checks.json 有注册承载；其对外快照必须与**现役**输出
                 逐项相等；记录面里残留旧快照即红。

状态阶梯（12.5）与门退出码判定词（verdict）是两套词汇，不得互相顶替；
vocabulary 的 denylist 显式点名造词，避免「用判定词冒充状态」。

用法
  python3 eng/tools/quality/check_conclusion_truth.py [--root DIR] [--json-out PATH] [--quiet]
  python3 eng/tools/quality/check_conclusion_truth.py --self-test
  python3 eng/tools/quality/check_conclusion_truth.py --fault-inject <vocab|surface|coverage|snapshot>
退出码 0 = 全绿；1 = 至少一条判红；2 = 用法/夹具错误。
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

REG = "eng/tools/quality/conclusion_vocabulary.json"
SURF = "eng/tools/quality/authority_surfaces.json"
CLAIMS = "eng/tools/quality/coverage_claims.json"
SNAPS = "eng/tools/quality/conclusion_snapshots.json"
CHECKS = "eng/ci/checks.json"
DESIGN = "ASTROCS_DESIGN.md"
LADDER_HEAD = "### 12.5 状态阶梯"
UPPER = re.compile(r"^[A-Z][A-Z0-9_]{1,}$")
# 规则表自身（词汇表/别名表/claims/快照）与门的负例夹具必然包含被禁字面量，
# 故不参与扫描——这是"规则不给自己判红"，不是给被测面开口子：被测面一律全扫。
SELF_FILES = (REG, SURF, CLAIMS, SNAPS, "eng/tools/quality/check_conclusion_truth.py")


def _read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def _load(root, rel):
    """读 registry；缺失/坏 JSON ⇒ (None, [fail])，调用方 fail-closed。"""
    p = os.path.join(root, rel)
    if not os.path.isfile(p):
        return None, ["registry 缺失: %s（fail-closed，不得静默跳过）" % rel]
    try:
        return json.loads(_read(p)), []
    except Exception as exc:  # noqa: BLE001
        return None, ["registry 不可解析: %s: %s" % (rel, exc)]


def _ptr(doc, ptr):
    cur = doc
    for tok in [t for t in ptr.split("/") if t]:
        if isinstance(cur, list):
            cur = cur[int(tok)]
        elif isinstance(cur, dict):
            if tok not in cur:
                return None
            cur = cur[tok]
        else:
            return None
    return cur


# ── T1 状态词阶梯 ────────────────────────────────────────────────────────────
def parse_ladder(root):
    """从 ASTROCS_DESIGN.md 12.5 表格逐字解析状态词。缺节即红（fail-closed）。"""
    p = os.path.join(root, DESIGN)
    if not os.path.isfile(p):
        return [], ["%s 缺失 ⇒ 状态阶梯唯一口径不可解析（fail-closed）" % DESIGN]
    text = _read(p)
    i = text.find(LADDER_HEAD)
    if i < 0:
        return [], ["%s 缺 %s 节 ⇒ 状态阶梯唯一口径不可解析" % (DESIGN, LADDER_HEAD)]
    seg = text[i:]
    j = seg.find("\n---", 1)
    if j > 0:
        seg = seg[:j]
    words = []
    for line in seg.splitlines():
        line = line.strip()
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 2:
            continue
        head = cells[0]
        if not head or head == "状态":
            continue
        for w in head.split("/"):
            w = w.strip()
            if UPPER.match(w):
                words.append(w)
    if not words:
        return [], ["%s 12.5 解析不出任何状态词（表格形态变了？）" % DESIGN]
    return words, []


def check_vocab(root):
    fails = []
    ladder, lfails = parse_ladder(root)
    fails += lfails
    reg, rf = _load(root, REG)
    fails += rf
    if reg is None:
        return fails
    snap = reg.get("ladder_snapshot") or []
    if sorted(snap) != sorted(ladder):
        fails.append("阶梯快照与 %s 12.5 不等（registry 单方面漂移）：registry=%s 现役=%s"
                     % (DESIGN, sorted(snap), sorted(ladder)))
    verdict = reg.get("verdict") or {}
    denylist = reg.get("denylist") or {}
    allowed = set(ladder) | set(verdict)
    # (a) 门结果形态：同一对象/近邻同时出现 status 与 passed
    for base in reg.get("scan_roots", ["eng", "artifacts/evidence"]):
        base_abs = os.path.join(root, base)
        for dirpath, dirnames, filenames in os.walk(base_abs):
            dirnames[:] = [d for d in dirnames if d not in ("__pycache__", ".git")]
            for fn in filenames:
                rel = os.path.relpath(os.path.join(dirpath, fn), root)
                if rel in SELF_FILES:
                    continue
                full = os.path.join(dirpath, fn)
                if fn.endswith(".py"):
                    try:
                        t = _read(full)
                    except Exception:  # noqa: BLE001
                        continue
                    for m in re.finditer(r'"(?:status|state)"\s*:\s*"([A-Z][A-Z0-9_]{1,})"', t):
                        seg = t[max(0, m.start() - 400):m.start() + 400]
                        if '"passed"' in seg and m.group(1) not in allowed:
                            ln = t[:m.start()].count("\n") + 1
                            fails.append("状态词越词（阶梯/判定词都不含）: %s:%d %s"
                                         % (rel, ln, m.group(1)))
                elif fn.endswith(".json"):
                    try:
                        doc = json.loads(_read(full))
                    except Exception:  # noqa: BLE001
                        continue
                    stack = [doc]
                    while stack:
                        n = stack.pop()
                        if isinstance(n, dict):
                            st = n.get("status")
                            if (isinstance(st, str) and UPPER.match(st)
                                    and ("passed" in n or "pass" in n or "tool" in n)
                                    and st not in allowed):
                                fails.append("状态词越词（门结果面）: %s status=%s" % (rel, st))
                            stack.extend(n.values())
                        elif isinstance(n, list):
                            stack.extend(n)
    # (b) 造词黑名单：在役面出现即红（不依赖门结果形态）
    for base in reg.get("denylist_roots", ["eng", "docs/ci"]):
        base_abs = os.path.join(root, base)
        targets = []
        if os.path.isfile(base_abs):
            targets = [base_abs]
        else:
            for dirpath, dirnames, filenames in os.walk(base_abs):
                dirnames[:] = [d for d in dirnames if d not in ("__pycache__", ".git")]
                targets += [os.path.join(dirpath, f) for f in filenames]
        for full in targets:
            rel = os.path.relpath(full, root)
            if rel in SELF_FILES:
                continue
            try:
                t = _read(full)
            except Exception:  # noqa: BLE001
                continue
            for word in denylist:
                if re.search(r"\b%s\b" % re.escape(word), t):
                    ln = next((i + 1 for i, L in enumerate(t.splitlines())
                               if re.search(r"\b%s\b" % re.escape(word), L)), 0)
                    fails.append("造词越词: %s:%d 出现 %s（%s）"
                                 % (rel, ln, word, denylist[word]))
    return fails


# ── T2 权威/豁免载体引用 ─────────────────────────────────────────────────────
def check_surfaces(root):
    fails = []
    reg, rf = _load(root, SURF)
    fails += rf
    if reg is None:
        return fails
    roots = reg.get("scan_roots", ["eng", "docs/ci"])
    exclude = set(reg.get("scan_exclude", [])) | set(SELF_FILES)
    files = []
    for base in roots:
        p = os.path.join(root, base)
        if os.path.isfile(p):
            files.append(p)
            continue
        for dirpath, dirnames, filenames in os.walk(p):
            dirnames[:] = [d for d in dirnames if d not in ("__pycache__", ".git", "node_modules")]
            files += [os.path.join(dirpath, f) for f in filenames]
    for s in reg.get("surfaces", []):
        canon = s["canonical"]
        cp = os.path.join(root, canon)
        if not os.path.isfile(cp):
            fails.append("权威载体缺失: %s（%s；%s）" % (canon, s["id"], s.get("authority", "")))
            continue
        if canon.endswith(".json"):
            try:
                json.loads(_read(cp))
            except Exception as exc:  # noqa: BLE001
                fails.append("权威载体不可解析: %s: %s" % (canon, exc))
        referenced = False
        for full in files:
            rel = os.path.relpath(full, root)
            if rel in exclude:
                continue
            try:
                t = _read(full)
            except Exception:  # noqa: BLE001
                continue
            if canon in t:
                referenced = True
            for alias in s.get("aliases", []):
                if alias == os.path.basename(canon):
                    continue
                for ln, line in enumerate(t.splitlines(), 1):
                    if re.search(r"(?<![\w/.-])%s(?![\w-])" % re.escape(alias), line):
                        fails.append("悬空引用: %s:%d 引用了不存在的 %s（真实载体 = %s）"
                                     % (rel, ln, alias, canon))
        if s.get("must_be_referenced", True) and not referenced:
            fails.append("权威载体零引用: %s 无任何在役面引用（禁止空载登记）" % canon)
    return fails


# ── T3 分子/分母同源 + 并存键自洽 ────────────────────────────────────────────
def _walk_dicts(node):
    stack = [node]
    while stack:
        n = stack.pop()
        if isinstance(n, dict):
            yield n
            stack.extend(n.values())
        elif isinstance(n, list):
            stack.extend(n)


def _walk_paths(node, path=()):
    """产出 (键路径, 对象)，用于跨对象判据（如 tool_missing 在 A 对象而 <tool>_ok 在 B 对象）。"""
    stack = [(node, path)]
    while stack:
        n, p = stack.pop()
        if isinstance(n, dict):
            yield p, n
            for k, v in n.items():
                stack.append((v, p + (k,)))
        elif isinstance(n, list):
            for i, v in enumerate(n):
                stack.append((v, p + (i,)))


def check_claims(root, recompute=True):
    fails = []
    reg, rf = _load(root, CLAIMS)
    fails += rf
    if reg is None:
        return fails
    for claim in reg.get("claims", []):
        cid = claim["id"]
        ap = os.path.join(root, claim["artifact"])
        if not os.path.isfile(ap):
            fails.append("覆盖度结论缺产物（fail-closed）: %s <- %s" % (claim["artifact"], cid))
            continue
        try:
            art = json.loads(_read(ap))
        except Exception as exc:  # noqa: BLE001
            fails.append("覆盖度结论不可解析: %s: %s" % (claim["artifact"], exc))
            continue
        num = _ptr(art, claim["numerator_pointer"])
        den = _ptr(art, claim["denominator_pointer"])
        src = _ptr(art, claim.get("source_command_pointer", "/source_command"))
        if not isinstance(num, (int, float)) or not isinstance(den, (int, float)):
            fails.append("%s: 分子/分母缺失或非数值 (num=%r den=%r)" % (cid, num, den))
        else:
            if den <= 0:
                fails.append("%s: 分母 <= 0（未定义分母）" % cid)
            if num > den:
                fails.append("%s: 分子 %r > 分母 %r（比值不可解释）" % (cid, num, den))
        want_src = claim.get("expected_source_command")
        if want_src is not None and src != want_src:
            fails.append("%s: source_command=%r 与登记的唯一命令 %r 不等（不同源）"
                         % (cid, src, want_src))
        if src is None and want_src is None:
            fails.append("%s: 结论未登记产出它的命令（分子/分母无法同源复算）" % cid)
        if claim.get("denominator_definition_pointer"):
            if not _ptr(art, claim["denominator_definition_pointer"]):
                fails.append("%s: 分母无定义（分母必须随产物落盘定义）" % cid)
        if recompute and claim.get("recompute_command"):
            cmd = [str(x) for x in claim["recompute_command"]]
            with tempfile.TemporaryDirectory() as td:
                tmp = os.path.join(td, "recomputed.json")
                cmd = [tmp if x == "{tmp}" else x for x in cmd]
                try:
                    r = subprocess.run(cmd, cwd=root, capture_output=True, text=True, timeout=600)
                except Exception as exc:  # noqa: BLE001
                    fails.append("%s: 复算命令无法运行: %s" % (cid, exc))
                    continue
                if r.returncode != 0 or not os.path.isfile(tmp):
                    fails.append("%s: 复算命令 rc=%d 且未产出（分子/分母不可复算）"
                                 % (cid, r.returncode))
                    continue
                fresh = json.loads(_read(tmp))
            fn = _ptr(fresh, claim["numerator_pointer"])
            fd = _ptr(fresh, claim["denominator_pointer"])
            if (fn, fd) != (num, den):
                fails.append("%s: 产物 %r 与同一命令复算 %r 不等（结论不可核）"
                             % (cid, (num, den), (fn, fd)))
    # 并存键自洽：工具缺失却「覆盖达标」这类自相矛盾
    pairs = [tuple(p) for p in reg.get("contradiction_pairs", [])]
    for pattern in reg.get("contradiction_docs", []):
        for full in sorted(glob.glob(os.path.join(root, pattern))):
            rel = os.path.relpath(full, root)
            try:
                doc = json.loads(_read(full))
            except Exception:  # noqa: BLE001
                continue
            for obj in _walk_dicts(doc):
                for a, b in pairs:
                    if obj.get(a) is True and obj.get(b) is True:
                        fails.append("自相矛盾并存: %s 同时 %s=true 与 %s=true" % (rel, a, b))
                if obj.get("tool_missing") is True:
                    for k, v in obj.items():
                        if k != "tool_missing" and k.endswith("_ok") and v is True:
                            fails.append("自相矛盾并存: %s 同时 tool_missing=true 与 %s=true"
                                         % (rel, k))
            # 跨对象同名判据：A 对象声明某工具缺失，全文任何 <工具名>*_ok=true 即自相矛盾
            #（实例：before.file_audit.tool_missing=true 与 gate_before.file_audit_coverage_ok=true）
            walked = list(_walk_paths(doc))
            for p, obj in walked:
                if obj.get("tool_missing") is not True or not p:
                    continue
                prefix = str(p[-1])
                for _, other in walked:
                    if other is obj:
                        continue
                    for k, v in other.items():
                        if v is True and k.endswith("_ok") and k.startswith(prefix):
                            fails.append("自相矛盾并存（跨对象）: %s 声明 %s.tool_missing=true，"
                                         "同一结论件里 %s=true" % (rel, prefix, k))
    return fails


# ── T4 快照与现役对齐 ────────────────────────────────────────────────────────
def check_snapshots(root):
    fails = []
    reg, rf = _load(root, SNAPS)
    fails += rf
    if reg is None:
        return fails
    checks_text = ""
    ck = os.path.join(root, CHECKS)
    if os.path.isfile(ck):
        checks_text = _read(ck)
    for snap in reg.get("snapshots", []):
        sid = snap["id"]
        rid = snap.get("registration_check_id")
        if rid and rid not in checks_text:
            fails.append("%s: 门 %s 在 %s 零登记（未登记的判据不受机器门约束）"
                         % (sid, rid, CHECKS))
        recorded = snap.get("recorded") or {}
        if snap.get("live_command"):
            live = None
            with tempfile.TemporaryDirectory() as td:
                tmp = os.path.join(td, "live.json")
                cmd = [tmp if x == "{tmp}" else x for x in snap["live_command"]]
                try:
                    r = subprocess.run(cmd, cwd=root, capture_output=True, text=True, timeout=1800)
                except Exception as exc:  # noqa: BLE001
                    fails.append("%s: 现役命令无法运行: %s" % (sid, exc))
                    r = None
                if r is not None:
                    if not os.path.isfile(tmp):
                        fails.append("%s: 现役命令 rc=%d 未产出（快照无法与现役对齐）"
                                     % (sid, r.returncode))
                    else:
                        live = json.loads(_read(tmp))
            if live is not None:
                got = {}
                for key, expr in (snap.get("live_extract") or {}).items():
                    if expr == "__count_checks__":
                        got[key] = len(live.get("checks") or [])
                    else:
                        got[key] = _ptr(live, "/" + expr)
                for key, want in recorded.items():
                    if got.get(key) != want:
                        fails.append("%s: 快照 %s=%r 与现役 %r 不等"
                                     % (sid, key, want, got.get(key)))
        for rec in snap.get("recorded_in", []):
            rel = rec["path"] if isinstance(rec, dict) else rec
            full = os.path.join(root, rel)
            if not os.path.isfile(full):
                fails.append("%s: 记录面 %s 不存在" % (sid, rel))
                continue
            if not isinstance(rec, dict):
                continue
            try:
                doc = json.loads(_read(full))
            except Exception as exc:  # noqa: BLE001
                fails.append("%s: 记录面 %s 不可解析: %s" % (sid, rel, exc))
                continue
            node = _ptr(doc, rec["pointer"])
            if not isinstance(node, dict):
                fails.append("%s: 记录面 %s%s 不是对象（口径变了？）"
                             % (sid, rel, rec["pointer"]))
                continue
            for key, field in (rec.get("keys") or {}).items():
                want = recorded.get(key)
                got = node.get(field)
                live_key = field + "_live"
                if node.get("superseded_by"):
                    if node.get(live_key) != want:
                        fails.append("%s: 记录面 %s%s 已标 superseded_by，但其 %s=%r 与现役 %r "
                                     "不等（标注必须指向现役真值）"
                                     % (sid, rel, rec["pointer"], live_key,
                                        node.get(live_key), want))
                elif got != want:
                    fails.append("%s: 记录面 %s%s 残留旧快照 %s=%r（现役 %r）；"
                                 "历史值必须显式标注 superseded_by 并给出 %s"
                                 % (sid, rel, rec["pointer"], field, got, want, live_key))
    return fails


CHECKS_MAP = [
    ("T1-vocab", check_vocab),
    ("T2-surface", check_surfaces),
    ("T3-same-source", check_claims),
    ("T4-snapshot", check_snapshots),
]
FAULT_TARGET = {"vocab": check_vocab, "surface": check_surfaces,
                "coverage": check_claims, "snapshot": check_snapshots}


def run(root):
    results = []
    for name, fn in CHECKS_MAP:
        fails = fn(root)
        results.append({"check": name, "pass": not fails, "fails": fails})
    return {"tool": "check_conclusion_truth", "root": os.path.abspath(root),
            "results": results, "pass": all(x["pass"] for x in results)}


# ── 自证（mini-repo 正/负例；负例必须判红）────────────────────────────────────
LADDER_DOC = """# 设计

### 12.5 状态阶梯（唯一口径）

| 状态 | 语义 |
|---|---|
| CONTRACT_READY | a |
| IMPLEMENTED | b |
| INSTALLED | c |
| VERIFIED | d |
| READY_FOR_OWNER_REVIEW | e |
| NOT_IMPLEMENTED / NOT_VERIFIED / DEFERRED / DORMANT / FAIL | 负向 |

---
"""


def _mini(root, *, ladder=None, verdict=None, denylist=None, surfaces=None,
          claims=None, snaps=None, checks_text="{}"):
    os.makedirs(root, exist_ok=True)
    with open(os.path.join(root, DESIGN), "w", encoding="utf-8") as f:
        f.write(LADDER_DOC)
    os.makedirs(os.path.join(root, "eng/tools/quality"), exist_ok=True)
    os.makedirs(os.path.join(root, "eng/ci"), exist_ok=True)
    with open(os.path.join(root, CHECKS), "w", encoding="utf-8") as f:
        f.write(checks_text)
    lad, _ = parse_ladder(root)
    voc = {"schema_version": 1, "ladder_snapshot": lad if ladder is None else ladder,
           "verdict": {"PASS": "x", "FAIL": "x"} if verdict is None else verdict,
           "denylist": {"DELIVERED": "造词"} if denylist is None else denylist,
           "scan_roots": ["eng"], "denylist_roots": ["eng"]}
    with open(os.path.join(root, REG), "w", encoding="utf-8") as f:
        json.dump(voc, f)
    with open(os.path.join(root, SURF), "w", encoding="utf-8") as f:
        json.dump({"schema_version": 1, "scan_roots": ["eng"],
                   "surfaces": surfaces if surfaces is not None else []}, f)
    with open(os.path.join(root, CLAIMS), "w", encoding="utf-8") as f:
        json.dump(claims if claims is not None else
                  {"schema_version": 1, "claims": [], "contradiction_pairs": [],
                   "contradiction_docs": []}, f)
    with open(os.path.join(root, SNAPS), "w", encoding="utf-8") as f:
        json.dump({"schema_version": 1, "snapshots": snaps or []}, f)


def self_test():
    cases = []
    with tempfile.TemporaryDirectory() as td:
        pos = os.path.join(td, "pos")
        _mini(pos)
        with open(os.path.join(pos, "eng/ci/gate_ok.py"), "w", encoding="utf-8") as f:
            f.write('r = {"status": "PASS", "passed": True}\n')
        cases.append(("pos-clean", run(pos)["pass"], True))
        n1 = os.path.join(td, "n1")
        _mini(n1)
        with open(os.path.join(n1, "eng/ci/gate_bad.py"), "w", encoding="utf-8") as f:
            f.write('r = {"status": "DELIVERED", "passed": True}\n')
        cases.append(("neg-coined-word", run(n1)["pass"], False))
        n2 = os.path.join(td, "n2")
        _mini(n2, ladder=["CONTRACT_READY", "IMPLEMENTED", "DELIVERED"])
        cases.append(("neg-ladder-drift", run(n2)["pass"], False))
        n3 = os.path.join(td, "n3")
        _mini(n3, surfaces=[{"id": "exemptions", "canonical": "eng/ci/exemptions.json",
                             "aliases": ["waivers.json"], "authority": "x"}])
        with open(os.path.join(n3, "eng/ci/exemptions.json"), "w") as f:
            f.write("{}")
        with open(os.path.join(n3, "eng/ci/gate_ref.py"), "w", encoding="utf-8") as f:
            f.write('w = repo / "waivers.json"\n')
        cases.append(("neg-dangling-alias", run(n3)["pass"], False))
        p3 = os.path.join(td, "p3")
        _mini(p3, surfaces=[{"id": "exemptions", "canonical": "eng/ci/exemptions.json",
                             "aliases": ["waivers.json"], "authority": "x"}])
        with open(os.path.join(p3, "eng/ci/exemptions.json"), "w") as f:
            f.write("{}")
        with open(os.path.join(p3, "eng/ci/gate_ref.py"), "w", encoding="utf-8") as f:
            f.write('w = repo / "eng/ci/exemptions.json"\n')
        cases.append(("pos-real-carrier", run(p3)["pass"], True))
        n4 = os.path.join(td, "n4")
        _mini(n4, claims={"schema_version": 1, "claims": [],
                          "contradiction_pairs": [], "contradiction_docs": ["ev/*.json"]})
        os.makedirs(os.path.join(n4, "ev"), exist_ok=True)
        with open(os.path.join(n4, "ev/a.json"), "w") as f:
            json.dump({"tool_missing": True, "coverage_ok": True}, f)
        cases.append(("neg-contradiction", run(n4)["pass"], False))
        n5 = os.path.join(td, "n5")
        _mini(n5, claims={"schema_version": 1, "claims": [{
            "id": "c", "artifact": "ev/a.json", "numerator_pointer": "/counts/n",
            "denominator_pointer": "/counts/d", "source_command_pointer": "/cmd",
            "expected_source_command": ["git", "ls-files", "-z"],
            "recompute_command": ["python3", "mk.py", "--out", "{tmp}"]}],
            "contradiction_pairs": [], "contradiction_docs": []})
        os.makedirs(os.path.join(n5, "ev"), exist_ok=True)
        with open(os.path.join(n5, "ev/a.json"), "w") as f:
            json.dump({"counts": {"n": 9, "d": 10}, "cmd": ["git", "ls-files", "-z"]}, f)
        with open(os.path.join(n5, "mk.py"), "w", encoding="utf-8") as f:
            f.write("import argparse,json\n"
                    "p=argparse.ArgumentParser();p.add_argument('--out');a=p.parse_args()\n"
                    "json.dump({'counts':{'n':8,'d':10}},open(a.out,'w'))\n")
        cases.append(("neg-recompute-drift", run(n5)["pass"], False))
        snap6 = {"id": "prod", "registration_check_id": "CHK-NOT-REGISTERED",
                 "recorded": {"checks": 11},
                 "recorded_in": [{"path": "ev/b.json", "pointer": "/mc",
                                  "keys": {"checks": "checks"}}]}
        n6 = os.path.join(td, "n6")
        _mini(n6, checks_text="{}", snaps=[snap6])
        os.makedirs(os.path.join(n6, "ev"), exist_ok=True)
        with open(os.path.join(n6, "ev/b.json"), "w") as f:
            json.dump({"mc": {"checks": 9}}, f)
        cases.append(("neg-unregistered+stale", run(n6)["pass"], False))
        p6 = os.path.join(td, "p6")
        _mini(p6, checks_text='{"checks": [{"id": "CHK-NOT-REGISTERED"}]}', snaps=[snap6])
        os.makedirs(os.path.join(p6, "ev"), exist_ok=True)
        with open(os.path.join(p6, "ev/b.json"), "w") as f:
            json.dump({"mc": {"checks": 9, "superseded_by": "eng/tools/docs_machine_consistency.py",
                              "checks_live": 11}}, f)
        cases.append(("pos-superseded-labelled", run(p6)["pass"], True))
    ok = all(got is want for _, got, want in cases)
    for name, got, want in cases:
        print("SELFTEST_%s %s (pass=%s want=%s)" % ("PASS" if got is want else "FAIL",
                                                    name, got, want))
    print("SELF_TEST %s cases=%d" % ("PASS" if ok else "FAIL", len(cases)))
    return 0 if ok else 1


MIRROR = ("eng", "docs/ci", "artifacts/evidence/truthful-conclusion-01",
          "artifacts/evidence/v19r7-quality",
          DESIGN, "ENGINEERING_SPEC.md", "ACCEPTANCE_SPEC.md")
MIRROR_EMPTY = ("lib", "docs")


def _mirror(root, dst):
    """把判据所及的仓库面复制到临时树（含空 lib/ docs/ 令现役命令可跑）。

    只做"外部注入"的观测面，不动真仓一个字节。"""
    def _ig(_d, names):
        return [n for n in names if n == "__pycache__"]
    for rel in MIRROR:
        src = os.path.join(root, rel)
        d = os.path.join(dst, rel)
        if os.path.isdir(src):
            shutil.copytree(src, d, ignore=_ig)
        elif os.path.isfile(src):
            os.makedirs(os.path.dirname(d), exist_ok=True)
            shutil.copy(src, d)
    for rel in MIRROR_EMPTY:
        os.makedirs(os.path.join(dst, rel), exist_ok=True)


def fault_inject(name, root):
    """真仓文件复制到临时树后注入违规；目标判据必须判红，且其余判据保持绿
    （判别力自证：红是被注入的那一处引起的，不是镜像面残缺引起的）。"""
    with tempfile.TemporaryDirectory() as td:
        dst = os.path.join(td, "repo")
        _mirror(root, dst)
        if name == "vocab":
            os.makedirs(os.path.join(dst, "eng/ci"), exist_ok=True)
            with open(os.path.join(dst, "eng/ci/inj.py"), "w", encoding="utf-8") as f:
                f.write('r = {"status": "DELIVERED", "passed": True}\n')
        elif name == "surface":
            os.makedirs(os.path.join(dst, "eng/ci"), exist_ok=True)
            src = os.path.join(root, "eng/ci/exemptions.json")
            if os.path.isfile(src):
                shutil.copy(src, os.path.join(dst, "eng/ci/exemptions.json"))
            with open(os.path.join(dst, "eng/ci/inj.py"), "w", encoding="utf-8") as f:
                f.write('w = repo / "waivers.json"\n')
        elif name == "coverage":
            # 把分母硬编成另一个值（713 = 审查节点点名过的"全仓无定义"分母）
            reg = json.loads(_read(os.path.join(root, CLAIMS)))
            art = reg["claims"][0]["artifact"]
            d = os.path.join(dst, art)
            os.makedirs(os.path.dirname(d), exist_ok=True)
            doc = json.loads(_read(os.path.join(root, art)))
            doc.setdefault("counts", {})["shipping_total"] = 713
            with open(d, "w", encoding="utf-8") as f:
                json.dump(doc, f)
        elif name == "snapshot":
            reg = json.loads(_read(os.path.join(root, SNAPS)))
            reg["snapshots"][0]["recorded"]["checks"] = 9
            with open(os.path.join(dst, SNAPS), "w", encoding="utf-8") as f:
                json.dump(reg, f)
        elif name == "all":
            pass
        else:
            print("unknown fault: %s" % name, file=sys.stderr)
            return 2
        target = FAULT_TARGET[name]
        # 镜像树不是 git 仓 ⇒ 复算臂（git ls-files）在镜像是不可用的，故镜像内一律
        # recompute=False；复算臂本身由 --self-test 的 neg-recompute-drift 与真仓
        # 运行（recompute=True）覆盖。
        res = {}
        for cname, cfn in CHECKS_MAP:
            res[cname] = check_claims(dst, recompute=False) if cfn is check_claims else cfn(dst)
        tgt_name = [n for n, f in CHECKS_MAP if f is target][0]
        tgt_fails = res[tgt_name]
        others = [n for n, f in res.items() if n != tgt_name and f]
        print("FAULT_INJECT %s target_red=%d other_red=%s"
              % (name, len(tgt_fails), others))
        for m in tgt_fails[:4]:
            print("    %s" % m)
        ok = bool(tgt_fails) and not others
        return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="结论真实性门（S2-B）")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
    ap.add_argument("--json-out", default="")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--fault-inject", default="")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    if a.fault_inject == "all":
        rc = 0
        for nm in ("vocab", "surface", "coverage", "snapshot"):
            rc |= fault_inject(nm, a.root)
        print("FAULT_INJECT_ALL %s" % ("PASS" if rc == 0 else "FAIL"))
        return rc
    if a.fault_inject:
        return fault_inject(a.fault_inject, a.root)
    res = run(a.root)
    if a.json_out:
        d = os.path.dirname(os.path.abspath(a.json_out))
        if d:
            os.makedirs(d, exist_ok=True)
        with open(a.json_out, "w", encoding="utf-8") as f:
            json.dump(res, f, ensure_ascii=False, indent=1, sort_keys=True)
        print("REPORT_WRITTEN %s" % a.json_out)
    if a.quiet:
        bad = [c["check"] for c in res["results"] if not c["pass"]]
        print("CONCLUSION_TRUTH %s checks=%d failed=%d %s"
              % ("PASS" if res["pass"] else "FAIL", len(res["results"]), len(bad),
                 ",".join(bad)))
        for c in res["results"]:
            for m in c["fails"][:6]:
                print("  - [%s] %s" % (c["check"], m))
    else:
        print(json.dumps(res, ensure_ascii=False, indent=1))
    return 0 if res["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
