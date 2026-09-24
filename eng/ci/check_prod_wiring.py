#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-PROD-WIRING —— 「能力已实现但未接入生产」复发门（WIRING-AUDIT-01）。

防的复发缺口（负责人 2026-09 原话）：
  「阶段一第二步 cosmetic 是恒等 pass」这类**能力已实现但未接入生产**的缺陷 ——
  整体算法能跑通，但能力声明（module.yaml / registry / 公共头 / 配置键 / 文档）
  与生产调用图之间断开，且现有门只看「符号是否链进二进制」，看不见
  「链进去了但没人调」「调了但实参恒为退化字面量」「开关默认关」。

判据族（全部 fail-closed；任一红即 exit 1）：

  W1 declared_unreachable —— 声明符号在生产调用图不可达。
     声明面（逐类抽取）：lib/**/module.yaml 的 source_symbols/entrypoint、
     lib/**/*.integration.json 的 dll.unique_entry / operations[].entry、
     lib/infrastructure/pipeline/module_ports.registry.json 的
     operations[].entry 与 ports[].code[].symbol、公共头
     （lib/include/**、lib/**/include/**）里 AC_API/P2_API/... 标记的 C ABI 声明。
     生产调用图 = lib/** 生产源（排除 tests/tools/third_party/archive/build/acr）
     的函数级名字图，根 = 三个命令入口 cmd_session1_run/cmd_session2_run/
     cmd_session3_run。**过近似**：任何函数体内出现的标识符（含 dlsym 字符串
     字面量、函数指针表初值）都算一条边 ⇒ 判红方向可靠（不可达者必然真不可达），
     判绿方向可能偏松 ⇒ 用台账显式豁免补足。
     分类证据：①真未接入（全仓零调用点）/ ④只被测试或工具调用。

  W2 degenerate_argument —— 生产调用点实参恒为退化字面量。
     对已被 W1 判为可达的声明符号，取其全部生产调用点的实参表；若某个
     **输入侧形参**（const T* / const T& / 容器引用）在**所有**生产调用点都传
     退化字面量（nullptr / NULL / 0 / {} / ""），则该形参承载的能力在生产上
     恒不生效（cosmetic master_dark/master_bias 传 nullptr ⇒ 检测禁用恒等 pass
     就是这一型）。输出侧/出参不在本判据面（合法可空）。

  W3 dead_config_key —— 配置键无消费点。
     声明面：eng/packaging/config/defaults.json#fields[].key、
     eng/packaging/config/templates/*.json 叶子键、
     eng/contracts/schemas/phase_config_*.schema.json 的 properties 键。
     判据：叶子键名必须作为带引号 token 出现在 lib/** 生产源码里（同一键的
     别名必须进台账并写明 consumed_as）。

  W4 unbuildable_test —— tests/ 目录存在测试源但无构建目标。
     判据：tests/** 下的 C/C++ 源既不在任何 CMake 源清单里按文件名出现，
     其所在目录也不在任何 add_subdirectory() 参数里 ⇒ 判红（drizzle
     healpix_drizzle/tests 四测试 tracked-but-unbuilt 就是这一型：源在库里、
     全仓无 add_executable 引用、ctest 命中 0）。

  W5 switch_default_off —— 能力被声明为默认关的开关门住。
     判据：可达能力符号的调用点，其外层 if(...) 条件里出现的配置键，若该键在
     声明面的默认值为 false/0/off，则判红（"生长默认关且未接线"这一型）。

  W6 plugin_entry_unreachable —— dlopen 插件入口的宿主侧不可达。
     判据：dlsym/GetProcAddress/resolve_symbol 的字符串目标符号，其**全部宿主
     解析点**所在函数在生产调用图不可达 ⇒ 判红。跨语言与间接调用面：符号名以
     字符串字面量出现，不走名字调用图；且 C ABI vtable 被采集后插件实现会显得
     可达，只有本判据能说明「可达只是表里登记、宿主从不加载」。

  W0 stale_ledger_entry —— 台账僵尸条目。
     判据：台账条目在本轮判定里一次都没命中 ⇒ 判红。把「台账只减不增」从口号
     变成机器强制：被修好的条目必须删除、符号改名后必须同步 id、写错的 id 必须
     纠正。三种都要求人工确认，故不静默忽略。

豁免唯一途径：eng/ci/ledgers/prod_wiring.json 显式登记，每条必须带
id/kind/reason/owner/exit_condition（缺一即 rc=2），可选 authority（权威依据）
与 evidence（证据路径）。台账**只减不增**：ledger.high_water.max_entries 为
条数上限，超出即 rc=2。禁止用空理由/宽泛理由静默豁免。

退出码（三态，判红与崩溃分离）：
  0 = 全部能力可达且实参非退化、键有消费点、测试有构建目标；
  1 = 有 finding（判红）；
  2 = 锚点/台账/工具不可用（CHK-PROD-WIRING_CRASH，崩塌，不是判红）。

用法：
  python3 eng/ci/check_prod_wiring.py [--repo ROOT] [--json-out F]
      [--inventory-out F] [--self-test]
只读（--self-test 只写 tempfile）；仅 stdlib（yaml 可选，缺失时回落纯文本解析）。
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import sys
from collections import defaultdict, deque

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gate_common as gc  # noqa: E402

CHECK_ID = "CHK-PROD-WIRING"
LEDGER = "eng/ci/ledgers/prod_wiring.json"

# ── 生产源域 ────────────────────────────────────────────────────────────────
PROD_EXTS = (".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".inc", ".cu")
# acr: ASTROCS_DESIGN §2「ACR 生产不可达」+ eng/tools/quality/check_prod_reachability.py
# ACR_SYMBOLS 机器断言 —— 设计上不在生产调用图，故既不作声明面也不作可达面。
EXCLUDE_PARTS = {"third_party", "tests", "test", "tools", "archive", "build",
                 "acr", "examples", "benchmark"}
ENTRY_ROOTS = ("cmd_session1_run", "cmd_session2_run", "cmd_session3_run")

# 退化字面量：指针/容器输入侧恒等值
DEGENERATE_LITERALS = {"nullptr", "NULL", "0", "{}", "{ }", '""', "nullptr_t()"}
# 输入侧形参类型（承载能力效应的一侧）；输出侧/出参不在判据面
INPUT_PARAM_RE = re.compile(r"(const\s+[\w:<>,\s\*&]+[\*&]\s*|const\s+std::(?:vector|string|array)<[^>]*>\s*&)")
CONST_PTR_RE = re.compile(r"^\s*const\s+[\w:<>,\s]*[\*&]")

CXX_KEYWORDS = {"if", "for", "while", "switch", "catch", "return", "sizeof", "else",
                "do", "new", "delete", "throw", "case", "static_assert", "decltype",
                "alignof", "typeid", "and", "or", "not"}
IDENT_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
CALLHEAD_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(")
TYPE_HEAD_KW = ("struct", "class", "union", "enum", "namespace", "extern")


# ═══════════════════════════════════════════════════════════════════════════
# 一、C/C++ 词法层：去注释 / 配平 / 函数定义与调用点
# ═══════════════════════════════════════════════════════════════════════════
def strip_comments(text: str) -> str:
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            i = n if j < 0 else j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def match_pair(text: str, i: int, open_ch: str, close_ch: str) -> int:
    depth, n, k = 0, len(text), i
    while k < n:
        c = text[k]
        if c == open_ch:
            depth += 1
        elif c == close_ch:
            depth -= 1
            if depth == 0:
                return k
        elif c == '"':
            k += 1
            while k < n and text[k] != '"':
                if text[k] == "\\":
                    k += 1
                k += 1
        k += 1
    return -1


def stmt_start(text: str, pos: int) -> int:
    k = pos - 1
    while k >= 0 and text[k] not in ";{}":
        k -= 1
    return k + 1


def find_defs(text: str):
    """返回 [(name, decl_start, body_start, body_end)]。"""
    defs = []
    for m in CALLHEAD_RE.finditer(text):
        name = m.group(1)
        if name in CXX_KEYWORDS:
            continue
        lp = m.end() - 1
        rp = match_pair(text, lp, "(", ")")
        if rp < 0:
            continue
        j, n = rp + 1, len(text)
        while j < n and text[j] in " \t\r\n":
            j += 1
        if not (j < n and text[j] == "{"):
            k = j
            while k < n and text[k] not in ";{}":
                k += 1
            tail = text[j:k]
            # function-try-block（形如 "int f(...) try { ... } catch (...) { ... }"）也是
            # 函数定义：漏掉整个函数体会让体内调用点被当成「文件域」，把真实接线误判成
            # 不可达（假红，实证：hp_drizzle_api.cpp 的 run_drizzle_internal）。
            if not (k < n and text[k] == "{" and k - j < 4000
                    and re.search(r"noexcept|override|final|->|try\b|:\s*\w+\s*\(", tail)):
                continue
            j = k
        pre = text[m.start() - 1] if m.start() > 0 else " "
        if pre.isalnum() or pre == "_":
            continue
        seg = text[max(0, m.start() - 160):m.start()]
        if re.search(r"(^|[^\w:>])(return|=|\(|,|&&|\|\||!|\+|-|\*|/)\s*$", seg):
            continue
        end = match_pair(text, j, "{", "}")
        if end < 0:
            continue
        defs.append((name, stmt_start(text, m.start()), j, end))
    return defs


BLOCK_RECURSE_KW = ("namespace", "extern")
BLOCK_SKIP_KW = ("struct", "class", "union", "enum")


def scope_init_uses(text: str, bodies) -> set:
    """文件域初始化式（函数指针表 / op 分发表 / C ABI vtable）里出现的标识符。

    三类块的处理口径（每类都写明理由）：
      * struct/class/union/enum -> 不进：成员与方法声明不是接线事实；
      * namespace / extern "C"  -> 递归进：C ABI 模块入口的九操作 vtable
        （static const acs_module_api_v1 g_x_module_api = {...}）就写在
        extern "C" 块内；整块跳过会把「表里登记了实现」误判成「没人调」；
      * 形如 T x = {...} / T x{...} 的初始化式 -> 采集花括号内标识符。
    """
    mask = bytearray(len(text))
    for (a, b) in bodies:
        for i in range(a, min(b + 1, len(text))):
            mask[i] = 1
    out = set()

    def walk(lo: int, hi: int) -> None:
        i = lo
        while i < hi:
            if mask[i] or text[i] != "{":
                i += 1
                continue
            e = match_pair(text, i, "{", "}")
            if e < 0 or e > hi:
                return
            head = text[stmt_start(text, i):i]
            hit = [k for k in BLOCK_RECURSE_KW + BLOCK_SKIP_KW if re.search(r"\b%s\b" % k, head)]
            if hit and hit[0] in BLOCK_SKIP_KW:
                i = e + 1
                continue
            if hit:                      # namespace / extern 块：递归进内容
                walk(i + 1, e)
                i = e + 1
                continue
            if (("=" in head or (")" not in head
                                 and not any(k in head.split()[-1:] for k in TYPE_HEAD_KW)))
                    and len(head) < 600):
                for mm in IDENT_RE.finditer(text, i, e + 1):
                    out.add(mm.group(0))
            i = e + 1
    walk(0, len(text))
    return out


DLSYM_RE = re.compile(
    r'(?:dlsym|GetProcAddress|resolve_symbol|resolve_proc|GetProcAddress)\s*\([^;()]*?"([A-Za-z_]\w*)"')


def enclosing_def_name(graph: "Graph", rel: str, pos: int):
    """pos 所在的最内层函数定义名（dlopen 插件宿主函数定位用）。"""
    best = None
    for name, defs in graph.defs.items():
        for (f, _ds, bs, be) in defs:
            if f != rel or not (bs <= pos <= be):
                continue
            if best is None or (be - bs) < best[1]:
                best = (name, be - bs)
    return best[0] if best else None


def _param_null_guarded(graph: "Graph", sym: str, pname: str) -> str:
    """被调函数（或其同文件直接被调者）里是否存在该形参的 null 守卫。

    有守卫 ⇒ 该形参按设计可空（degraded「设计可选」），不是「能力被意外禁用」。
    返回守卫所在的 file:line 证据串，无守卫返回空串。
    """
    if not pname:
        return ""
    pat = re.compile(r"(!\s*%s\b|%s\s*(?:==|!=)\s*(?:nullptr|NULL)|%s\s*\?|if\s*\(\s*%s\s*\))"
                     % (pname, pname, pname, pname))
    for (rel, _ds, bs, be) in graph.defs.get(sym, [])[:1]:
        t = graph.text[rel]
        body = t[bs:be + 1]
        m = pat.search(body)
        if m:
            return "%s:%d" % (rel, t.count("\n", 0, bs + m.start()) + 1)
        # 一级下钻：同文件内被本函数调用的其它定义
        for callee in {x.group(0) for x in IDENT_RE.finditer(body)}:
            for (r2, _d2, b2, e2) in graph.defs.get(callee, ()):
                if r2 != rel:
                    continue
                m2 = pat.search(t[b2:e2 + 1])
                if m2:
                    return "%s:%d" % (r2, t.count("\n", 0, b2 + m2.start()) + 1)
    return ""


def collect_plugin_hosts(graph: "Graph", reach_names):
    """返回 {dlsym 目标符号: [(file, line, 宿主函数, 宿主可达)]}（跨语言/间接调用面）。"""
    by_sym = defaultdict(list)
    for rel, t in graph.text.items():
        for m in DLSYM_RE.finditer(t):
            sym = m.group(1)
            host = enclosing_def_name(graph, rel, m.start())
            by_sym[sym].append((rel, t.count("\n", 0, m.start()) + 1, host,
                                bool(host) and host in reach_names))
    return by_sym

def production_files(repo: pathlib.Path):
    base = repo / "lib"
    if not base.is_dir():
        raise gc.GateError("ANCHOR_MISSING: lib/ 生产源根不存在")
    out = []
    for p in base.rglob("*"):
        if not p.is_file() or p.suffix not in PROD_EXTS:
            continue
        if any(part in EXCLUDE_PARTS for part in p.parts):
            continue
        out.append(p)
    if not out:
        raise gc.GateError("ANCHOR_STALE: lib/** 零生产源（判据面为空）")
    return sorted(out)


class Graph:
    """生产函数级名字调用图（过近似，见模块 docstring）。"""

    def __init__(self, repo: pathlib.Path):
        self.repo = repo
        self.defs = defaultdict(list)        # name -> [(rel, decl_start, body_start, body_end)]
        self.edges = defaultdict(set)        # (rel, name) -> {used identifiers}
        self.scope = {}                      # rel -> file-scope initializer identifiers
        self.bodies = defaultdict(list)      # rel -> [(body_start, body_end)] 全部函数体
        self.text = {}                       # rel -> 去注释源码
        self.token_files = defaultdict(set)  # 标识符 -> 出现的文件（调用点检索倒排，避免全仓重扫）
        self.files = []
        for p in production_files(repo):
            rel = p.relative_to(repo).as_posix()
            raw = p.read_text(encoding="utf-8", errors="replace")
            t = strip_comments(raw)
            self.text[rel] = t
            self.files.append(rel)
            for m in IDENT_RE.finditer(t):
                self.token_files[m.group(0)].add(rel)
            bodies = []
            for (name, ds, bs, be) in find_defs(t):
                self.defs[name].append((rel, ds, bs, be))
                self.edges[(rel, name)] |= {x.group(0) for x in IDENT_RE.finditer(t, bs, be + 1)}
                bodies.append((ds, be))
                self.bodies[rel].append((bs, be))
            self.scope[rel] = scope_init_uses(t, bodies)

    def reachable(self, skip_scope_files=()):
        """从三个命令入口做可达性 BFS。

        skip_scope_files：这些文件的「文件域初始化表」不被采集（用于判定
        「某符号只经由该文件的静态 vtable 才显得可达」——典型是 C ABI 模块 DLL
        的九操作 vtable：表里登记了实现，但宿主从不 dlopen 该 DLL）。
        """
        seen, seenf, q = set(), set(), deque()
        skip = set(skip_scope_files)

        def add_file(f):
            if f in seenf:
                return
            seenf.add(f)
            if f in skip:
                return
            for u in self.scope.get(f, ()):
                add(u)

        def add(name):
            if name in seen:
                return
            seen.add(name)
            q.append(name)
            for (rel, _ds, _bs, _be) in self.defs.get(name, ()):
                add_file(rel)

        for r in ENTRY_ROOTS:
            if r not in self.defs:
                raise gc.GateError(
                    "ANCHOR_STALE: 生产入口 %s 未在 lib/** 定义（可达性面无根）" % r)
            add(r)
        while q:
            name = q.popleft()
            for (rel, _ds, _bs, _be) in self.defs.get(name, ()):
                add_file(rel)
                for u in self.edges.get((rel, name), ()):
                    add(u)
        return seen, seenf

    def call_sites(self, name: str, prod_only: bool = True):
        """返回 [(rel, line, args_text)]；跳过定义处签名与声明。"""
        out = []
        mask_ranges = defaultdict(list)
        for (rel, ds, bs, be) in self.defs.get(name, ()):
            mask_ranges[rel].append((ds, be))
        pat = re.compile(r"\b%s\s*\(" % re.escape(name))
        files = sorted(self.token_files.get(name, ()))
        for rel in files:
            t = self.text[rel]
            for m in pat.finditer(t):
                if m.start() > 0 and (t[m.start() - 1].isalnum() or t[m.start() - 1] == "_"):
                    continue
                if any(a <= m.start() <= b for (a, b) in mask_ranges.get(rel, ())):
                    continue
                # 只认「函数体内」的出现：文件域/类体里的声明不是调用点
                if not any(a <= m.start() <= b for (a, b) in self.bodies.get(rel, ())):
                    continue
                lp = m.end() - 1
                rp = match_pair(t, lp, "(", ")")
                if rp < 0:
                    continue
                out.append((rel, t.count("\n", 0, m.start()) + 1, t[lp + 1:rp]))
        return out


def split_args(args_text: str):
    parts, depth_p, depth_b, depth_a, cur = [], 0, 0, 0, []
    in_str = False
    i, n = 0, len(args_text)
    while i < n:
        c = args_text[i]
        if in_str:
            cur.append(c)
            if c == "\\":
                i += 1
                if i < n:
                    cur.append(args_text[i])
            elif c == '"':
                in_str = False
            i += 1
            continue
        if c == '"':
            in_str = True
        elif c == "(":
            depth_p += 1
        elif c == ")":
            depth_p -= 1
        elif c == "{":
            depth_b += 1
        elif c == "}":
            depth_b -= 1
        elif c == "[":
            depth_a += 1
        elif c == "]":
            depth_a -= 1
        elif c == "," and depth_p == 0 and depth_b == 0 and depth_a == 0:
            parts.append("".join(cur).strip())
            cur = []
            i += 1
            continue
        cur.append(c)
        i += 1
    if "".join(cur).strip() or parts:
        parts.append("".join(cur).strip())
    return parts


def param_names(sig_text: str):
    """从形参表文本抽取形参名（用于 W2 证据），失败返回 []。"""
    names = []
    for raw in split_args(sig_text):
        s = re.sub(r"=\s*[^,]*$", "", raw).strip()
        if not s or s == "void":
            continue
        m = re.search(r"([A-Za-z_]\w*)\s*(\[\s*\])?\s*$", s)
        names.append(m.group(1) if m else "")
    return names


def signature_params(repo: pathlib.Path, graph: Graph, name: str):
    """返回被调函数的形参文本列表（首个生产定义）。"""
    for (rel, _ds, bs, _be) in graph.defs.get(name, ()):
        t = graph.text[rel]
        # 从 body_start 往回找配对的 '('
        j = bs - 1
        while j >= 0 and t[j] in " \t\r\n":
            j -= 1
        if j < 0 or t[j] != ")":
            continue
        depth, k = 0, j
        while k >= 0:
            if t[k] == ")":
                depth += 1
            elif t[k] == "(":
                depth -= 1
                if depth == 0:
                    break
            k -= 1
        if k < 0:
            continue
        return split_args(t[k + 1:j])
    return []


# ═══════════════════════════════════════════════════════════════════════════
# 二、声明面抽取（逐类）
# ═══════════════════════════════════════════════════════════════════════════
class Decl:
    __slots__ = ("source", "symbol", "where", "module_id", "line")

    def __init__(self, source, symbol, where, module_id=None, line=None):
        self.source = source
        self.symbol = symbol
        self.where = where
        self.module_id = module_id
        self.line = line

    @property
    def key(self):
        return "%s:%s" % (self.source, self.symbol)


def _load_yaml(path):
    try:
        import yaml  # noqa: PLC0415
    except ImportError:
        return None
    try:
        return yaml.safe_load(path.read_text(encoding="utf-8")) or {}
    except Exception:  # noqa: BLE001
        return None


def decl_from_manifests(repo: pathlib.Path):
    out = []
    globs = sorted((repo / "lib").rglob("module.yaml"))
    if not globs:
        raise gc.GateError("ANCHOR_MISSING: lib/**/module.yaml 缺失（声明面无输入）")
    for p in globs:
        doc = _load_yaml(p)
        if not isinstance(doc, dict):
            raise gc.GateError("ANCHOR_UNPARSABLE: %s（需 PyYAML 解析 module.yaml）"
                               % p.relative_to(repo).as_posix())
        rel = p.relative_to(repo).as_posix()
        mid = doc.get("module_id") or doc.get("id") or rel
        for sym in doc.get("source_symbols") or []:
            nm = str(sym).split("(")[0].strip()
            if re.fullmatch(r"[A-Za-z_]\w*", nm):
                out.append(Decl("manifest.source_symbols", nm, rel, mid))
        ep = doc.get("entrypoint")
        if isinstance(ep, str) and re.fullmatch(r"[A-Za-z_]\w*", ep):
            out.append(Decl("manifest.entrypoint", ep, rel, mid))
    return out


