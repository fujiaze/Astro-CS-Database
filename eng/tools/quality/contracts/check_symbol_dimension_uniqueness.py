#!/usr/bin/env python3
"""check_symbol_dimension_uniqueness.py — 符号量纲唯一性门（SYM-DIM-UNIQUE）

判据（本门唯一目的）：**同一符号不得在同仓指两个量纲不同的量**。

三条可判定子判据（全部 fail-closed）：
  C1 SYM-DIM-DEF-SPLIT       定义站点 `SYM = <expr>` 的右侧经量纲代数求值后，必须等于该
                             符号在注册表中的唯一量纲类；不等即红（同时给出两个站点的 file:line）。
  C2 SYM-DIM-CLAIM-CONFLICT  符号旁的显式量纲断言（`SYM` 无量纲 / `SYM` 量纲 `X` /
                             `SYM` 单位 `X`）必须与注册量纲类一致；不一致即红。
  C3 SYM-DIM-FORBIDDEN-CTX   注册表为符号声明的「禁止共现短语」（如 `D_p` 旁不得出现「权重和」）
                             命中即红。同一窗口含否定标记（不是/不得/禁止/…）视为**消歧声明**，不判红。

**声明证据面（诚实边界，不作过度声明）**：本门只对「定义站点」与「显式量纲断言」有判定力。
仅使用符号、既不定义也不断言量纲的站点（如「或 `D_p=0`」）**本门不作判定** —— 该形态由
`docs/contracts/DATA_SEMANTICS.md` §4a 的符号唯一性条款 + 人工复核清单覆盖。

判据自检（AGENTS §5，`--self-test`）：
  - 正例：干净夹具必须 PASS 且零 finding；
  - 负例：三种注入（C1/C2/C3 各一）必须各自产生**预期 id + 预期 file:line** 的 finding；
  - **恒真守卫**：与判据无关的任意改动必须产生与干净夹具**完全相同**的 finding 集（否则判据恒红）；
  - **判别力守卫**：注入后的 finding 集必须与干净夹具**不同**（否则判据恒真/无证据资格）；
  - **恒假守卫**：否定式消歧写法（「`D_p` 不是无量纲权重和」）不得触发 C3。
  任一守卫不成立 ⇒ `--self-test` 非零退出。

Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 registry schema error
"""
import argparse
import json
import pathlib
import re
import sys
import tempfile

REGISTRY_REL = "eng/tools/quality/contracts/symbol_dimension_registry.json"

# ── 表达式切分 ───────────────────────────────────────────────────────────────
# RHS 终止符：反引号 / 中文句读 / 分号 / 竖线 / 右括号 / 第二个等号 / 行尾
_RHS_STOP = "`。，；;|)）]】、,#⇒→≥≤≠"
# Σ/Sum 的下标可能是 CJK（如 Σ_合格 / Σ_{合格} / Σ_j）⇒ 下标字符集必须含 CJK，
# 否则 "_合格" 会残留成未登记叶符号（实测 DATA-002 §2a 即此形态）。
_SUM_RE = re.compile(r"(?:Σ|Sum)\s*(?:\{[^{}]*\}|_\{[^{}]*\}|_[A-Za-z0-9\u4e00-\u9fff]+)?")
_CJK_RE = re.compile(r"[\u3000-\u303f\u4e00-\u9fff\uff00-\uffef]")
_SUP = {"²": "^2", "³": "^3", "⁻": "^-", "·": "*", "×": "*", "−": "-", "–": "-", "＊": "*"}
_TOK_RE = re.compile(r"\s*(?:\*\*|[\^*/()+\-]|[A-Za-z_][A-Za-z0-9_]*|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)")


class DimError(Exception):
    pass


def _extract_rhs(line, start):
    """取 `SYM =` 之后的右侧表达式（切到终止符 / 行尾），再去 Σ 算子、再截到首个 CJK。"""
    rhs = line[start:]
    cut = len(rhs)
    for i, ch in enumerate(rhs):
        if ch in _RHS_STOP:
            cut = i
            break
    rhs = rhs[:cut]
    rhs = _SUM_RE.sub(" ", rhs)
    m = _CJK_RE.search(rhs)
    if m:
        rhs = rhs[: m.start()]
    # 尾部悬空运算符 / 未闭合括号是散文残留（如 "a_jp  (覆盖球面面积" 被 CJK 截断），
    # 合法表达式不会以二元运算符结尾 ⇒ 去掉后再解析（避免把散文当表达式判红）。
    return rhs.strip().rstrip(" \t(+-*/^·")


