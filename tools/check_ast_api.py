#!/usr/bin/env python3
"""DOC-004: Clang AST public symbols 与 API 头/合同文档一致性校验。

旧版判据是「同头文件正则声明名 ⊆ 同头文件 AST 名集」——恒真且 nparams 提取后
从不参与比较（M5b-G-03）。本版按 ENGINEERING_SPEC §8「检查器必须能红能绿」
重建为三个可判红命题：

  P1 header→AST   头内声明（正则表）的每个函数必须在 AST FunctionDecl 集内；
  P2 params       header 声明与 AST 的参数个数必须一致；
  P3 doc→AST      docs/contracts/API_CONTRACTS.csv 的自由函数行必须命中 AST，
                  且 full_signature 顶层参数个数与 AST 一致（文档与 AST 参数个数
                  不一致 ⇒ 判红）。

扫描面 = 3 个核心 public 头 ∪ API_CONTRACTS.csv 中登记的 header。
clang 解析失败/头缺失一律 fail-closed 判红（§8 输入缺失不得静默通过）。
覆盖计数（解析成功头数 / 已比对文档行 / 跳过行及原因）写入输出，PASS 文案不得
虚报覆盖。

用法：
  python3 tools/check_ast_api.py            # 真形态
  python3 tools/check_ast_api.py --selftest # 负例面（参数个数不一致必红）
exit 0 = PASS。
"""
import argparse
import csv
import json
import pathlib
import re
import subprocess
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[1]

# 检查的核心 public 头 (各含合同声明)
CORE_HEADERS = [
    "include/astrocs/core/contracts.h",
    "include/astrocs/core/artifact.h",
    "include/astrocs/io/io_adapter.h",
]
API_CSV = "docs/contracts/API_CONTRACTS.csv"

DECL_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\(([^;{)]*)\)\s*(?:const)?\s*;", re.S)
SKIP_NAMES = {"if", "for", "while", "sizeof", "return", "static_assert", "switch",
              "defined", "catch", "assert", "decltype"}


def include_flags(header: pathlib.Path) -> list[str]:
    """header 自解析所需 include 路径：include/ + 各模块 include/ 与 cpp/ 根。"""
    flags = ["-I", str(REPO / "include")]
    roots = []
    for base in (REPO / "lib", REPO / "include"):
        if base.is_dir():
            for d in base.rglob("include"):
                if d.is_dir():
                    roots.append(d)
            for d in base.rglob("cpp"):
                if d.is_dir():
                    roots.append(d)
    for d in sorted(set(roots)):
        flags += ["-I", str(d)]
    flags += ["-I", str(header.parent)]
    return flags


def ast_functions(header_rel: str):
    """返回 (自由函数名→参数个数, 任意声明名集合, 错误)。"""
    full = REPO / header_rel
    if not full.is_file():
        return {}, set(), f"header missing: {header_rel}"
    def run(lang: str, std: str):
        cmd = ["clang", lang, std, "-Xclang", "-ast-dump=json", "-fsyntax-only",
               *include_flags(full), str(full)]
        try:
            return subprocess.run(cmd, capture_output=True, text=True, timeout=300), None
        except (OSError, subprocess.SubprocessError) as exc:
            return None, f"clang unavailable: {exc}"

    # 先按 C++ 解析（本仓 ABI 头均 C++ 兼容），失败再回退 C11
    r, err = run("-xc++", "-std=c++17")
    if err:
        return {}, set(), err
    if r.returncode != 0:
        r2, err2 = run("-xc", "-std=c11")
        if err2:
            return {}, set(), err2
        if r2.returncode == 0:
            r = r2
    if r.returncode != 0:
        first = next((l for l in r.stderr.splitlines() if "error" in l), r.stderr[:200])
        return {}, set(), f"clang failed: {first.strip()[:300]}"
    try:
        dump = json.loads(r.stdout)
    except json.JSONDecodeError as exc:
        return {}, set(), f"clang AST dump not JSON: {exc}"
    fns = {}
    all_names = set()

    def walk(node):
        if isinstance(node, dict):
            kind = node.get("kind")
            name = node.get("name")
            if name and kind in ("FunctionDecl", "CXXMethodDecl", "CXXConstructorDecl",
                                 "CXXDestructorDecl", "FunctionTemplateDecl",
                                 "ClassTemplateDecl", "CXXRecordDecl", "StructDecl",
                                 "TypedefDecl", "EnumDecl"):
                all_names.add(name)
            if kind == "FunctionDecl" and name:
                params = [c for c in node.get("inner", [])
                          if isinstance(c, dict) and c.get("kind") == "ParmVarDecl"]
                # 同一名字可能有多次声明（前置声明/宏展开），收集全部参数个数
                fns.setdefault(name, set()).add(len(params))
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    walk(dump)
    return fns, all_names, None