def decl_from_integration(repo: pathlib.Path):
    out = []
    for p in sorted((repo / "lib").rglob("*.integration.json")):
        doc = gc.read_json(p, p.relative_to(repo).as_posix())
        rel = p.relative_to(repo).as_posix()
        mid = doc.get("module_id")
        ue = (doc.get("dll") or {}).get("unique_entry")
        if isinstance(ue, str) and re.fullmatch(r"[A-Za-z_]\w*", ue):
            out.append(Decl("integration.unique_entry", ue, rel, mid))
        for op in doc.get("operations") or []:
            e = op.get("entry") if isinstance(op, dict) else None
            if isinstance(e, str) and re.fullmatch(r"[A-Za-z_]\w*", e):
                out.append(Decl("integration.op_entry", e, rel, mid))
    return out


def decl_from_registry(repo: pathlib.Path):
    p = repo / "lib/infrastructure/pipeline/module_ports.registry.json"
    doc = gc.read_json(p, "module_ports.registry.json")
    rel = p.relative_to(repo).as_posix()
    out = []
    for module in doc.get("modules") or []:
        mid = module.get("module_id")
        for op in module.get("operations") or []:
            e = op.get("entry")
            if isinstance(e, str) and re.fullmatch(r"[A-Za-z_]\w*", e):
                out.append(Decl("registry.entry", e, rel, mid))
            for port in op.get("ports") or []:
                for c in port.get("code") or []:
                    s = c.get("symbol")
                    if isinstance(s, str) and re.fullmatch(r"[A-Za-z_]\w*", s):
                        out.append(Decl("registry.code_anchor", s,
                                        c.get("file") or rel, mid))
    if not out:
        raise gc.GateError("ANCHOR_STALE: %s 零 (operation, entry, code) 声明" % rel)
    return out