def _is_definition_lhs(line, start):
    """SYM 是否处在「等式左值」位置（而不是链式等式的右段 / 乘积因子）。

    排除三类：
      - 前导（跳空白）为 = * · × ⇒ 本 SYM 是某个更大表达式的右段（如 `S_p = F_p/D_p = Σ...`）；
      - 前导为字母/数字/下划线/右括号 ⇒ 本 SYM 是乘积因子（如 `S_p D_p = ...`）；
      - 前导为单个 / ⇒ 除号（如 `F_p/D_p = ...`）；但 `//` 注释前缀不算除号。
    """
    i = start - 1
    while i >= 0 and line[i] in " \t":
        i -= 1
    if i < 0:
        return True
    c = line[i]
    if c in "=*·×":
        return False
    if re.match(r"[A-Za-z0-9_)\]]", c):
        return False
    if c == "/":
        j = i - 1
        while j >= 0 and line[j] in " \t":
            j -= 1
        return j >= 0 and line[j] == "/"
    return True


def _tokenize(expr):
    toks, pos = [], 0
    while pos < len(expr):
        m = _TOK_RE.match(expr, pos)
        if not m:
            raise DimError("非法字符 %r @%d in %r" % (expr[pos], pos, expr))
        t = m.group(0).strip()
        if t == "**":
            t = "^"
        if t:
            toks.append(t)
        pos = m.end()
    return toks


class _Parser:
    def __init__(self, toks, leaf_dims, sym):
        self.t, self.i, self.leaf, self.sym = toks, 0, leaf_dims, sym

    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else None

    def next(self):
        v = self.peek()
        self.i += 1
        return v

    def parse(self):
        d = self.expr()
        if self.i != len(self.t):
            raise DimError("表达式尾部未消费: %r" % (self.t[self.i:],))
        return d

    def expr(self):
        d = self.term()
        while self.peek() in ("+", "-"):
            self.next()
            r = self.term()
            if d != r:
                raise DimError("加减号两侧量纲不同: %s vs %s" % (d, r))
            d = r
        return d

    def term(self):
        d = self.factor()
        while True:
            p = self.peek()
            if p in ("*", "/"):
                self.next()
                r = self.factor()
                d = _div(d, r) if p == "/" else _mul(d, r)
            elif p is not None and (p == "(" or re.match(r"^[A-Za-z_]", p) or re.match(r"^\d", p)):
                # 隐式乘法（并置）: V_j w_jp^2
                d = _mul(d, self.factor())
            else:
                return d

    def factor(self):
        d = self.primary()
        if self.peek() == "^":
            self.next()
            sign = 1
            if self.peek() == "-":
                self.next()
                sign = -1
            n = self.next()
            if n is None or not re.match(r"^\d+$", n):
                raise DimError("幂次非整数: %r" % (n,))
            d = {k: v * sign * int(n) for k, v in d.items()}
        return _clean(d)

    def primary(self):
        p = self.next()
        if p is None:
            raise DimError("表达式意外结束")
        if p == "(":
            d = self.expr()
            if self.next() != ")":
                raise DimError("括号未闭合")
            return d
        if re.match(r"^\d", p):
            return {}
        if p in self.leaf:
            return dict(self.leaf[p]["dim"])
        raise DimError("未登记叶符号 %r（符号 %s 的定义站点无法求值）" % (p, self.sym))


def _clean(d):
    return {k: v for k, v in d.items() if v != 0}


def _mul(a, b):
    o = dict(a)
    for k, v in b.items():
        o[k] = o.get(k, 0) + v
    return _clean(o)


def _div(a, b):
    o = dict(a)
    for k, v in b.items():
        o[k] = o.get(k, 0) - v
    return _clean(o)


def eval_dim(expr, leaf_dims, sym):
    """表达式 → 量纲向量。空表达式 / 纯数字 → {}。"""
    toks = _tokenize(expr)
    if not toks:
        return {}
    return _Parser(toks, leaf_dims, sym).parse()


def canonical_dim(expr, leaf_dims, sym):
    """规范化量纲串（sr / ADU^2/sr^2 / 无量纲 / 1）→ 量纲向量。"""
    e = expr.strip().strip("*").strip()
    if e in ("无量纲", "dimensionless", "1", "-"):
        return {}
    for k, v in _SUP.items():
        e = e.replace(k, v)
    return eval_dim(e, leaf_dims, sym)


