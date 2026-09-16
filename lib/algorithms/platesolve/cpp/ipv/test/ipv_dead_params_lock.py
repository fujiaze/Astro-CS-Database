#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ipv_dead_params_lock.py - P27 "配置不生效" 防误用机器锁

背景 (已实证):
  生产解算路径 phase1 run -> p1_op_wcs (lib/infrastructure/scheduler/src/module_adapters.cpp:2100)
  -> ipv_solve_from_memory_with_callback_d -> IPVSolver::solve_from_memory_with_callback_f64
  -> solve_post_select 中, triangle_match(U, W, 60, 60, 0.002, s0) 传的是**字面量常数**,
  且选星前 p_adapt.img_n_target = 60 硬覆盖; 另有 polygon_match /
  polygon_match_adaptive / geometric_vote / extract_consensus / prosac_verify 等
  匹配器在 src/ 内**零调用点**(仅 test/ 与 include 声明)。
  => IPVSolverParams / RobustRefineParams 的部分字段在生产路径完全不被读取,
     改配置既不改行为也不报错 ("配置不生效" 型误用)。

负责人裁决 (选 A): 不改解算行为, 只做"明确标注 + 机器锁防误用"。

本锁把该结论变成**可执行断言**:
  1. 解析生产链源码, 提取每个函数的函数体 (花括号配对 + 行号区间);
  2. 计算每个 <配置对象>.<字段> 在各函数体内的**引用点** (去注释);
  3. 与冻结清单 ipv_dead_params_manifest.json 逐项比对:
       - 清单声明 dead 的字段 => 生产链内引用点数必须为 0;
       - 清单声明 live 的字段 => 引用点数必须与冻结值完全一致;
       - 生产链函数闭包必须与清单登记完全一致;
       - 清单字段集合必须与配置结构体头文件字段集合一致;
  4. 任一不一致 => 退出码 1, 并提示"请同步更新清单与文档"。

阴性对照见 ipv_dead_params_selfcheck.py (ctest 门 ipv_dead_params_lock_selfcheck)。

用法:
  ipv_dead_params_lock.py [--src <树根>] [--manifest <json>] [--quiet]