API_MARK_RE = re.compile(
    r"(?m)^[^\n;#]*(?P<mark>AC_API|P2_API|P3_API|ACS_API|HP_API|ACS_EXPORT|GAIA_API|DRZ_API)"
    r"[^\n;]*?\b(?P<name>[A-Za-z_]\w*)\s*\(")


def decl_from_public_headers(repo: pathlib.Path):
    out = []
    headers = []
    for pat in ("lib/include/**/*.h", "lib/include/**/*.hpp",
                "lib/**/include/**/*.h", "lib/**/include/**/*.hpp"):
        headers.extend(sorted(repo.glob(pat)))
    headers = [p for p in headers if "third_party" not in p.parts]
    if not headers:
        raise gc.GateError("ANCHOR_MISSING: 公共头（lib/**/include/**）为零")
    for p in headers:
        rel = p.relative_to(repo).as_posix()
        t = strip_comments(p.read_text(encoding="utf-8", errors="replace"))
        for m in API_MARK_RE.finditer(t):
            out.append(Decl("header." + m.group("mark"), m.group("name"), rel,
                            None, t.count("\n", 0, m.start()) + 1))
    if not out:
        raise gc.GateError("ANCHOR_STALE: 公共头零 C ABI 标记声明（判据面为空）")
    return out