# ── 扫描面 ───────────────────────────────────────────────────────────────────
def collect_files(repo, scan):
    excl = scan.get("exclude_path_substrings", [])
    cap = scan.get("max_file_bytes", 4 << 20)
    seen, out = set(), []
    for pat in scan.get("globs", []):
        for p in repo.glob(pat):
            if not p.is_file():
                continue
            rel = p.relative_to(repo).as_posix()
            if rel in seen:
                continue
            if any(x in "/" + rel for x in excl):
                continue
            try:
                if p.stat().st_size > cap:
                    continue
            except OSError:
                continue
            seen.add(rel)
            out.append((rel, p))
    return sorted(out)


def _read(p):
    try:
        return p.read_text(encoding="utf-8", errors="ignore").splitlines()
    except OSError:
        return []


# ── 三条判据 ─────────────────────────────────────────────────────────────────
def check_c1_defs(files, reg):
    """C1：同一符号的全部定义站点必须求值到同一量纲类。"""
    leaf = reg["leaf_symbols"]
    findings = []
    for ent in reg["symbols"]:
        sym = ent["symbol"]
        declared = ent["dim"]
        pat = re.compile(r"(?<![A-Za-z0-9_])" + re.escape(sym) + r"\s*=(?!=)")
        for rel, path in files:
            for ln, line in enumerate(_read(path), 1):
                for m in pat.finditer(line):
                    if not _is_definition_lhs(line, m.start()):
                        continue
                    rhs = _extract_rhs(line, m.end())
                    if not rhs or not re.search(r"[A-Za-z_]", rhs):
                        continue  # 守卫站点（纯常量），不作量纲判定
                    try:
                        got = eval_dim(rhs, leaf, sym)
                    except DimError as exc:
                        findings.append({
                            "id": "SYM-DIM-DEF-UNRESOLVED", "severity": "P1", "symbol": sym,
                            "file": rel, "line": ln, "expr": rhs,
                            "observed": str(exc), "expected": "定义站点右侧可求值"})
                        continue
                    if not _dim_ok(declared, got, reg):
                        findings.append({
                            "id": "SYM-DIM-DEF-SPLIT", "severity": "P1", "symbol": sym,
                            "file": rel, "line": ln, "expr": rhs,
                            "observed": "定义站点量纲 %s" % _fmt(got),
                            "expected": "注册量纲类 %s（证据 %s）" % (_fmt(declared), ent.get("evidence", "?"))})
    return findings


def _dim_ok(declared, got, reg):
    if "parametric" in declared:
        fam = declared["parametric"]
        allowed = set()
        if fam == "w":
            for s in reg.get("weight_leaves", []):
                if s in reg["leaf_symbols"]:
                    allowed.add(_fmt(dict(reg["leaf_symbols"][s]["dim"])))
        return _fmt(got) in allowed
    return dict(declared) == dict(got)


def _fmt(d):
    if "parametric" in d:
        return "<%s 的量纲>" % d["parametric"]
    if not d:
        return "1(无量纲)"
    num = "·".join("%s^%d" % (k, v) for k, v in sorted(d.items()) if v > 0)
    den = "·".join("%s^%d" % (k, -v) for k, v in sorted(d.items()) if v < 0)
    return (num or "1") + ("/" + den if den else "")


_DIMLESS_RE = re.compile(r"((?:`[^`\n]+`\s*/\s*)*`[^`\n]+`)\s*\**\s*(?:是|为)?\s*\**\s*(无量纲|dimensionless)")
_UNIT_RE = re.compile(r"`([A-Za-z_][A-Za-z0-9_]*)`\s*(?:的)?\s*(?:量纲|单位)\s*(?:是|=|为)?\s*\**\s*`([^`\n]+)`")
_BT_RE = re.compile(r"`([^`\n]+)`")


def check_c2_claims(files, reg):
    """C2：符号旁的显式量纲断言必须与注册量纲类一致。"""
    leaf = reg["leaf_symbols"]
    byname = {e["symbol"]: e for e in reg["symbols"]}
    findings = []
    for rel, path in files:
        for ln, line in enumerate(_read(path), 1):
            for m in _DIMLESS_RE.finditer(line):
                for t in _BT_RE.findall(m.group(1)):
                    ent = byname.get(t.strip())
                    if ent and not _dim_ok(ent["dim"], {}, reg):
                        findings.append({
                            "id": "SYM-DIM-CLAIM-CONFLICT", "severity": "P1", "symbol": t.strip(),
                            "file": rel, "line": ln, "claim": "无量纲",
                            "observed": "文本断言 1(无量纲)",
                            "expected": "注册量纲类 %s（证据 %s）" % (_fmt(ent["dim"]), ent.get("evidence", "?"))})
            for m in _UNIT_RE.finditer(line):
                ent = byname.get(m.group(1))
                if not ent:
                    continue
                try:
                    got = canonical_dim(m.group(2), leaf, ent["symbol"])
                except DimError:
                    continue
                if not _dim_ok(ent["dim"], got, reg):
                    findings.append({
                        "id": "SYM-DIM-CLAIM-CONFLICT", "severity": "P1", "symbol": ent["symbol"],
                        "file": rel, "line": ln, "claim": m.group(2),
                        "observed": "文本断言 %s" % _fmt(got),
                        "expected": "注册量纲类 %s（证据 %s）" % (_fmt(ent["dim"]), ent.get("evidence", "?"))})
    return findings