"""

import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# HERE = lib/algorithms/platesolve/cpp/ipv/test
DEFAULT_SRC = os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "..", ".."))
DEFAULT_MANIFEST = os.path.join(HERE, "ipv_dead_params_manifest.json")

CONTROL_KEYWORDS = {
    "if", "for", "while", "switch", "catch", "else", "do", "return",
    "sizeof", "static_assert", "defined", "alignof", "decltype",
}
PRIMITIVE_TYPES = {
    "void", "int", "bool", "double", "float", "char", "long", "short",
    "unsigned", "signed", "size_t", "uint16_t", "uint32_t", "uint64_t",
    "int8_t", "int16_t", "int32_t", "int64_t", "auto", "const", "static",
    "template", "typename", "class", "struct", "enum", "namespace",
}


def strip_comments(text):
    """去掉注释但保留行结构 (行号必须与源文件一致)。"""
    text = re.sub(r"/\*.*?\*/",
                  lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)
    out = []
    for ln in text.split("\n"):
        i = ln.find("//")
        if i >= 0:
            ln = ln[:i]
        out.append(ln)
    return "\n".join(out)


def extract_functions(text):
    """返回 {函数名: (起始行, 结束行, 函数体文本)} (1-based, 含首尾行)。"""
    lines = strip_comments(text).split("\n")
    funcs = {}
    i, n = 0, len(lines)
    while i < n:
        raw = lines[i]
        # 允许最多 8 个空格缩进: 匿名 namespace / 模板实例化内的自由函数
        # (如 ipv_select_from_memory_with_callback_impl) 必须计入生产链闭包;
        # 控制流关键字由 CONTROL_KEYWORDS 过滤, 不会误判为函数定义。
        stripped = raw.strip()
        indent = raw[:len(raw) - len(raw.lstrip())]
        if stripped and len(indent) <= 8 and "(" in raw and ";" not in raw \
                and "(*" not in raw and stripped.split()[0] != "namespace":
            name = None
            for m in re.finditer(
                    r"([A-Za-z_~][A-Za-z0-9_]*(?:::[A-Za-z_~][A-Za-z0-9_]*)*)\s*\(", raw):
                nm = m.group(1)
                base = nm.split("::")[-1]
                if base in CONTROL_KEYWORDS or base in PRIMITIVE_TYPES:
                    continue
                name = nm
                break
            if name:
                j, depth_paren = i, 0
                while j < n:
                    l2 = lines[j]
                    depth_paren += l2.count("(") - l2.count(")")
                    if "{" in l2 and depth_paren <= 0:
                        break
                    j += 1
                if j < n:
                    depth, started, end = 0, False, None
                    k = j
                    while k < n:
                        for ch in lines[k]:
                            if ch == "{":
                                depth += 1
                                started = True
                            elif ch == "}":
                                depth -= 1
                        if started and depth <= 0:
                            end = k
                            break
                        k += 1
                    if end is not None:
                        funcs[name] = (i + 1, end + 1, "\n".join(lines[i:end + 1]))
                        i = end + 1
                        continue
        i += 1
    return funcs


def count_reads(lines, objects, field):
    """统计 <objects[*]>.<field> 的引用点; 返回命中行号列表。

    objects 为配置对象的变量别名列表 (如 ["params"] 或 ["params","p_adapt"]):
    生产路径里 "IPVSolverParams p_adapt = params; p_adapt.img_n_target = 60;"
    这类别名也属于"该字段在代码里被读取", 必须计入。
    """
    hits = []
    pat = re.compile(r"\b(?:" + "|".join(re.escape(o) for o in objects)
                     + r")\s*\.\s*" + re.escape(field) + r"\b")
    for idx, ln in enumerate(lines):
        if pat.search(ln):
            hits.append(idx)
    return hits


def struct_fields(src, header_rel, struct_name):
    """从结构体定义中抽取字段名。"""
    path = os.path.join(src, header_rel)
    text = strip_comments(open(path, encoding="utf-8").read())
    m = re.search(r"struct\s+" + re.escape(struct_name) + r"\s*\{", text)
    if not m:
        raise RuntimeError("找不到结构体 %s (%s)" % (struct_name, header_rel))
    depth, rest = 1, text[m.end():]
    body = []
    for ch in rest:
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                break
        body.append(ch)
    fields = []
    for ln in "".join(body).split("\n"):
        ln = ln.strip()
        mm = re.match(r"^[A-Za-z_][A-Za-z0-9_:<>\s\*&]*?\s+([A-Za-z_][A-Za-z0-9_]*)"
                      r"\s*(\[[^\]]*\])?\s*(=[^;]*)?;\s*$", ln)
        if mm:
            fields.append(mm.group(1))
    return fields


def build_call_graph(sources):
    per_file_funcs = {}
    for rel, text in sources.items():
        per_file_funcs[rel] = extract_functions(text)
    short = {}
    for rel, funcs in per_file_funcs.items():
        for full in funcs:
            short.setdefault(full.split("::")[-1], []).append((rel, full))
    graph = {}
    for rel, funcs in per_file_funcs.items():
        for full, (_s, _e, body) in funcs.items():
            callees = set()
            # 允许显式模板实参: call< T >(...) —— 否则 ipv_select_*_impl<double>
            # 会被漏掉, 生产链闭包不完整。
            for mm in re.finditer(
                    r"\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:<[^;{}()]*>)?\s*\(", body):
                nm = mm.group(1)
                if nm == full or nm in CONTROL_KEYWORDS:
                    continue
                for (r2, f2) in short.get(nm, []):
                    callees.add((r2, f2))
            graph[(rel, full)] = callees
    return per_file_funcs, graph


def reachable_from(sources, entry_points):
    """返回 (可达函数集合, 每文件函数表, 未解析外部符号集合)。

    入口点本身必须能解析 (否则解析器失效, 直接报错);
    被调函数若是未纳入 source_files 的外部符号 (如模块外函数),
    记入 unresolved 由清单的 external_symbols 白名单核对。
    """
    per_file_funcs, graph = build_call_graph(sources)
    for node in entry_points:
        if node not in graph:
            raise RuntimeError("生产入口未解析: %s (函数解析器失效?)" % (node,))
    seen, stack, unresolved = set(), list(entry_points), set()
    while stack:
        node = stack.pop()
        if node in seen:
            continue
        if node not in graph:
            raise RuntimeError("生产链被调函数未解析: %s" % (node,))
        seen.add(node)
        for nxt in graph[node]:
            if nxt not in seen:
                stack.append(nxt)
    # 统计所有从生产链函数体内被调用、但不在函数表内的标识符
    for (rel, fn) in seen:
        body = per_file_funcs[rel][fn][2]
        for mm in re.finditer(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", body):
            nm = mm.group(1)
            if nm in CONTROL_KEYWORDS:
                continue
            if not any(nm == f.split("::")[-1] for (_r, f) in graph):
                unresolved.add(nm)
    return seen, per_file_funcs, unresolved


def main(argv=None):
    ap = argparse.ArgumentParser(description="P27 死参数防误用机器锁")
    ap.add_argument("--src", default=os.environ.get("P27_SRC", DEFAULT_SRC))
    ap.add_argument("--manifest", default=DEFAULT_MANIFEST)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)

    man = json.load(open(args.manifest, encoding="utf-8"))
    errors = []

    sources = {}
    for rel in man["source_files"]:
        path = os.path.join(args.src, rel)
        if not os.path.isfile(path):
            raise RuntimeError("缺少源码文件: %s" % path)
        with open(path, encoding="utf-8") as fh:
            sources[rel] = fh.read()

    reach, per_file_funcs, unresolved = reachable_from(
        sources, [tuple(e) for e in man["entry_points"]])
    declared_prod = set((f, n) for f, n in man["production_chain"])
    extra = sorted(reach - declared_prod)
    missing = sorted(declared_prod - reach)
    for node in extra:
        errors.append("生产链闭包多出 %s::%s (入口可达但清单未登记; 请同步更新清单与文档)" % node)
    for node in missing:
        errors.append("生产链闭包缺少 %s::%s (清单登记但入口不可达; 请同步更新清单与文档)" % node)

    # 外部符号: 仅作信息输出 (标准库/局部 lambda 噪声大, 不参与断言);
    # 生产链内新增的本地函数由 production_chain 闭包一致性检查兜住。
    ext_observed = sorted(unresolved)

    for cname, spec in man["config_structs"].items():
        actual = set(struct_fields(args.src, spec["header"], spec["struct"]))
        declared = set(spec["fields"])
        for f in sorted(actual - declared):
            errors.append("%s 新增字段 %s 未登记清单 (请同步更新清单与文档)" % (cname, f))
        for f in sorted(declared - actual):
            errors.append("%s 声明字段 %s 在头文件中不存在 (请同步更新清单与文档)" % (cname, f))

    # ------- 逐字段引用点核对 (正向: 清单登记的引用点计数) -------
    rows = []
    declared_map = {}   # (rel, fn) -> {field: want}
    for fname, fspec in man["fields"].items():
        objects = fspec.get("objects", ["params"])
        for rel in sorted(fspec["allow_reads"]):
            for (fn, want) in fspec["allow_reads"][rel]:
                if fn not in per_file_funcs.get(rel, {}):
                    errors.append("%s: 清单登记的函数 %s::%s 不存在 (请同步更新清单与文档)"
                                  % (fname, rel, fn))
                    continue
                _s, _e, body = per_file_funcs[rel][fn]
                hits = count_reads(body.split("\n"), objects, fname)
                ok = (len(hits) == want)
                rows.append((fname, rel, fn, want, len(hits), ok))
                declared_map.setdefault((rel, fn), {})[fname] = want
                if not ok:
                    errors.append(
                        "%s: %s::%s 引用点数 %d != 冻结值 %d (%s)"
                        % (fname, rel, fn, len(hits), want,
                           "新接线?" if len(hits) > want else "真消费被删?"))

    # ------- 反向: 生产链函数体内出现未登记的引用点 = 新接线 -------
    prod_by_file = {}
    for (rel, fn) in reach:
        prod_by_file.setdefault(rel, []).append(fn)
    for fname, fspec in man["fields"].items():
        objects = fspec.get("objects", ["params"])
        for rel, fns in sorted(prod_by_file.items()):
            for fn in sorted(fns):
                _s, _e, body = per_file_funcs[rel][fn]
                hits = count_reads(body.split("\n"), objects, fname)
                want = declared_map.get((rel, fn), {}).get(fname)
                if hits and want is None:
                    errors.append(
                        "%s: 生产链函数 %s::%s 出现未登记引用点 (行内 %d 处) => "
                        "疑似新接线; 请同步更新清单与文档" % (fname, rel, fn, len(hits)))

    # ------- 死符号调用面: 已判定不可达的匹配器不得被生产链接线 -------
    for ds in man.get("dead_symbols", []):
        sym = ds["symbol"]
        total = 0
        where = []
        for (rel, fn) in sorted(reach):
            body = per_file_funcs[rel][fn][2]
            n = len(re.findall(r"\b" + re.escape(sym) + r"\s*\(", body))
            if n:
                total += n
                where.append("%s::%s" % (rel, fn))
        rows.append(("[dead_symbol] " + sym, ds["defined_in"], "-", 0, total, total == 0))
        if total:
            errors.append(
                "dead_symbol %s: 生产链出现 %d 处调用 (%s) => 该匹配器已被接线; "
                "请同步更新清单与文档" % (sym, total, ", ".join(where)))

    # ------- 冻结字面量: 生产路径的常数/默认值来源不可被悄悄换成配置 -------
    for lit in man.get("frozen_literals", []):
        rel, fn, pattern, want = lit["file"], lit["function"], lit["pattern"], lit["count"]
        if fn not in per_file_funcs.get(rel, {}):
            errors.append("frozen_literal: 函数 %s::%s 不存在" % (rel, fn))
            continue
        _s, _e, body = per_file_funcs[rel][fn]
        n = len(re.findall(pattern, body))
        rows.append(("[literal] " + lit["label"], rel, fn, want, n, n == want))
        if n != want:
            errors.append(
                "frozen_literal %s: %s::%s 匹配 %d != 冻结值 %d (%s)"
                % (lit["label"], rel, fn, n, want, lit["note"]))

    if not args.quiet:
        print("=" * 100)
        print("P27 死参数防误用机器锁 - 生产链函数闭包 (%d 个函数)" % len(reach))
        print("=" * 100)
        for (rel, fn) in sorted(reach):
            print("  + %-52s %s" % (fn, rel))
        print()
        print("=" * 100)
        print("逐字段引用点核对 (decl=冻结值, act=源码实测)")
        print("=" * 100)
        print("  %-34s %-40s %-30s %5s %5s %s"
              % ("field", "file", "function", "decl", "act", "OK"))
        for (fname, rel, fn, want, got, ok) in rows:
            print("  %-34s %-40s %-30s %5d %5d %s"
                  % (fname, os.path.basename(rel), fn, want, got,
                     "OK" if ok else "MISMATCH"))
        print()
        print("字段状态汇总:")
        for fname, fspec in man["fields"].items():
            print("  %-34s %-5s reads=%d  %s"
                  % (fname, fspec["status"],
                     sum(w for rel in fspec["allow_reads"]
                         for (_fn, w) in fspec["allow_reads"][rel]),
                     fspec["reason"]))

    if errors:
        print()
        print("P27_DEAD_PARAMS_LOCK FAIL (%d 处):" % len(errors))
        for e in errors:
            print("  - " + e)
        print()
        print("=> 死字段清单与实现不一致。请同步更新:")
        print("   lib/algorithms/platesolve/cpp/ipv/test/ipv_dead_params_manifest.json")
        print("   run/perf-fix/P27-dead-params/REPORT.md (逐字段核对表)")
        print("   docs/contracts/ (公共头/合同标注)")
        return 1
    print()
    print("P27_DEAD_PARAMS_LOCK PASS: 死字段清单与生产路径实现一致")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