def collect_declarations(repo: pathlib.Path):
    decls = (decl_from_manifests(repo) + decl_from_integration(repo)
             + decl_from_registry(repo) + decl_from_public_headers(repo))
    if not decls:
        raise gc.GateError("ANCHOR_STALE: 声明面零条目")
    return decls


# ═══════════════════════════════════════════════════════════════════════════
# 三、配置键声明面
# ═══════════════════════════════════════════════════════════════════════════
def _leaves(node, prefix=""):
    if isinstance(node, dict):
        for k, v in node.items():
            yield from _leaves(v, prefix + k + ".")
    elif isinstance(node, list):
        for item in node:
            yield from _leaves(item, prefix)
    else:
        yield prefix.rstrip(".")


def _schema_property_keys(props: dict, prefix: str):
    """JSON-Schema 形态的配置键：只沿 properties 递归（schema 关键字如 type/default 不是键）。"""
    out = []
    for k, v in (props or {}).items():
        kp = prefix + k
        out.append(kp)
        if isinstance(v, dict) and isinstance(v.get("properties"), dict):
            out.extend(_schema_property_keys(v["properties"], kp + "."))
    return out


def config_keys(repo: pathlib.Path):
    """返回 [(key_path, leaf, source_rel)]。"""
    out = []
    defaults = repo / "eng/packaging/config/defaults.json"
    if defaults.is_file():
        doc = gc.read_json(defaults, "defaults.json")
        for f in doc.get("fields") or []:
            k = f.get("key") if isinstance(f, dict) else None
            if isinstance(k, str) and k.strip():
                out.append((k, k.split(".")[-1], "eng/packaging/config/defaults.json"))
    for p in sorted(repo.glob("eng/packaging/config/templates/*.json")):
        doc = gc.read_json(p, p.relative_to(repo).as_posix())
        roots = []
        if isinstance(doc.get("config"), dict):
            roots.append(("", doc["config"]))
        for blk in doc.get("blocks") or []:
            if isinstance(blk, dict):
                roots.append(("blocks[].", blk))
        if not roots:
            raise gc.GateError("ANCHOR_STALE: %s 无 config/blocks 数据块"
                               % p.relative_to(repo).as_posix())
        for prefix, root in roots:
            for leaf_path in _leaves(root, prefix):
                if leaf_path:
                    out.append((leaf_path, leaf_path.split(".")[-1],
                                p.relative_to(repo).as_posix()))
    schemas = sorted(repo.glob("eng/contracts/schemas/phase_config_*.schema.json"))
    if not schemas:
        raise gc.GateError("ANCHOR_MISSING: eng/contracts/schemas/phase_config_*.schema.json 缺失")
    for p in schemas:
        doc = gc.read_json(p, p.relative_to(repo).as_posix())
        # 两种形态：① 顶层 properties 平铺配置键（phase_config_*.schema.json 现状）；
        # ② properties.config.properties（模板/合同包装形态）。
        props = doc.get("properties") or {}
        if isinstance(props.get("config"), dict) and props["config"].get("properties"):
            props = props["config"]["properties"]
        if not props:
            raise gc.GateError("ANCHOR_STALE: %s 无 properties（配置键声明面缺失）"
                               % p.relative_to(repo).as_posix())
        for kp in _schema_property_keys(props, ""):
            out.append((kp, kp.split(".")[-1], p.relative_to(repo).as_posix()))
        blocks = props.get("blocks")
        if isinstance(blocks, dict) and isinstance(blocks.get("items"), dict):
            bprops = blocks["items"].get("properties")
            if isinstance(bprops, dict):
                for kp in _schema_property_keys(bprops, "blocks[]."):
                    out.append((kp, kp.split(".")[-1], p.relative_to(repo).as_posix()))
    if not out:
        raise gc.GateError("ANCHOR_STALE: 配置键声明面为零")
    return out


def config_key_defaults(repo: pathlib.Path):
    """key_path -> 默认值（用于 W5 判「默认关」）。"""
    out = {}
    defaults = repo / "eng/packaging/config/defaults.json"
    if defaults.is_file():
        doc = gc.read_json(defaults, "defaults.json")
        for f in doc.get("fields") or []:
            if isinstance(f, dict) and isinstance(f.get("key"), str):
                out[f["key"]] = f.get("value")
    for p in sorted(repo.glob("eng/packaging/config/templates/*.json")):
        doc = gc.read_json(p, p.relative_to(repo).as_posix())

        def walk(node, prefix):
            if isinstance(node, dict):
                for k, v in node.items():
                    walk(v, prefix + k + ".")
            elif isinstance(node, list):
                for item in node:
                    walk(item, prefix)
            else:
                out.setdefault(prefix.rstrip("."), node)
        for blk in doc.get("blocks") or []:
            if isinstance(blk, dict):
                walk(blk, "")
        if isinstance(doc.get("config"), dict):
            walk(doc["config"], "")
    return out


OFF_VALUES = (False, 0, "false", "off", "none", "disabled")


# ═══════════════════════════════════════════════════════════════════════════
# 四、判据
# ═══════════════════════════════════════════════════════════════════════════
def build_nonprod_index(repo: pathlib.Path):
    """测试/工具面索引：标识符 -> 出现文件（分类 ④ 证据用；一次扫，避免逐符号重扫）。"""
    idx = defaultdict(set)
    for base in ("lib", "eng"):
        root = repo / base
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if not p.is_file() or p.suffix not in PROD_EXTS:
                continue
            if not any(part in ("tests", "test", "tools", "examples") for part in p.parts):
                continue
            if set(p.parts) & {"third_party", "archive", "build"}:
                continue
            t = strip_comments(p.read_text(encoding="utf-8", errors="replace"))
            for m in set(x.group(0) for x in IDENT_RE.finditer(t)):
                idx[m].add(p.relative_to(repo).as_posix())
    return idx


def _nonprod_call_sites(index, name: str):
    return sorted(index.get(name, ()))