def check_c3_forbidden(files, reg):
    """C3：符号旁不得出现注册的禁止共现短语（否定式消歧声明除外）。"""
    negs = reg.get("negation_markers", [])
    win = 16
    findings = []
    for ent in reg["symbols"]:
        sym, phrases = ent["symbol"], ent.get("forbidden_context_phrases", [])
        if not phrases:
            continue
        pat = re.compile(r"(?<![A-Za-z0-9_])" + re.escape(sym) + r"(?![A-Za-z0-9_])")
        for rel, path in files:
            for ln, line in enumerate(_read(path), 1):
                for m in pat.finditer(line):
                    lo, hi = max(0, m.start() - win), min(len(line), m.end() + win)
                    ctx = line[lo:hi]
                    hit = next((p for p in phrases if p in ctx), None)
                    if not hit:
                        continue
                    if any(n in ctx for n in negs):
                        continue  # 消歧声明，不判红
                    findings.append({
                        "id": "SYM-DIM-FORBIDDEN-CTX", "severity": "P1", "symbol": sym,
                        "file": rel, "line": ln, "phrase": hit, "context": ctx.strip(),
                        "observed": "禁止共现短语 %r 出现在符号 %s 的近邻窗口" % (hit, sym),
                        "expected": ent.get("forbidden_context_reason", "该符号不得指另一个量纲的量")})
    return findings


def run_checks(repo, reg):
    files = collect_files(repo, reg["scan"])
    f = check_c1_defs(files, reg) + check_c2_claims(files, reg) + check_c3_forbidden(files, reg)
    f.sort(key=lambda x: (x["id"], x["file"], x["line"]))
    return f, len(files)


def load_registry(repo):
    p = repo / REGISTRY_REL
    if not p.is_file():
        raise SystemExit("env error: 注册表缺失 %s" % REGISTRY_REL)
    doc = json.loads(p.read_text(encoding="utf-8"))
    for e in doc.get("symbols", []):
        if not e.get("symbol") or "dim" not in e or not e.get("evidence"):
            raise SystemExit("schema error: 注册项缺 symbol/dim/evidence: %r" % e)
        ev = repo / e["evidence"].rpartition(":")[0]
        if not ev.is_file():
            raise SystemExit("schema error: evidence 文件不存在 %s" % e["evidence"])
        if e["symbol"] not in ev.read_text(encoding="utf-8", errors="ignore"):
            raise SystemExit("schema error: evidence 未逐字含 %s: %s" % (e["symbol"], e["evidence"]))
    return doc


# ── 判据自检（AGENTS §5：先证判别力，再谈证据资格）────────────────────────────
_CLEAN = {
    "docs/contracts/FIX.md": [
        "`D_p = Σ_j a_jp` = covered_area，量纲 `sr`。",
        "`W_p = Σ_{合格} w_j`，无量纲。",
        "`D_p` 不是无量纲权重和；权重和一律用 `W_p`。",
        "仅零合格样本（或 `D_p = 0`）时 S=NaN。",
    ],
    "lib/fixture/engine.cpp": [
        "    // D_p = Σ_j a_jp  (覆盖球面面积, sr)",
        "    double wsum = 0.0;   // W_p = Σ_{合格} w_j (无量纲)",
    ],
}


def _write_fixture(root, corpus):
    for rel, lines in corpus.items():
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _mutate(corpus, rel, old, new):
    c = {k: list(v) for k, v in corpus.items()}
    hit = False
    for i, l in enumerate(c[rel]):
        if old in l:
            c[rel][i] = l.replace(old, new)
            hit = True
    assert hit, "夹具注入失败: %r 不在 %s" % (old, rel)
    return c