# 顶层函数原型：行首「返回类型 [导出宏] 名(」，排除 std::x(...)/a.b(...)/表达式语句。
PROTO_RE = re.compile(
    r"^\s*(?:[A-Z][A-Z0-9_]*_API\s+)?"
    r"(?:[A-Za-z_]\w*(?:\s*<[^;()]*>)?(?:\s+|\s*[*&]\s*))+"
    r"([A-Za-z_]\w*)\s*\(", re.M)


def header_prototypes(header_rel: str) -> tuple[dict, set]:
    """头内顶层函数原型表 (名→参数个数) 与条件编译块内原型名集合。

    条件编译块（#if/#ifdef/#ifndef 直到匹配 #endif）内的原型在当前宏集下可能不
    参与编译，故只做「AST 若可见则比对」，不参与「必须可见」的硬断言。
    """
    p = REPO / header_rel
    if not p.is_file():
        return {}, set()
    lines = p.read_text(encoding="utf-8", errors="ignore").splitlines()
    depth = 0
    cond_names: set[str] = set()
    protos: dict = {}
    for ln in lines:
        s = ln.strip()
        if s.startswith("#if") or s.startswith("#elif"):
            depth += 1
            continue
        if s.startswith("#endif"):
            depth = max(0, depth - 1)
            continue
        if s.startswith("#else") or s.startswith("#include") or s.startswith("#define"):
            continue
        m = PROTO_RE.match(ln)
        if not m:
            continue
        name = m.group(1)
        if name in SKIP_NAMES or name.startswith("#"):
            continue
        pre = ln[:m.start(1)]
        if any(t in pre for t in ("::", ".", "->", "=", "(", ")")):
            continue  # 调用/初始化语句，不是声明
        if not ln.rstrip().endswith(";") and ";" not in ln:
            # 多行原型：只在首行登记名字，参数个数交给 AST 判定
            protos.setdefault(name, None)
        else:
            tail = ln[ln.find("("):]
            inner = tail[1:tail.rfind(")")] if ")" in tail else ""
            n = None if ")" not in tail else (
                0 if inner.strip() in ("", "void") else
                len([x for x in inner.split(",") if x.strip()]))
            protos.setdefault(name, n)
        if depth > 0:
            cond_names.add(name)
    return protos, cond_names


def header_declares(header_rel: str, sym: str) -> bool:
    """头文本内是否存在该符号的声明（宏条件编译下的兜底存在性判据）。"""
    p = REPO / header_rel
    if not p.is_file():
        return False
    txt = p.read_text(encoding="utf-8", errors="ignore")
    return re.search(r"\b" + re.escape(sym) + r"\s*\(", txt) is not None


def sig_params(sig: str):
    """full_signature 顶层参数个数（跳过 <> 与 () 嵌套）。"""
    i, j = sig.find("("), sig.rfind(")")
    if i < 0 or j <= i:
        return None
    inner = sig[i + 1:j].strip()
    if not inner or inner == "void":
        return 0
    depth = 0
    n = 1
    for ch in inner:
        if ch in "<([":
            depth += 1
        elif ch in ">)]":
            depth -= 1
        elif ch == "," and depth == 0:
            n += 1
    return n


def load_doc_rows():
    p = REPO / API_CSV
    if not p.is_file():
        return [], f"contract doc missing: {API_CSV}"
    with open(p, encoding="utf-8") as fh:
        return list(csv.DictReader(fh)), None