def evaluate(repo: pathlib.Path, symbols_file=None):
    graph = Graph(repo)
    reach_names, reach_files = graph.reachable()
    nonprod_idx = build_nonprod_index(repo)
    plugin_hosts = collect_plugin_hosts(graph, reach_names)
    # 插件通道专属可达面：把 dlsym 目标（插件入口）所在文件的静态 vtable 关掉后
    # 仍可达的符号 = 真正的生产可达；只在关掉后才不可达的符号 = 仅经插件 vtable
    # 显得可达（通道本身由 W6 判红，见 report §4.3）。
    plugin_entry_files = {f for sym in plugin_hosts for (f, _d, _b, _e) in graph.defs.get(sym, ())}
    reach_no_plugin, _rf2 = graph.reachable(skip_scope_files=plugin_entry_files)
    decls = collect_declarations(repo)
    ledger = gc.load_ledger(repo / LEDGER, LEDGER)
    _check_high_water(repo, ledger)

    findings = []
    inventory = []
    seen_keys = set()
    ledgered = []   # 台账承载项：显式可见（不静默消红），在 extra 里如实列出

    # ── W1 + W2 ──────────────────────────────────────────────────────────
    for d in sorted(decls, key=lambda x: (x.source, x.symbol)):
        defined = graph.defs.get(d.symbol)
        if not defined:
            inventory.append({"symbol": d.symbol, "source": d.source, "where": d.where,
                              "verdict": "NOT_A_PRODUCTION_FUNCTION", "category": "-",
                              "evidence": "lib/** 生产源无该名字的函数定义（类型/宏/外部符号）"})
            continue
        reachable = d.symbol in reach_names
        nonprod = [] if reachable else _nonprod_call_sites(nonprod_idx, d.symbol)
        if reachable:
            if d.symbol not in reach_no_plugin:
                verdict, category = "WIRED_ONLY_VIA_PLUGIN_DLL", "①"
                evidence = ("仅经插件 DLL 的 C ABI vtable 显得可达（关掉该 vtable 后不可达）；"
                            "def=%s；通道本身由 W6 判红"
                            % ",".join(f for (f, _a, _b, _c) in defined[:2]))
            else:
                verdict, category = "WIRED", "⑤"
                evidence = "def=%s" % ",".join(f for (f, _a, _b, _c) in defined[:2])
        else:
            verdict = "UNREACHABLE"
            category = "④" if nonprod else "①"
            plugin_sites = plugin_hosts.get(d.symbol, ())
            evidence = ("全仓零调用点；def=%s" % ",".join(f for (f, _a, _b, _c) in defined[:2])
                        if not nonprod else
                        "调用点仅在非生产面：%s" % ",".join(nonprod[:3]))
            if plugin_sites:
                evidence += ("；生产侧 dlsym 宿主引用：%s"
                             % ",".join("%s:%d(host=%s)" % (r, l, h or "?")
                                        for (r, l, h, _ok) in plugin_sites[:2]))
        inventory.append({"symbol": d.symbol, "source": d.source, "where": d.where,
                          "module_id": d.module_id, "verdict": verdict,
                          "category": category, "evidence": evidence})
        if not reachable:
            key = "unreachable:%s:%s" % (d.source, d.symbol)
            if key in ledger or _alias_hit(ledger, d.symbol):
                ledgered.append(key)
                continue
            if key in seen_keys:
                continue
            seen_keys.add(key)
            findings.append({"id": key, "family": "W1", "category": category,
                             "symbol": d.symbol, "declared_at": d.where,
                             "source": d.source, "evidence": evidence})

    # W2：可达能力的生产调用点实参恒退化
    wired_symbols = sorted({d.symbol for d in decls
                            if d.symbol in graph.defs and d.symbol in reach_names})
    for sym in wired_symbols:
        params = signature_params(repo, graph, sym)
        if not params:
            continue
        sites = graph.call_sites(sym, prod_only=True)
        if not sites:
            continue
        args_per_site = [split_args(a) for (_r, _l, a) in sites]
        if any(len(a) != len(params) for a in args_per_site):
            continue  # 变参/默认参数/宏包装：本判据不覆盖
        for idx, ptext in enumerate(params):
            if not CONST_PTR_RE.match(ptext):
                continue  # 只判输入侧 const 指针/引用
            vals = [a[idx].strip() for a in args_per_site]
            if not vals:
                continue
            grades = [_arg_degeneracy(v) for v in vals]
            if any(g == "live" for g in grades):
                continue          # 至少一个生产调用点真供源 ⇒ 该形参承载的能力在生产上有效
            hard = all(g == "literal" for g in grades)
            guard = _param_null_guarded(graph, sym, (param_names(", ".join([ptext])) or [""])[0])
            if guard:
                hard = False      # 被调函数里有该形参的 null 守卫 ⇒ 设计可选，降级
            key = ("degenerate_arg:%s:%d" if hard else "nullable_source:%s:%d") % (sym, idx)
            evidence = ("%d/%d 个生产调用点该形参%s %s；例：%s:%d"
                        % (len(vals), len(sites),
                           "恒为退化字面量" if hard else "恒为含 null 分支的条件式（degraded）",
                           sorted(set(vals))[:3], sites[0][0], sites[0][1]))
            inventory.append({"symbol": sym, "source": "call_site",
                              "where": "%s:%d" % (sites[0][0], sites[0][1]),
                              "verdict": "DEGENERATE_ARG" if hard else "NULLABLE_SOURCE_ONLY",
                              "category": "②", "evidence": evidence})
            if key in ledger or _alias_hit(ledger, sym):
                ledgered.append(key)
                continue
            if key in seen_keys:
                continue
            seen_keys.add(key)
            findings.append({"id": key, "family": "W2", "category": "②",
                             "severity": "hard" if hard else "degraded",
                             "symbol": sym, "declared_at": "%s:%d" % (sites[0][0], sites[0][1]),
                             "source": "call_site",
                             "evidence": evidence + "；形参=%s" % " ".join(ptext.split())[:80]})

    # ── W3：配置键消费面 ────────────────────────────────────────────────
    blob = "\n".join(graph.text.values())
    keys = config_keys(repo)
    dead = []
    for (kpath, leaf, src) in keys:
        if ('"%s"' % leaf) in blob:
            continue
        dead.append((kpath, leaf, src))
        key = "dead_config_key:%s" % kpath
        if key in ledger:
            ledgered.append(key)
            continue
        if key in seen_keys:
            continue
        seen_keys.add(key)
        findings.append({"id": key, "family": "W3", "category": "①",
                         "symbol": kpath, "declared_at": src, "source": "config",
                         "evidence": "生产源码零命中带引号键 token \"%s\"" % leaf})

    # ── W4：测试资产可构建性 ────────────────────────────────────────────
    cmake_text = _cmake_blob(repo)
    compiled, graph_mode = _compiled_sources(repo)
    script_text = _script_blob(repo)
    unbuildable = []
    for p in _test_sources(repo):
        rel = p.relative_to(repo).as_posix()
        if p.name in compiled or rel in compiled:
            continue
        if p.name in cmake_text or rel in cmake_text:
            continue
        if p.name in script_text:
            continue  # 脚本驱动编译（Python/shell harness）：有构建路径，不计 W4
        unbuildable.append(rel)
    for rel in unbuildable:
        key = "unbuildable_test:%s" % rel
        if key in ledger:
            ledgered.append(key)
            continue
        if key in seen_keys:
            continue
        seen_keys.add(key)
        findings.append({"id": key, "family": "W4", "category": "①",
                         "symbol": rel, "declared_at": rel, "source": "tests",
                         "evidence": "tests/ 下测试源无构建目标（构建图 %s 无编译边、"
                                     "CMake 源清单无引用、无脚本驱动编译）" % graph_mode})

    # ── W5：默认关的开关门住能力 ────────────────────────────────────────
    kdefaults = config_key_defaults(repo)
    off_keys = {k: v for k, v in kdefaults.items() if v in OFF_VALUES}
    for sym in wired_symbols:
        for (rel, line, args) in graph.call_sites(sym, prod_only=True):
            guard = _enclosing_guard(graph.text[rel], _offset_of_line(graph.text[rel], line))
            if not guard:
                continue
            for key_path, default in off_keys.items():
                leaf = key_path.split(".")[-1]
                if re.search(r'[\"\']%s[\"\']' % re.escape(leaf), guard):
                    fid = "switch_default_off:%s:%s" % (sym, key_path)
                    ev = ("调用点 %s:%d 外层门 %s 使用键 %s（声明默认=%r）"
                          % (rel, line, guard.strip()[:60], key_path, default))
                    inventory.append({"symbol": sym, "source": "call_site",
                                      "where": "%s:%d" % (rel, line),
                                      "verdict": "SWITCH_DEFAULT_OFF", "category": "③",
                                      "evidence": ev})
                    if fid in ledger:
                        ledgered.append(fid)
                        continue
                    if fid in seen_keys:
                        continue
                    seen_keys.add(fid)
                    findings.append({"id": fid, "family": "W5", "category": "③",
                                     "symbol": sym, "declared_at": "%s:%d" % (rel, line),
                                     "source": "call_site", "evidence": ev})
    # ── W6：dlopen/GetProcAddress 插件入口的宿主侧不可达 ──────────────────
    # 跨语言与间接调用面：符号名以字符串字面量出现在生产源里（dlsym）。这类接线
    # 不经过名字调用图，必须单独判：**宿主解析函数本身不可达** ⇒ 整个插件通道
    # 死掉（即使被 dlsym 的实现在源码里"有人引用"）。这条同时堵住一个陷阱：
    # extern "C" vtable 被采集后，插件实现会显得"可达"，只有本判据能说明
    # 「可达只是表里登记，宿主从不加载」。
    for sym, sites in sorted(plugin_hosts.items()):
        if any(ok for (_r, _l, _h, ok) in sites):
            continue
        key = "plugin_entry_unreachable:%s" % sym
        ev = ("dlsym 目标 %s 的宿主解析点全部不可达：%s"
              % (sym, "; ".join("%s:%d(host=%s)" % (r, l, h or "?") for (r, l, h, _ok) in sites[:3])))
        inventory.append({"symbol": sym, "source": "plugin.dlsym", "where": "%s:%d" % (sites[0][0], sites[0][1]),
                          "verdict": "PLUGIN_CHANNEL_UNREACHABLE", "category": "①", "evidence": ev})
        if key in ledger:
            ledgered.append(key)
            continue
        if key in seen_keys:
            continue
        seen_keys.add(key)
        findings.append({"id": key, "family": "W6", "category": "①", "severity": "hard",
                         "symbol": sym, "declared_at": "%s:%d" % (sites[0][0], sites[0][1]),
                         "source": "plugin.dlsym", "evidence": ev})

    # ── W0：台账僵尸条目（把「只减不增」从口号变成机器强制）────────────────
    # 台账条目若本轮一次都没命中，说明它承载的 finding 已不存在：被修好（台账该
    # 缩小）、键名/符号名漂移后失配（必须改 id）、或当初就写错（台账自身缺陷）。
    # 三种都必须人工确认，故判红而不是静默忽略。
    for _lid, _le in sorted(ledger.items()):
        if _lid in ledgered or _lid in seen_keys:
            continue
        _key = "stale_ledger_entry:%s" % _lid
        if _key in seen_keys:
            continue
        seen_keys.add(_key)
        findings.append({"id": _key, "family": "W0", "category": "台账", "severity": "hard",
                         "symbol": _lid, "declared_at": LEDGER, "source": "ledger",
                         "evidence": ("台账条目未命中任何判定（僵尸条目）：%s；被修好则删除该条并把 high_water.max_entries 下调" % str(_le.get("reason", ""))[:90])})

    # 清单去重（同一 (符号, 声明来源, 声明处, 判定) 只留一行；不改判据面）
    dedup = {}
    for row in inventory:
        dedup[(row.get("symbol"), row.get("source"), row.get("where"), row.get("verdict"))] = row
    inventory = sorted(dedup.values(), key=lambda r: (r.get("verdict", ""), r.get("source", ""),
                                                      r.get("symbol", "")))

    findings.sort(key=lambda f: (f["family"], f["id"]))
    extra = {
        "production_files": len(graph.files),
        "reachable_files": len(reach_files),
        "declaration_count": len(decls),
        "declared_symbol_count": len({d.symbol for d in decls}),
        "config_key_count": len(keys),
        "dead_config_key_count": len(dead),
        "test_source_count": len(_test_sources(repo)),
        "unbuildable_test_count": len(unbuildable),
        "entry_roots": list(ENTRY_ROOTS),
        "ledgered_findings": sorted(set(ledgered)),
        "ledgered_count": len(set(ledgered)),
    }
    return findings, inventory, extra