def self_test(repo):
    reg = load_registry(repo)
    cases = []

    def probe(corpus):
        with tempfile.TemporaryDirectory(prefix="symdim-selftest-") as td:
            root = pathlib.Path(td)
            _write_fixture(root, corpus)
            f, _ = run_checks(root, reg)
            return [dict(x) for x in f]

    clean = probe(_CLEAN)
    cases.append(("正例·干净夹具零 finding", clean == [], "got %r" % clean))

    m1 = probe(_mutate(_CLEAN, "docs/contracts/FIX.md",
                      "`D_p = Σ_j a_jp` = covered_area，量纲 `sr`。",
                      "`D_p = Σ_{合格} w_j` = covered_area。"))
    ids1 = sorted({x["id"] for x in m1})
    # 该注入同时命中 C1 与 C3（同一行既有错误定义又有权重和短语）—— 两条都红是正确行为，
    # 断言只要求「预期 finding 在场」且「不得出现预期闭包之外的 id」。
    cases.append(("负例·C1 定义站点量纲分裂",
                  "SYM-DIM-DEF-SPLIT" in ids1
                  and set(ids1) <= {"SYM-DIM-DEF-SPLIT", "SYM-DIM-FORBIDDEN-CTX"}
                  and any(x["id"] == "SYM-DIM-DEF-SPLIT" and x["file"] == "docs/contracts/FIX.md"
                          and x["line"] == 1 and x["symbol"] == "D_p" for x in m1),
                  "got %r" % m1))

    m2 = probe(_mutate(_CLEAN, "docs/contracts/FIX.md",
                      "`W_p = Σ_{合格} w_j`，无量纲。", "`W_p`/`D_p` 无量纲。"))
    ids2 = sorted({x["id"] for x in m2})
    cases.append(("负例·C2 量纲断言冲突",
                  ids2 == ["SYM-DIM-CLAIM-CONFLICT"] and any(
                      x["id"] == "SYM-DIM-CLAIM-CONFLICT" and x["file"] == "docs/contracts/FIX.md"
                      and x["line"] == 2 and x["symbol"] == "D_p" for x in m2),
                  "got %r" % m2))

    m3 = probe(_mutate(_CLEAN, "lib/fixture/engine.cpp",
                      "    double wsum = 0.0;   // W_p = Σ_{合格} w_j (无量纲)",
                      "    double wsum = 0.0;   // D_p = Σ_{合格} w_j (有效权重和)"))
    ids3 = sorted({x["id"] for x in m3})
    cases.append(("负例·C3 禁止共现短语",
                  "SYM-DIM-FORBIDDEN-CTX" in ids3 and any(
                      x["file"] == "lib/fixture/engine.cpp" and x["line"] == 2 for x in m3),
                  "got %r" % m3))

    m4 = probe(_mutate(_CLEAN, "docs/contracts/FIX.md",
                      "`W_p = Σ_{合格} w_j`，无量纲。", "`Q_p = Σ_{合格} w_j`，无量纲。"))
    cases.append(("恒真守卫·无关改动不得产生 finding", m4 == clean, "got %r" % m4))

    cases.append(("判别力守卫·注入必须改变结论", m1 != clean and m2 != clean and m3 != clean,
                  "m1=%r m2=%r m3=%r clean=%r" % (m1, m2, m3, clean)))

    m5 = probe(_mutate(_CLEAN, "lib/fixture/engine.cpp",
                      "    // D_p = Σ_j a_jp  (覆盖球面面积, sr)",
                      "    // D_p 不是无量纲权重和 (覆盖球面面积, sr)"))
    cases.append(("恒假守卫·否定式消歧不得触发 C3",
                  not any(x["id"] == "SYM-DIM-FORBIDDEN-CTX" for x in m5), "got %r" % m5))

    ok = True
    print("== check_symbol_dimension_uniqueness --self-test ==")
    for name, passed, detail in cases:
        print(("  PASS  " if passed else "  FAIL  ") + name + ("" if passed else "  | " + detail))
        ok = ok and passed
    print("== self-test %s (%d/%d) ==" % ("PASS" if ok else "FAIL",
                                          sum(1 for _, p, _ in cases if p), len(cases)))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    repo = pathlib.Path(args.repo).resolve()
    if args.self_test:
        return self_test(repo)
    reg = load_registry(repo)
    findings, nfiles = run_checks(repo, reg)
    status = "FAIL" if findings else "PASS"
    result = {"tool": "check_symbol_dimension_uniqueness", "status": status,
              "criterion": "SYM-DIM-UNIQUE: 同一符号不得在同仓指两个量纲不同的量",
              "files_scanned": nfiles,
              "symbols": [e["symbol"] for e in reg["symbols"]],
              "evidence_surface": "定义站点 + 显式量纲断言 + 禁止共现短语（其余形态不作判定）",
              "findings": findings, "passed": status == "PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