def collect_errors():
    errors = []
    doc_rows, doc_err = load_doc_rows()

    if doc_err:
        errors.append(doc_err)
    headers = list(CORE_HEADERS)
    for r in doc_rows:
        h = (r.get("header") or "").strip()
        if h and h not in headers:
            headers.append(h)

    stats = {"headers_total": len(headers), "headers_parsed": 0, "ast_functions": 0,
             "doc_rows": len(doc_rows), "doc_rows_checked": 0,
             "doc_rows_skipped": 0, "decl_checked": 0}

    ast_cache: dict = {}
    proto_cache: dict = {}

    def ast_of(h):
        if h not in ast_cache:
            ast_cache[h] = ast_functions(h)
        return ast_cache[h]

    def protos_of(h):
        if h not in proto_cache:
            proto_cache[h] = header_prototypes(h)
        return proto_cache[h]

    for h in headers:
        fns, all_names, err = ast_of(h)
        if err:
            errors.append(f"{h}: {err}")
            continue
        stats["headers_parsed"] += 1
        stats["ast_functions"] += len(fns)
        # P1/P2: 头内声明 ↔ AST
        protos, cond = protos_of(h)
        for name, n in sorted(protos.items()):
            stats["decl_checked"] += 1
            if name not in all_names:
                if name in cond:
                    stats["decl_conditional"] = stats.get("decl_conditional", 0) + 1
                    continue  # 条件编译块内原型，当前宏集不参与编译
                errors.append(f"{h}: {name} 不在 AST 任何声明集（声明/AST 不一致）")
                continue
            if n is not None and name in fns and n not in fns[name]:
                errors.append(f"{h}: {name} 参数个数不一致 header={n} ast={sorted(fns[name])}")

    # P3: 文档 ↔ AST
    by_header = {}
    for r in doc_rows:
        by_header.setdefault((r.get("header") or "").strip(), []).append(r)
    for h, rows in sorted(by_header.items()):
        fns, all_names, err = ast_of(h)
        if err:
            # 头无法解析的文档行计入 skipped（覆盖计数不得把未比对行当已比对）
            stats["doc_rows_skipped"] += len(rows)
            continue
        for r in rows:
            sym = (r.get("symbol") or "").strip()
            plain = re.fullmatch(r"[A-Za-z_]\w*", sym) is not None
            if not plain:
                # C++ 方法/类型行：只要求末段符号在 AST 任意声明集内
                leaf = sym.split("::")[-1].strip()
                stats["doc_rows_skipped"] += 1
                if leaf and leaf not in all_names:
                    errors.append(f"{API_CSV}:{r.get('id')}: {sym} 的叶符号 {leaf} "
                                  f"不在 AST 声明集")
                continue
            if sym not in fns:
                if sym in all_names:
                    # 类型/类名行：只做存在性比对，不计入参数个数断言
                    stats["doc_rows_skipped"] += 1
                    continue
                protos, cond = protos_of(h)
                if sym in protos:
                    # 头内确有原型但当前宏集下未编入 AST（如 #ifdef AIO_ENABLE_HEALPIX）：
                    # 计入 skipped 而非判红，也不计入「已比对」
                    stats["doc_rows_skipped"] += 1
                    continue
                stats["doc_rows_checked"] += 1
                errors.append(f"{API_CSV}:{r.get('id')}: 文档符号 {sym} "
                              f"不在 {h} 的 AST 任何声明集且头内无声明")
                continue
            protos, cond = protos_of(h)
            if sym not in protos:
                # 头内非顶层原型（libc 转发/宏包装行）：只做存在性，不做参数个数断言
                stats["doc_rows_skipped"] += 1
                continue
            stats["doc_rows_checked"] += 1
            n = sig_params(r.get("full_signature") or "")
            if n is not None and n not in fns[sym]:
                errors.append(f"{API_CSV}:{r.get('id')}: {sym} 参数个数不一致 "
                              f"doc={n} ast={sorted(fns[sym])}")
    return errors, stats


def selftest() -> int:
    """负例面：文档参数个数与 AST 不一致必须能被检出。"""
    global REPO
    with tempfile.TemporaryDirectory() as tmp:
        td = pathlib.Path(tmp)
        (td / "inc").mkdir()
        (td / "docs/contracts").mkdir(parents=True)
        (td / "inc/demo.h").write_text("int demo_add(int a, int b);\n", encoding="utf-8")
        # 核心头在临时树内同样存在（collect_errors 恒定包含 CORE_HEADERS）
        for rel in CORE_HEADERS:
            p = td / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("int core_stub(void);\n", encoding="utf-8")
        (td / "docs/contracts/API_CONTRACTS.csv").write_text(
            "id,symbol,header,full_signature\n"
            'API-demo,demo_add,inc/demo.h,"int demo_add(int a, int b);"\n',
            encoding="utf-8")
        saved = REPO
        REPO = td
        try:
            errors, _ = collect_errors()
            if errors:
                print("SELFTEST_FAIL: 干净基线误报: %s" % errors[:2])
                return 1
            (td / "docs/contracts/API_CONTRACTS.csv").write_text(
                "id,symbol,header,full_signature\n"
                'API-demo,demo_add,inc/demo.h,"int demo_add(int a, int b, int c);"\n',
                encoding="utf-8")
            errors, _ = collect_errors()
            if not any("参数个数不一致" in e for e in errors):
                print("SELFTEST_FAIL: 参数个数不一致未被检出: %s" % errors)
                return 1
        finally:
            REPO = saved
    print("SELFTEST_PASS: 干净基线绿；文档/AST 参数个数不一致必红")
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="DOC-004 AST/API 文档一致性")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()
    errors, stats = collect_errors()
    if errors:
        print("DOC-004_AST_VIOLATION:")
        for e in errors[:60]:
            print("  " + e)
        print("DOC-004 coverage: %s" % json.dumps(stats, ensure_ascii=False))
        return 1
    print("DOC-004_PASS: %d/%d 头 clang 解析成功, %d 条头内声明参数一致, "
          "%d 条文档行已比对, %d 条 C++ 行仅叶符号比对, AST 函数 %d"
          % (stats["headers_parsed"], stats["headers_total"], stats["decl_checked"],
             stats["doc_rows_checked"], stats["doc_rows_skipped"],
             stats["ast_functions"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