def _arg_degeneracy(v: str) -> str:
    """实参退化度：literal（纯退化字面量）/ nullable（含 null 分支的条件式）/ live。

    nullable 对应「永远走 null 分支的条件式」这一型（cosmetic 的
    pdark.empty() ? (const float*)NULL : (const float*)pdark.data() 即此）。
    它比纯字面量弱一档，故单独判为 degraded 级 finding，需核验条件在生产上是否恒真。
    """
    s = v.strip()
    if _is_degenerate(s):
        return "literal"
    if re.search(r"\bnullptr\b|\bNULL\b|\bnullopt\b", s):
        return "nullable"
    return "live"


def _is_degenerate(v: str) -> bool:
    s = v.strip()
    if s in DEGENERATE_LITERALS:
        return True
    if re.fullmatch(r"\w*nullptr\w*", s):
        return True
    if re.fullmatch(r"static_cast<[^>]*>\(\s*(nullptr|NULL|0)\s*\)", s):
        return True
    if re.fullmatch(r"std::(vector|string|array)<[^>]*>\s*\(\s*\)", s):
        return True
    return False


def _alias_hit(ledger: dict, symbol: str):
    for entry in ledger.values():
        if entry.get("symbol") == symbol:
            return True
    return False


def _check_high_water(repo: pathlib.Path, ledger: dict):
    doc = gc.read_json(repo / LEDGER, LEDGER)
    hw = doc.get("high_water") or {}
    limit = hw.get("max_entries")
    if limit is None:
        raise gc.GateError("LEDGER_HIGH_WATER_MISSING: %s 必须给 high_water.max_entries"
                           "（台账只减不增）" % LEDGER)
    if not isinstance(limit, int) or limit < len(ledger):
        raise gc.GateError("LEDGER_HIGH_WATER_EXCEEDED: entries=%d > max_entries=%s"
                           % (len(ledger), limit))


def _offset_of_line(text: str, line: int) -> int:
    idx = 0
    for _ in range(line - 1):
        nxt = text.find("\n", idx)
        if nxt < 0:
            return len(text)
        idx = nxt + 1
    return idx


GUARD_RE = re.compile(r"if\s*\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*\{")


def _enclosing_guard(text: str, pos: int):
    """调用点之前 800 字符内、最近的一个 if(...) {... 守卫条件。"""
    window = text[max(0, pos - 800):pos]
    last = None
    for m in GUARD_RE.finditer(window):
        last = m.group(1)
    return last


def _compiled_sources(repo: pathlib.Path, build_graph=None):
    """构建图里的真实编译单元（实测优先）：CMake/Ninja build.ninja 的 CXX/C 编译边。

    返回 (basename ∪ relpath 集合, 模式标签)。无构建图时回落 ("", "no-build-graph")，
    此时判据只靠 CMake 源清单与脚本驱动面（仍 fail-closed，不把「没构建图」当绿）。
    """
    cands = [pathlib.Path(build_graph)] if build_graph else [
        repo / "build/build.ninja", repo / "build/linux-control/build.ninja"]
    for path in cands:
        if path and path.is_file():
            text = path.read_text(encoding="utf-8", errors="replace")
            names = set()
            for line in text.splitlines():
                if not line.startswith("build ") or "COMPILER" not in line:
                    continue
                if "CXX_COMPILER" not in line and "C_COMPILER" not in line:
                    continue
                for tok in line.split():
                    if tok.endswith((".cpp", ".cc", ".cxx", ".c", ".cu")):
                        raw = tok.replace("$ ", " ").replace("$:", ":")
                        names.add(raw.split("/")[-1])
                        names.add(raw)
            if names:
                return names, "build.ninja"
    return set(), "no-build-graph"


def _script_blob(repo: pathlib.Path) -> str:
    chunks = []
    # 脚本驱动面含 Makefile 族：Makefile 目标同样是"构建目标"（W4 判的是
    # 「有没有构建路径」，不是「有没有 CMake 目标」）。CI 采集面是否覆盖由
    # eng/tools/quality/check_ctest_registration.py（CI-REG-002）另行判定。
    for pat in ("**/*.py", "**/*.sh", "**/*.ps1", "**/Makefile", "**/makefile",
                "**/GNUmakefile", "**/*.mk"):
        for p in repo.glob(pat):
            if set(p.parts) & {"build", "run", "third_party", ".git", "__pycache__"}:
                continue
            try:
                chunks.append(p.read_text(encoding="utf-8", errors="replace"))
            except OSError:
                continue
    return "\n".join(chunks)


def _cmake_blob(repo: pathlib.Path) -> str:
    chunks = []
    for p in repo.rglob("CMakeLists.txt"):
        parts = set(p.parts)
        if parts & {"build", "run", "third_party", ".git"}:
            continue
        chunks.append(p.read_text(encoding="utf-8", errors="replace"))
    for p in repo.rglob("*.cmake"):
        parts = set(p.parts)
        if parts & {"build", "run", "third_party", ".git"}:
            continue
        chunks.append(p.read_text(encoding="utf-8", errors="replace"))
    if not chunks:
        raise gc.GateError("ANCHOR_MISSING: 仓库内零 CMake 构建文件")
    return "\n".join(chunks)


TEST_EXTS = (".cpp", ".cc", ".cxx", ".c")


def _test_sources(repo: pathlib.Path):
    out = []
    for base_name in ("lib", "eng"):
        base = repo / base_name
        if not base.is_dir():
            continue
        for p in base.rglob("*"):
            if not p.is_file() or p.suffix not in TEST_EXTS:
                continue
            if "tests" not in p.parts and "test" not in p.parts:
                continue
            if set(p.parts) & {"third_party", "archive", "build"}:
                continue
            out.append(p)
    return sorted(out)


# ═══════════════════════════════════════════════════════════════════════════
# 五、自检（--self-test）：正例必绿 + 负例必红 + fail-closed
# ═══════════════════════════════════════════════════════════════════════════
_FIX_MODULE_YAML = """\
id: MOD-fixture
module_id: astrocs.fixture
source_symbols:
  - fix_capability
"""
_FIX_REGISTRY = {
    "registry_schema": "astrocs.module-ports-registry/v2",
    "modules": [{"module_id": "astrocs.fixture", "phase": "phase1",
                 "operations": [{"operation": "op", "entry": "fix_capability",
                                 "ports": [{"name": "p", "code": [
                                     {"file": "lib/fix/mod.cpp", "symbol": "fix_capability",
                                      "token": "x"}]}]}]}],
}
_FIX_DEFAULTS = {
    "defaults_schema": "astrocs.config-defaults/v1",
    "fields": [{"key": "fix.live_key", "value": 1}],
}
_FIX_ENTRY = """\
int cmd_session1_run(int x) {
  fix_capability(x, dark_plane, 1);
  return 0;
}
int cmd_session2_run(int x) { return fix_capability(x, dark_plane, 2); }
int cmd_session3_run(int x) { return fix_capability(x, dark_plane, 3); }
"""
_FIX_MOD = """\
const float* dark_plane = nullptr;
int fix_capability(int x, const float* master_dark, int m) { return x + m; }
int fix_degenerate(int x, const float* master_dark, int m) { return x; }
const char* fix_live_key_user() { return "live_key"; }
const char* fix_out_dir_user() { return "out_dir"; }
"""


def _write_fixture(root: pathlib.Path, *, entry=None, mod=None, defaults=None,
                   ledger=None, test_dir=False, cmake_test_ref=False,
                   registry=None, delete_module_yaml=False, extra_decls=()):
    (root / "lib/fix").mkdir(parents=True, exist_ok=True)
    (root / "lib/cli").mkdir(parents=True, exist_ok=True)
    (root / "lib/include").mkdir(parents=True, exist_ok=True)
    (root / "eng/ci/ledgers").mkdir(parents=True, exist_ok=True)
    (root / "eng/packaging/config").mkdir(parents=True, exist_ok=True)
    (root / "eng/contracts/schemas").mkdir(parents=True, exist_ok=True)
    manifest = _FIX_MODULE_YAML + "".join("  - %s\n" % s for s in extra_decls)
    (root / "lib/fix/module.yaml").write_text(manifest, encoding="utf-8")
    (root / "lib/fix/mod.cpp").write_text(mod if mod is not None else _FIX_MOD, encoding="utf-8")
    (root / "lib/cli/commands.cpp").write_text(entry if entry is not None else _FIX_ENTRY,
                                               encoding="utf-8")
    (root / "lib/include/fixture.h").write_text(
        "AC_API int fix_capability(int, const float*, int);\n", encoding="utf-8")
    (root / "lib/infrastructure/pipeline").mkdir(parents=True, exist_ok=True)
    (root / "lib/infrastructure/pipeline/module_ports.registry.json").write_text(
        json.dumps(registry or _FIX_REGISTRY), encoding="utf-8")
    (root / "eng/packaging/config/defaults.json").write_text(
        json.dumps(defaults or _FIX_DEFAULTS), encoding="utf-8")
    (root / "eng/contracts/schemas/phase_config_fixture.schema.json").write_text(
        json.dumps({"properties": {"config": {"properties": {"out_dir": {"type": "string"}}}}}),
        encoding="utf-8")
    (root / "CMakeLists.txt").write_text(
        "add_executable(acsd lib/cli/commands.cpp lib/fix/mod.cpp)\n", encoding="utf-8")
    if test_dir:
        (root / "lib/fix/tests").mkdir(parents=True, exist_ok=True)
        (root / "lib/fix/tests/orphan_test.cpp").write_text("int main(){return 0;}\n",
                                                            encoding="utf-8")
        (root / "lib/fix/tests/listed_test.cpp").write_text("int main(){return 0;}\n",
                                                            encoding="utf-8")
        if cmake_test_ref:
            (root / "CMakeLists.txt").write_text(
                "add_executable(acsd lib/cli/commands.cpp lib/fix/mod.cpp)\n"
                "add_executable(listed_test lib/fix/tests/listed_test.cpp)\n",
                encoding="utf-8")
    if delete_module_yaml:
        (root / "lib/fix/module.yaml").unlink()
    (root / LEDGER).write_text(json.dumps(
        ledger or {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "prod-wiring",
                   "high_water": {"max_entries": 0}, "entries": []}, ensure_ascii=False),
        encoding="utf-8")


def _selftest() -> int:
    import tempfile

    failures = []

    def case(name, expect_fail, repo):
        try:
            findings, _inv, _extra = evaluate(pathlib.Path(repo))
        except gc.GateError as exc:
            failures.append("%s: unexpected GateError: %s" % (name, exc))
            return
        got = bool(findings)
        if got != expect_fail:
            failures.append("%s: expect_fail=%s got=%s ids=%s"
                            % (name, expect_fail, got, [f["id"] for f in findings][:4]))
        else:
            print("SELFTEST_PASS %s (expect_fail=%s, findings=%d)"
                  % (name, expect_fail, len(findings)))

    def expect_gate_error(name, repo):
        try:
            evaluate(pathlib.Path(repo))
        except gc.GateError as exc:
            print("SELFTEST_PASS %s (fail-closed: %s)" % (name, str(exc)[:60]))
            return
        failures.append("%s: expected GateError" % name)

    with tempfile.TemporaryDirectory() as td:
        base = pathlib.Path(td)
        d_ok = base / "ok"
        _write_fixture(d_ok)
        case("green_wired", False, d_ok)

        # 负例 W1：声明了但生产调用图不可达（fix_dead 有实现、无生产调用点）
        d_dead = base / "dead"
        _write_fixture(d_dead, mod=_FIX_MOD.replace(
            "int fix_degenerate(", "int fix_dead(int x) { return x; }\nint fix_degenerate("),
            entry=_FIX_ENTRY + "int fix_dead_user(int x){ return fix_dead(x); }\n",
            extra_decls=("fix_dead",))
        case("red_declared_unreachable", True, d_dead)

        # 负例 W2：drop 掉调用点中的检测源（构造「有声明、有调用、实参恒 nullptr」）
        deg_entry = ("int cmd_session1_run(int x) {\n"
                     "  fix_degenerate(x, nullptr, 1);\n"
                     "  fix_capability(x, dark_plane, 1);\n"
                     "  return 0;\n}\n"
                     "int cmd_session2_run(int x) { return fix_capability(x, dark_plane, 2); }\n"
                     "int cmd_session3_run(int x) { return fix_capability(x, dark_plane, 3); }\n")
        d_deg = base / "deg"
        _write_fixture(d_deg, entry=deg_entry, extra_decls=("fix_degenerate",))
        case("red_degenerate_arg", True, d_deg)

        # 负例 W3：配置键无消费点（fix.dead_key 不在任何生产源码里）
        d_deadkey = base / "deadkey"
        _write_fixture(d_deadkey, defaults={
            "defaults_schema": "astrocs.config-defaults/v1",
            "fields": [{"key": "fix.never_consumed_key", "value": 1}]})
        case("red_dead_config_key", True, d_deadkey)

        # 负例 W4：tests/ 有测试源但无构建目标
        d_test = base / "testasset"
        _write_fixture(d_test, test_dir=True, cmake_test_ref=False)
        case("red_unbuildable_test", True, d_test)
        d_test_ok = base / "testasset_ok"
        _write_fixture(d_test_ok, test_dir=True, cmake_test_ref=True)
        case("red_only_orphan_test_listed_ok", True, d_test_ok)

        # 负例 W5：能力被「默认关」的开关门住（键声明默认 false）
        w5_entry = ("int cmd_session1_run(int x) {\n"
                    "  if (doc.value(\"fix_gate_key\", false)) {\n"
                    "  fix_capability(x, dark_plane, 1);\n"
                    "  }\n"
                    "  return 0;\n}\n"
                    "int cmd_session2_run(int x) { return fix_capability(x, dark_plane, 2); }\n"
                    "int cmd_session3_run(int x) { return fix_capability(x, dark_plane, 3); }\n")
        d_w5 = base / "switchoff"
        _write_fixture(d_w5, entry=w5_entry, defaults={
            "defaults_schema": "astrocs.config-defaults/v1",
            "fields": [{"key": "fix.live_key", "value": 1},
                       {"key": "fix.fix_gate_key", "value": False}]})
        case("red_switch_default_off", True, d_w5)

        # 负例 W6：dlopen 插件入口的宿主解析函数不可达（跨语言/间接调用面）
        w6_mod = _FIX_MOD + (
            "\nvoid *fix_loader_open(const char *p) { return 0; }\n"
            "void *fix_resolve(void *dl, const char *n) { (void)n; return dl; }\n"
            "int fix_plugin_entry(void *host) { return host ? 1 : 0; }\n"
            "int fix_load_plugin(void *dl) { return fix_resolve(dl, \"fix_plugin_entry\") ? 1 : 0; }\n"
        )
        d_w6 = base / "plugin"
        _write_fixture(d_w6, mod=w6_mod, extra_decls=("fix_plugin_entry",))
        case("red_plugin_entry_unreachable", True, d_w6)

        # 台账豁免：带 reason/owner/exit_condition/authority ⇒ 绿
        # 夹具必须**真的产生**那两条 finding（否则触发 W0 僵尸条目判红）：
        #   一条 W1（fix_dead 有声明无生产调用点）、一条 W2（检测源恒 nullptr）。
        ledger = {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fixture",
                  "high_water": {"max_entries": 2},
                  "entries": [
                      {"id": "unreachable:manifest.source_symbols:fix_dead", "kind": "dormant",
                       "reason": "夹具：设计上由外部工具调用", "owner": "fixture",
                       "exit_condition": "接进生产或撤声明", "authority": "fixture"},
                      {"id": "degenerate_arg:fix_degenerate:1", "kind": "degraded",
                       "reason": "夹具：设计可空形参", "owner": "fixture",
                       "exit_condition": "夹具", "authority": "fixture"},
                  ]}
        d_led = base / "ledgered"
        _write_fixture(d_led,
                       mod=_FIX_MOD.replace("int fix_degenerate(",
                                            "int fix_dead(int x) { return x; }\nint fix_degenerate("),
                       entry=deg_entry + "int fix_dead_user(int x){ return fix_dead(x); }\n",
                       extra_decls=("fix_dead", "fix_degenerate"),
                       ledger=ledger)
        case("green_ledgered", False, d_led)

        # 负例 W0：台账僵尸条目（条目 id 不命中任何判定 ⇒ 判红，强制台账收缩）
        ledger_stale = {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fixture",
                        "high_water": {"max_entries": 1},
                        "entries": [{"id": "unreachable:manifest.source_symbols:fix_nonexistent",
                                     "kind": "dormant", "reason": "夹具：已被修好但没删台账",
                                     "owner": "fixture", "exit_condition": "删除台账条目",
                                     "authority": "fixture"}]}
        d_stale = base / "staleledger"
        _write_fixture(d_stale, ledger=ledger_stale)
        case("red_stale_ledger_entry", True, d_stale)

        # fail-closed：module.yaml 缺失（声明面无输入）
        d_nomanifest = base / "nomanifest"
        _write_fixture(d_nomanifest, delete_module_yaml=True)
        expect_gate_error("crash_no_module_yaml", d_nomanifest)

        # fail-closed：台账缺字段
        d_badled = base / "badledger"
        _write_fixture(d_badled, ledger={"ledger_schema": gc.LEDGER_SCHEMA,
                                         "ledger_id": "x",
                                         "high_water": {"max_entries": 5},
                                         "entries": [{"id": "y", "kind": "z"}]})
        expect_gate_error("crash_bad_ledger", d_badled)

        # fail-closed：台账超只减不增高水位
        d_hw = base / "highwater"
        _write_fixture(d_hw, ledger={"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "x",
                                     "high_water": {"max_entries": 0},
                                     "entries": [{"id": "y", "kind": "z", "reason": "r",
                                                  "owner": "o", "exit_condition": "e"}]})
        expect_gate_error("crash_high_water", d_hw)

        # fail-closed：入口锚缺失（可达性面无根）
        d_noroot = base / "noroot"
        _write_fixture(d_noroot, entry="int not_the_entry() { return 0; }\nint cmd_session2_run(int x){return x;}\nint cmd_session3_run(int x){return x;}\n")
        expect_gate_error("crash_entry_root_missing", d_noroot)

    if failures:
        print("SELFTEST_FAIL:")
        for item in failures:
            print("  " + item)
        return 1
    print("SELFTEST_PASS: all cases match expectation")
    return 0


# ═══════════════════════════════════════════════════════════════════════════
# ══════════════════════════════════════════════════════════════════
# 棘轮基线：门首日即 209 条判红，若直接入 CI 会让整条流水线长期红。
# 基线记录**已冻结的 finding id**；默认运行只对**新增**判红，
# 已冻结项逐轮消解并在读数里报告进度（冻结数只减不增）。
# ══════════════════════════════════════════════════════════════════
BASELINE_DEFAULT = "eng/ci/prod_wiring_baseline.json"


def _load_baseline(path):
    try:
        with open(path, encoding="utf-8") as fh:
            data = json.load(fh)
    except (OSError, json.JSONDecodeError):
        return None
    ids = data.get("finding_ids")
    if not isinstance(ids, list):
        return None
    return set(ids)


def _apply_ratchet(findings, baseline):
    """无基线 ⇒ 全部算新增（fail-closed）；有基线 ⇒ 只判新增。"""
    if baseline is None:
        return findings, [], []
    cur = {f["id"] for f in findings}
    new = [f for f in findings if f["id"] not in baseline]
    stale = sorted(baseline - cur)
    return new, stale, sorted(cur)


def _write_baseline(path, ids, extra):
    payload = {
        "schema_version": "1",
        "purpose": ("CHK-PROD-WIRING 棘轮基线：已冻结的判红 finding id。"
                    "默认运行只对新增判红；冻结项消解后运行 --update-baseline 收紧。"),
        "check_id": CHECK_ID,
        "finding_ids": sorted(ids),
        "finding_count": len(ids),
        "family_counts": extra.get("family_counts"),
    }
    gc.write_json(path, payload)
    print("BASELINE_WRITTEN %s n=%d" % (path, len(ids)))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-PROD-WIRING 能力生产接线门")
    ap.add_argument("--repo", default=str(gc.repo_root()))
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--inventory-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    ap.add_argument("--baseline", default=BASELINE_DEFAULT,
                    help="棘轮基线路径（默认 %s）" % BASELINE_DEFAULT)
    ap.add_argument("--update-baseline", action="store_true", dest="update_baseline")
    args = ap.parse_args(argv)
    if args.self_test:
        return _selftest()
    repo = pathlib.Path(args.repo).resolve()
    try:
        findings, inventory, extra = evaluate(repo)
    except gc.GateError as exc:
        print("%s_CRASH: %s" % (CHECK_ID, exc), file=sys.stderr)
        return 2
    except Exception as exc:  # noqa: BLE001 —— 任何未预期异常都算崩塌（rc=2），不得伪装判红
        print("%s_CRASH: unexpected %s: %s" % (CHECK_ID, type(exc).__name__, exc),
              file=sys.stderr)
        return 2
    if args.inventory_out:
        gc.write_json(args.inventory_out, {"check_id": CHECK_ID, "inventory": inventory,
                                           "summary": extra})
    baseline_path = args.baseline
    if not os.path.isabs(baseline_path):
        baseline_path = str(repo / baseline_path)
    baseline = _load_baseline(baseline_path)

    if args.update_baseline:
        cur_ids = sorted({f["id"] for f in findings})
        if baseline is not None and len(cur_ids) > len(baseline):
            print("BASELINE_REFUSED net_growth current=%d baseline=%d"
                  % (len(cur_ids), len(baseline)))
            return 1
        _write_baseline(baseline_path, cur_ids, extra)
        return 0

    new, stale, cur_ids = _apply_ratchet(findings, baseline)
    extra = dict(extra)
    extra["ratchet"] = {"baseline": baseline_path,
                        "baseline_count": (len(baseline) if baseline is not None else None),
                        "current_count": len(cur_ids),
                        "new_count": len(new),
                        "resolved_since_baseline": len(stale),
                        "family_counts": extra.get("family_counts")}

    if new:
        gc.print_findings(CHECK_ID, [f["id"] + " — " + f["evidence"] for f in new])
        gc.report("FAIL", CHECK_ID, new, extra, args.json_out)
        return 1
    if findings:
        print("%s_PASS(ratchet) new=0 frozen=%d resolved_since_baseline=%d 剩余冻结项仍待消解"
              % (CHECK_ID, len(findings), len(stale)))
        gc.report("PASS", CHECK_ID, [], extra, args.json_out)
        return 0
    print("%s_PASS decls=%d symbols=%d prod_files=%d cfg_keys=%d tests=%d"
          % (CHECK_ID, extra["declaration_count"], extra["declared_symbol_count"],
             extra["production_files"], extra["config_key_count"],
             extra["test_source_count"]))
    gc.report("PASS", CHECK_ID, [], extra, args.json_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
