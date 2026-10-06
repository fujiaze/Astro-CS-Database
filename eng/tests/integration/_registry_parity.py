"""集成层测试的「注册表 ↔ 代码 ↔ 管线 IR」双向一致判定面（纯标准库）。

本模块**不含任何测试函数**，只提供 `test_registry_code_parity.py` 调用的解析器与
判定器。它是本层的工具面，不是判据面。

## 判据来源

主判据正本 = `docs/engineering/contracts/PIPELINE_BLOCK.md`：

- C1 结构、PIPELINE_BLOCK.md:91
- C2 声明⇒实现、PIPELINE_BLOCK.md:92
- C3 实现⇒声明、PIPELINE_BLOCK.md:93
- C3b 载体一致、PIPELINE_BLOCK.md:94
- PC-C4 方向一致、PIPELINE_BLOCK.md:95
- PC-C5 载体合同、PIPELINE_BLOCK.md:96
- PC-C6 非退化、PIPELINE_BLOCK.md:97
- IR-C4 无幻边、PIPELINE_BLOCK.md:112
- IR-C5 序为拓扑序、PIPELINE_BLOCK.md:113
- IR-C5b 声明序一致、PIPELINE_BLOCK.md:114
- IR-C6 psf 在 wcs 之后、PIPELINE_BLOCK.md:115
- IR-C7 非退化、PIPELINE_BLOCK.md:116
- IR-C8 IR 端口 ∈ descriptor、PIPELINE_BLOCK.md:117

派生规则正本 = `eng/tools/quality/gen_block_flow_spec.py`（docstring:4「唯一事实源：
注册表」，:24 三相↔三阶段映射，:30-32 阶段终产物，:50 节点排序键）。

判据纪律 = `docs/engineering/testing/TEST.md` 第 2 节（恒真的比较没有证据资格）、
第 4 节（端口/元数据/选择结果 = 精确一致）、第 5 节（负例必须可红）；
`docs/engineering/testing/VALIDATION_EVIDENCE.md` 第 7 节（零对象守卫两条）、
第 8 节 S1–S7（判别力；S5 规模锚用实算值、禁止无来源魔数、S6 注入点必须是该判定
自身的判定逻辑）、第 12.3 节（fail-closed 与锚存活）、第 12.6 节（判据边界）。

## 事实源（锚点，硬编码路径必须存活）

| 常量 | 仓库相对路径 | 角色 |
|---|---|---|
| `REGISTRY_PATH` | `lib/infrastructure/pipeline/module_ports.registry.json` | 端口事实源（v2 冻结绑定表） |
| `IR_PATH` | `eng/contracts/block_flow/stage_block_flow.json` | 管线 IR（由注册表派生） |
| `ADAPTERS_PATH` | `lib/infrastructure/scheduler/src/module_adapters.cpp` | 20 个 op 入口 + descriptor |
| `SINK_PATH` | `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp` | `write_hips_phase1` 模板入口 |

## 本模块刻意不做的事

1. **不编译、不链接、不起子进程**：全部判定走「读文本 + 自解析」。
2. **不裁决代码**（AGENTS.md §8「测试不是约束代码的门」）：本模块不产生退出码，
   不含 `sys.exit` / `SystemExit` / `os._exit`；判红由 pytest 报项承载。
3. **不做豁免**：解析不到一律 fail-closed（`VALIDATION_EVIDENCE.md` 12.3）。
"""

from __future__ import annotations

import json
import os
import re
import shutil
import tempfile
from typing import Any, Callable, Dict, Iterable, List, Optional, Sequence, Tuple

# ==========================================================================
# 0. 仓库锚点（VALIDATION_EVIDENCE.md 12.3「锚存活」）
# ==========================================================================

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(_HERE)))

REGISTRY_PATH = "lib/infrastructure/pipeline/module_ports.registry.json"
IR_PATH = "eng/contracts/block_flow/stage_block_flow.json"
ADAPTERS_PATH = "lib/infrastructure/scheduler/src/module_adapters.cpp"
SINK_PATH = "lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp"

#: 四个锚点常量 → (常量名, 仓库相对路径)。锚存活检查按这张表逐条走。
ANCHOR_TABLE: Tuple[Tuple[str, str], ...] = (
    ("REGISTRY_PATH", REGISTRY_PATH),
    ("IR_PATH", IR_PATH),
    ("ADAPTERS_PATH", ADAPTERS_PATH),
    ("SINK_PATH", SINK_PATH),
)


class ParityError(Exception):
    """具名失败（fail-closed）。`code` 用于 `pytest.fail` 时的点名。

    约定的 `code` 取值见 `VALIDATION_EVIDENCE.md` 12.3：
    `ANCHOR_STALE`（硬编码路径失效）、`RESOLUTION_FAILED`（符号/JSON 解析不到）。
    """

    def __init__(self, code: str, detail: str) -> None:
        super().__init__("%s: %s" % (code, detail))
        self.code = code
        self.detail = detail


# ==========================================================================
# 1. C++ 源码解析：等长掩码 + 括号配平函数体定位
# ==========================================================================


def mask_cpp(src: str) -> str:
    """把 `src` 的**注释与字符串/字符字面量内容**换成空格，返回**等长**字符串。

    等长是硬要求：定位器的下标必须能直接换算成原文行号。实测两份锚点源码
    （`module_adapters.cpp` 765964 字符 / `astro_sphere_sink.cpp` 30269 字符）
    在掩码后长度不变、换行数不变（16274 / 634）。

    必须正确处理的四类干扰（本文件实���含全部四类，见负例 N4/N5 的注入形态）：

    1. 行注释 `// …`（含含 `//` 的 URL 形态）；
    2. 块注释 `/* … */`，可跨行、**可嵌套注释里的 `*/` 之外的任意花括号**
       —— 花括号必须一并抹掉，否则配平会被注释里的 `{}` 带偏；
    3. 字符串字面量 `"…"`：内容抹掉、**两侧引号保留**，这样字面量扫描仍能定位
       引号区间而内容从原文切出（判定器 G1 文法需要读字面量内容）；
    4. 字符字面量 `'…'`（含转义）。

    转义序列 `\\` 后的字符一并抹掉（掩码里保留长度）；反斜杠后紧跟换行时保留换行。
    """
    out = list(src)
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        # 行注释
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            while i < n and src[i] != "\n":
                out[i] = " "
                i += 1
            continue
        # 块注释（可跨行）
        if c == "/" and i + 1 < n and src[i + 1] == "*":
            out[i] = out[i + 1] = " "
            i += 2
            while i + 1 < n and not (src[i] == "*" and src[i + 1] == "/"):
                if src[i] != "\n":
                    out[i] = " "
                i += 1
            if i < n:
                out[i] = " "
            if i + 1 < n:
                out[i + 1] = " "
            i += 2
            continue
        # 字符串字面量：抹内容、留引号
        if c == '"':
            i += 1
            while i < n:
                if src[i] == "\\":
                    if src[i] != "\n":
                        out[i] = " "
                    if i + 1 < n and src[i + 1] != "\n":
                        out[i + 1] = " "
                    i += 2
                    continue
                if src[i] == '"':
                    i += 1  # 收尾引号保留
                    break
                if src[i] != "\n":
                    out[i] = " "
                i += 1
            continue
        # 字符字面量
        if c == "'":
            i += 1
            while i < n:
                if src[i] == "\\":
                    i += 2
                    continue
                if src[i] == "'":
                    i += 1
                    break
                if src[i] != "\n":
                    out[i] = " "
                i += 1
            continue
        i += 1
    return "".join(out)


def _match_pair(text: str, start: int, opener: str, closer: str) -> int:
    """`text[start] == opener`，返回配对 `closer` 的下标；不配平返回 -1。"""
    depth = 0
    i, n = start, len(text)
    while i < n:
        if text[i] == opener:
            depth += 1
        elif text[i] == closer:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


class FunctionSpan:
    """一个 C++ 函数定义在源码里的区间。"""

    __slots__ = ("symbol", "name_line", "body_start", "body_end", "unit")

    def __init__(self, symbol: str, name_line: int, body_start: int, body_end: int,
                 unit: "CppUnit") -> None:
        self.symbol = symbol
        self.name_line = name_line
        self.body_start = body_start
        self.body_end = body_end
        self.unit = unit

    @property
    def raw_body(self) -> str:
        return self.unit.raw[self.body_start:self.body_end + 1]

    @property
    def masked_body(self) -> str:
        return self.unit.masked[self.body_start:self.body_end + 1]

    def line_of(self, offset_in_body: int) -> int:
        return self.unit.line_of(self.body_start + offset_in_body)


class CppUnit:
    """一份 C++ 源码（原文 + 等长掩码），提供函数定义定位与字面量扫描。"""

    def __init__(self, rel_path: str, raw: str) -> None:
        self.rel_path = rel_path
        self.raw = raw
        self.masked = mask_cpp(raw)
        self._line_index = _build_line_index(raw)

    def line_of(self, offset: int) -> int:
        return self._line_index(offset)

    def find_definitions(self, symbol: str) -> List[FunctionSpan]:
        """找出 `symbol` 在本文件内的**全部函数定义**。

        定义 = 「`symbol` + 形参表 + `{`」。前向声明（`…);`）不算；带构造初始
        列表的（`…): init {`）不算。三类实现在 `module_adapters.cpp` 里都出现：
        普通函数（`p1_op_calibrate`）、模板函数（`astro_sphere_sink.cpp:309` 的
        `template <typename Scalar> bool write_hips_phase1(...)`）、以及同名的显式
        实例化（`astro_sphere_sink.cpp:627/630` 是 `template bool write_hips_phase1<float>(...)`
        形式，收尾是 `;` 不是 `{`，故不误判为定义）。
        """
        spans: List[FunctionSpan] = []
        pattern = r"(?<![A-Za-z0-9_])" + re.escape(symbol) + r"\s*\("
        for m in re.finditer(pattern, self.masked):
            open_paren = self.masked.index("(", m.start())
            close_paren = _match_pair(self.masked, open_paren, "(", ")")
            if close_paren < 0:
                continue
            j = close_paren + 1
            while j < len(self.masked) and self.masked[j] in " \t\r\n":
                j += 1
            if j >= len(self.masked):
                continue
            if self.masked[j] == ":":  # 构造初始化列表 → 不是普通函数定义
                continue
            if self.masked[j] != "{":
                continue  # 前向声明 / 显式实例化
            close_brace = _match_pair(self.masked, j, "{", "}")
            if close_brace < 0:
                continue
            spans.append(FunctionSpan(symbol, self.line_of(m.start()), j, close_brace, self))
        return spans

    def resolve_unique(self, symbol: str) -> FunctionSpan:
        """唯一解析到一个函数体；解析不到或解析出多个 ⇒ fail-closed。"""
        spans = self.find_definitions(symbol)
        if len(spans) == 1:
            return spans[0]
        raise ParityError(
            "RESOLUTION_FAILED",
            "%s: symbol %r 解析出 %d 个函数体（要求恰好 1）"
            % (self.rel_path, symbol, len(spans)),
        )

    def body_string_literals(self, span: FunctionSpan) -> List[Tuple[str, int]]:
        """函数体内全部**完整**字符串字面量：(字面量内容, 相对函数体的起始偏移)。

        扫描在**掩码**上做（注释里的引号已被抹掉，不会被当成字面量），内容从
        **原文**按同一下标切出（掩码等长）。字面量不得跨行、不得含裸换行。
        """
        masked_body = span.masked_body
        raw_body = span.raw_body
        out: List[Tuple[str, int]] = []
        for m in re.finditer(r'"(?:[^"\\\n]|\\.)*"', masked_body):
            out.append((raw_body[m.start() + 1:m.end() - 1], m.start()))
        return out


def _build_line_index(raw: str) -> Callable[[int], int]:
    starts = [0]
    for i, ch in enumerate(raw):
        if ch == "\n":
            starts.append(i + 1)

    def line_of(offset: int) -> int:
        lo, hi = 0, len(starts) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if starts[mid] <= offset:
                lo = mid
            else:
                hi = mid - 1
        return lo + 1

    return line_of


# ==========================================================================
# 2. 数据读取（fail-closed）
# ==========================================================================


def read_text(path: str, code: str = "ANCHOR_STALE") -> str:
    if not os.path.isfile(path):
        raise ParityError(code, "路径不存在: %s" % path)
    with open(path, "r", encoding="utf-8") as fh:
        return fh.read()


def parse_json(text: str, label: str) -> Any:
    try:
        return json.loads(text)
    except (ValueError, TypeError) as exc:
        raise ParityError("RESOLUTION_FAILED", "%s JSON 解析失败: %s" % (label, exc))


# ==========================================================================
# 3. 事实集合：注册表 + IR + 按需加载的 C++ 单元
# ==========================================================================


class Facts:
    """一次判定所需的全部输入面。所有内容在构造时已落到内存，判定器不再读盘。"""

    def __init__(self, registry: Dict[str, Any], ir: Dict[str, Any],
                 units: Dict[str, CppUnit]) -> None:
        self.registry = registry
        self.ir = ir
        self.units = units

    # ---- 注册表访问原语 ----
    @property
    def modules(self) -> List[Dict[str, Any]]:
        return self.registry.get("modules") or []

    def operations(self) -> List[Tuple[Dict[str, Any], Dict[str, Any]]]:
        """(module, operation) 对，顺序 = `modules` 数组的出现序 × `operations` 序。"""
        out = []
        for m in self.modules:
            for op in m.get("operations") or []:
                out.append((m, op))
        return out

    def ports(self) -> List[Tuple[Dict[str, Any], Dict[str, Any], Dict[str, Any]]]:
        """(module, operation, port) 三元组，顺序同上。"""
        return [(m, op, p) for (m, op) in self.operations() for p in (op.get("ports") or [])]

    def anchors(self) -> List[Tuple[Dict[str, Any], Dict[str, Any], Dict[str, Any], Dict[str, Any]]]:
        """(module, port, code_anchor) 三元组。"""
        return [(m, p, c) for (m, _op, p) in self.ports() for c in (p.get("code") or [])]

    def declared_tokens(self, module_id: str) -> set:
        """某 module 已声明的代码 token 全集（其所有端口的 `code[].token`）。"""
        out = set()
        for m in self.modules:
            if m.get("module_id") != module_id:
                continue
            for op in m.get("operations") or []:
                for p in op.get("ports") or []:
                    for c in p.get("code") or []:
                        out.add(c.get("token"))
        return out

    # ---- 端口图（全局，跨 phase） ----
    def port_direction(self) -> Dict[Tuple[str, str], str]:
        return {(m["module_id"], p["name"]): p["direction"] for (m, _o, p) in self.ports()}

    def port_carrier(self) -> Dict[Tuple[str, str], str]:
        return {(m["module_id"], p["name"]): p["carrier"] for (m, _o, p) in self.ports()}

    def port_phase(self) -> Dict[Tuple[str, str], str]:
        return {(m["module_id"], p["name"]): m["phase"] for (m, _o, p) in self.ports()}

    def producers(self) -> Dict[str, List[str]]:
        out: Dict[str, List[str]] = {}
        for (m, _o, p) in self.ports():
            if p["direction"] == "output":
                out.setdefault(p["name"], []).append(m["module_id"])
        return out

    def consumers(self) -> Dict[str, List[str]]:
        out: Dict[str, List[str]] = {}
        for (m, _o, p) in self.ports():
            if p["direction"] == "input":
                out.setdefault(p["name"], []).append(m["module_id"])
        return out

    def ir_nodes(self) -> List[Dict[str, Any]]:
        return self.ir.get("nodes") or []

    def ir_stage_of(self) -> Dict[str, str]:
        return {n["module_id"]: n["stage"] for n in self.ir_nodes()}

    def ir_pos_of(self) -> Dict[str, int]:
        return {n["module_id"]: i for i, n in enumerate(self.ir_nodes())}

    def ir_reads(self) -> Dict[str, set]:
        return {n["module_id"]: set(n.get("reads") or []) for n in self.ir_nodes()}

    def registry_edges(self) -> List[Tuple[str, str, str]]:
        """注册表端口图的「生产→消费」边：(产物身份, 生产模块, 消费模块)。"""
        prod = self.producers()
        cons = self.consumers()
        edges: List[Tuple[str, str, str]] = []
        for artifact in sorted(set(prod) & set(cons)):
            for pm in prod[artifact]:
                for cm in cons[artifact]:
                    edges.append((artifact, pm, cm))
        return edges


# ==========================================================================
# 4. 违例载体
# ==========================================================================


class Violation:
    """一条判红证据（可归因到被核对对象本身）。"""

    __slots__ = ("criterion", "subject", "detail")

    def __init__(self, criterion: str, subject: str, detail: str) -> None:
        self.criterion = criterion
        self.subject = subject
        self.detail = detail

    def __repr__(self) -> str:  # pragma: no cover - 仅用于失败信息排版
        return "[%s] %s :: %s" % (self.criterion, self.subject, self.detail)


def subjects(violations: Iterable[Violation]) -> set:
    return {v.subject for v in violations}


def render(violations: Sequence[Violation], limit: int = 10) -> str:
    """按 `VALIDATION_EVIDENCE.md` 10 节「判红输出逐项给检查名、对象计数与前 10 条」排版。"""
    lines = ["n_violations=%d" % len(violations)]
    for v in violations[:limit]:
        lines.append("  %s | %s | %s" % (v.criterion, v.subject, v.detail))
    if len(violations) > limit:
        lines.append("  … 另有 %d 条" % (len(violations) - limit))
    return "\n".join(lines)


# ==========================================================================
# 5. 判定器 —— pipeline-carrier 侧（C1 / C2 / C3 / C3b / PC-C4..C6）
# ==========================================================================

#: C1 允许的 `direction` 取值集合（封闭枚举）。
#: 来源：PIPELINE_BLOCK.md:91 要求 `direction` 齐全；可取值的封闭面见
#: `gen_block_flow_spec.py:40-41`（只区分 `== "input"` 与 `== "output"` 两类）
#: 与 `eng/contracts/block_flow/stage_block_flow.json` 派生只用这两值。
ALLOWED_DIRECTIONS = frozenset({"input", "output"})

#: C1 / PC-C5 允许的 `carrier` 取值集合（**逐字**取自 PIPELINE_BLOCK.md:21）。
#: 「端口到载体的对应由注册表的 `carrier` 字段声明，取值 `output_dir_file` /
#: `hips_product_tree` / `config_path`」
ALLOWED_CARRIERS = frozenset({"output_dir_file", "hips_product_tree", "config_path"})

#: PC-C5：节点间端口允许的载体（逐字取自 PIPELINE_BLOCK.md:96）。
INTER_NODE_CARRIERS = frozenset({"output_dir_file", "hips_product_tree"})

#: C1 要求每个端口齐全的字段（逐字取自 PIPELINE_BLOCK.md:91）。
REQUIRED_PORT_FIELDS = ("name", "direction", "carrier", "artifacts", "code")

#: 注册表 v2 的 schema 常量（取自 `module_ports.registry.json:2`，并由
#: PIPELINE_BLOCK.md:9「端口事实源：…（v2，冻结绑定表）」冻结）。
REGISTRY_SCHEMA = "acsd.module-ports-registry/v2"
REGISTRY_VERSION = 2

#: 管线 IR 的 schema 常量（取自 `stage_block_flow.json:2`）。
IR_SCHEMA = "acsd.stage-block-flow/v1"


def judge_c1_structure(facts: Facts) -> List[Violation]:
    """C1 结构：注册表 v2 + `carrier_contract`；每 module 恰一个 operation；
    端口 `name/direction/carrier/artifacts/code` 齐全；`direction` 与 `carrier`
    取值分别落在封闭枚举内。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:91`。
    """
    out: List[Violation] = []
    reg = facts.registry
    if reg.get("registry_schema") != REGISTRY_SCHEMA:
        out.append(Violation("C1", "registry_schema",
                             "期望 %r，实际 %r" % (REGISTRY_SCHEMA, reg.get("registry_schema"))))
    if reg.get("registry_version") != REGISTRY_VERSION:
        out.append(Violation("C1", "registry_version",
                             "期望 %r，实际 %r" % (REGISTRY_VERSION, reg.get("registry_version"))))
    cc = reg.get("carrier_contract")
    if not isinstance(cc, dict) or not cc.get("carriers"):
        out.append(Violation("C1", "carrier_contract", "缺失或 carriers 为空"))
    elif not ALLOWED_CARRIERS.issubset(set(cc["carriers"])):
        out.append(Violation("C1", "carrier_contract.carriers",
                             "未覆盖 %s" % sorted(ALLOWED_CARRIERS - set(cc["carriers"]))))

    modules = reg.get("modules")
    if not isinstance(modules, list):
        out.append(Violation("C1", "modules", "modules 不是数组"))
        return out

    for m in modules:
        mid = m.get("module_id", "<no module_id>")
        ops = m.get("operations")
        if not isinstance(ops, list) or len(ops) != 1:
            out.append(Violation("C1", mid, "每 module 恰一个 operation，实际 %r"
                                 % (len(ops) if isinstance(ops, list) else ops)))
            continue
        for p in ops[0].get("ports") or []:
            subject = "%s:%s" % (mid, p.get("name", "<no name>"))
            for field in REQUIRED_PORT_FIELDS:
                if field not in p:
                    out.append(Violation("C1", subject, "缺字段 %s" % field))
            if p.get("direction") not in ALLOWED_DIRECTIONS:
                out.append(Violation("C1", subject, "direction 越出封闭枚举: %r"
                                     % p.get("direction")))
            if p.get("carrier") not in ALLOWED_CARRIERS:
                out.append(Violation("C1", subject, "carrier 越出枚举 %s: %r"
                                     % (sorted(ALLOWED_CARRIERS), p.get("carrier"))))
    return out


def judge_c2_anchor_resolves(facts: Facts) -> List[Violation]:
    """C2 声明⇒实现：每条端口 `code` 锚点必须解析——文件存在、`symbol` 在本文件内
    **唯一**解析到一个函数体、`token` 落在该函数体内。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:92`。
    解析失败按 `VALIDATION_EVIDENCE.md:412-413` fail-closed，不按「无违规」处理。
    """
    out: List[Violation] = []
    for (m, p, c) in facts.anchors():
        subject = "%s:%s:%s" % (m["module_id"], p.get("name"), c.get("token"))
        rel = c.get("file")
        unit = facts.units.get(rel)
        if unit is None:
            out.append(Violation("C2/FILE_MISSING", subject, "锚文件不在事实面: %r" % rel))
            continue
        symbol = c.get("symbol")
        spans = unit.find_definitions(symbol)
        if len(spans) != 1:
            out.append(Violation("C2/SYMBOL_UNRESOLVED", subject,
                                 "%s: %r 解析出 %d 个函数体（要求 1）"
                                 % (rel, symbol, len(spans))))
            continue
        span = spans[0]
        token = c.get("token")
        occurrences = classify_occurrences(unit, span, token)
        good = [(off, kind) for (off, kind) in occurrences if kind in ("code", "string")]
        if not good:
            kinds = sorted({k for _o, k in occurrences}) or ["<无命中>"]
            out.append(Violation("C2/ANCHOR_STALE", subject,
                                 "token %r 在 %s:%s 的函数体内无「代码或字符串」形态的命中"
                                 "（函数体 %d..%d 行，实际形态 %s）"
                                 % (token, rel, symbol, span.name_line,
                                    unit.line_of(span.body_end), kinds)))
    return out


def classify_occurrences(unit: CppUnit, span: FunctionSpan, token: str
                         ) -> List[Tuple[int, str]]:
    """token 在函数体内的每一处命中，按形态分类：`code` / `string` / `comment`。

    形态判定做法：先在**掩码**上扫出全部字符串字面量区间（含两侧引号），再在**原文**
    上把剩余的非空白、非注释片段视作 `code`；既不在字面量区间内、又只出现在注释里的
    命中记作 `comment`（用掩码「该区间是否已被抹空」区分）。

    C2 只要求「`token` 落在该函数体内」（`PIPELINE_BLOCK.md:92`）。本函数额外区分形态，
    是为了让负例 N4（删 token）/ N5（把 token 移出符号）能分别落到「彻底无命中」与
    「命中但形态不可用」两种可归因的读数上。
    """
    if not token:
        return []
    raw_body = span.raw_body
    masked_body = span.masked_body
    lit_ranges: List[Tuple[int, int]] = []
    for m in re.finditer(r'"(?:[^"\\\n]|\\.)*"', masked_body):
        lit_ranges.append((m.start(), m.end()))

    out: List[Tuple[int, str]] = []
    for m in re.finditer(re.escape(token), raw_body):
        off = m.start()
        inside_literal = any(s <= off < e for (s, e) in lit_ranges)
        if inside_literal:
            out.append((off, "string"))
            continue
        window = masked_body[off:off + len(token)]
        if window.strip() == "":
            out.append((off, "comment"))
        else:
            out.append((off, "code"))
    return out


# --------------------------------------------------------------------------
# C3 的代码侧闭合文法
# --------------------------------------------------------------------------

#: G1「产物文件名后缀字面量」：字面量内容全文匹配 `/<名字>.<json|bin|fits>`。
#: 取值理由逐条来自 PIPELINE_BLOCK.md:19「注册表里每个端口对应 `output_dir` 下的一个
#: 具体产物（文件或产品目录）」——端口产物就是 `output_dir` 下的文件名，后缀字面量
#: 是它在代码里唯一的、非注册表派生的可判定形态。
G1_PRODUCT_LITERAL = re.compile(r"^/[A-Za-z0-9_][A-Za-z0-9_.\-]*\.(?:json|bin|fits)$")


def extract_g1_tokens(unit: CppUnit, span: FunctionSpan) -> List[str]:
    """G1 文法：抽出该函数体触碰的**产物文件名后缀字面量**。

    本文法**与注册表无关**（判定「抽什么」时完全不查注册表；注册表只在随后用于
    「是否已声明」的比对）。抽取分三步：

    1. 在**掩码**函数体上扫 `"…"` 字面量区间（注释里的引号已被抹掉，不误抽）；
    2. 内容从**原文**同下标切出（掩码等长）；
    3. `G1_PRODUCT_LITERAL.fullmatch(内容)` 命中即收。

    实测抽取规模（HEAD bf944fad，20 个 module、21 个不同 `symbol`、共 **77** 个 G1 token；下表逐行相加 = 77）：

    | module | G1 token 数 | 未声明 |
    |---|---|---|
    | acsd.phase1.calibration | 1 | `/master_refs.json` |
    | acsd.phase1.cosmetic | 2 | `/master_refs.json`、`/badcol_report.json` |
    | acsd.phase1.wcs-platesolve | 1 | — |
    | acsd.phase1.star-psf | 3 | — |
    | acsd.phase1.photometry | 5 | — |
    | acsd.phase1.noise-snr | 3 | — |
    | acsd.phase1.drizzle | 4(+1 于 `write_hips_phase1`) | — |
    | acsd.phase1.writer | 4 | — |
    | acsd.phase2.coverage | 1 | — |
    | acsd.phase2.sample | 2 | — |
    | acsd.phase2.upm-fit | 5 | — |
    | acsd.phase2.upm-apply | 6 | — |
    | acsd.phase2.reject | 6 | — |
    | acsd.phase2.integrate | 8 | — |
    | acsd.phase2.write | 7 | — |
    | acsd.phase3.properties | 1 | — |
    | acsd.phase3.wcs | 2 | — |
    | acsd.phase3.resample2 | 4 | — |
    | acsd.phase3.writer | 6 | — |
    | acsd.phase3.verify | 5 | — |

    **已否证的文法**（实测后不用，理由记在各自 docstring / 交付报告里）：
    G3 `(?<![\\w])([a-z][a-z0-9]*(?:_[a-z0-9]+)*_(?:path|dir|file))(?![\\w])`
    抽 171 个、161 个未声明——抽到的是**函数内局部变量**（`out_dir`、`frame_path`、
    `cor_path` …），不是端口身份；G4 同族 `*_hash` 抽的是 manifest 内部字段
    （`model_hash`、`geometry_hash` …），也不是端口身份。两者都不满足「产物 token」。
    """
    out = []
    for content, _off in unit.body_string_literals(span):
        if G1_PRODUCT_LITERAL.fullmatch(content):
            out.append(content)
    return sorted(set(out))


def judge_c3_implementation_to_declaration(facts: Facts) -> List[Violation]:
    """C3 实现⇒声明：以代码侧闭合文法（G1）抽出每个节点函数触碰的产物 token，
    断言全部已在注册表声明。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:93`、:75
    「代码中真实存在的数据流未在注册表声明 ⇒ 判红」。
    节点函数 = 该 module 各端口 `code[].symbol` 指向的函数体（与 C2 同一解析面）。
    """
    out: List[Violation] = []
    for m in facts.modules:
        mid = m.get("module_id")
        declared = facts.declared_tokens(mid)
        sym_files: Dict[str, str] = {}
        for op in m.get("operations") or []:
            for p in op.get("ports") or []:
                for c in p.get("code") or []:
                    sym_files.setdefault(c["symbol"], c["file"])
        for symbol, rel in sorted(sym_files.items()):
            unit = facts.units.get(rel)
            if unit is None:
                raise ParityError("RESOLUTION_FAILED",
                                  "C3 无法取到锚文件 %s（module %s）" % (rel, mid))
            spans = unit.find_definitions(symbol)
            if len(spans) != 1:
                raise ParityError("RESOLUTION_FAILED",
                                  "C3 前置：%s 中 %r 解析出 %d 个函数体"
                                  % (rel, symbol, len(spans)))
            for token in extract_g1_tokens(unit, spans[0]):
                if token not in declared:
                    out.append(Violation("C3/UNDECLARED_FLOW", "%s@%s" % (mid, symbol),
                                         "代码侧抽到产物 token %r，注册表未声明" % token))
    return out


#: C3b 的代码侧触发集：**命名约定闭合**，不从注册表取。
#: 生产者角色 = 调用 `aio_hips_product_begin` / `aio_hips_write_*` / `aio_hips_finalize`；
#: 消费者角色 = 调用 `aio_hips_read_*`。两条都只按 `aio_hips_` 这个代码侧命名空间
#: 的前缀判定，规则本身对注册表一无所知。
HIPS_PRODUCER_CALL = re.compile(
    r"(?<![A-Za-z0-9_:])(?:aio_hips_product_begin|aio_hips_write_[a-z0-9_]+"
    r"|aio_hips_finalize)\s*\(")
HIPS_CONSUMER_CALL = re.compile(r"(?<![A-Za-z0-9_:])aio_hips_read_[a-z0-9_]+\s*\(")


def c3b_role_triggers(facts: Facts) -> Tuple[List[str], List[str]]:
    """用 C3b 判定器**自己**的正则在**自己定位的**函数体区间上复算角色触发面。

    返回 `(producers, consumers)` 两个 module_id 列表。
    这是 `judge_c3b_carrier_consistency` 的取证面，供「零对象守卫」与证据复核用；
    它与判定器共用同一段逻辑（`find_definitions` + `masked` 区间 + 两条正则），
    因此不会与判定结果漂移。**必须走函数体区间**：整文件拼接会得到 20 个 module
    （每个 op 都在同一文件里），是错误的计数口径。
    """
    producers: List[str] = []
    consumers: List[str] = []
    for m in facts.modules:
        mid = m["module_id"]
        sym_files: Dict[str, str] = {}
        for op in m.get("operations") or []:
            for p in op.get("ports") or []:
                for c in p.get("code") or []:
                    sym_files.setdefault(c["symbol"], c["file"])
        body = ""
        for symbol, rel in sorted(sym_files.items()):
            unit = facts.units.get(rel)
            if unit is None:
                raise ParityError("RESOLUTION_FAILED",
                                  "C3b 无法取到锚文件 %s（module %s）" % (rel, mid))
            span = unit.resolve_unique(symbol)
            body += unit.masked[span.body_start:span.body_end + 1]
        if HIPS_PRODUCER_CALL.search(body):
            producers.append(mid)
        if HIPS_CONSUMER_CALL.search(body):
            consumers.append(mid)
    return producers, consumers


def judge_c3b_carrier_consistency(facts: Facts) -> List[Violation]:
    """C3b 载体一致：节点触碰 HiPS 产品树 ⇒ 必须有对应 `carrier=hips_product_tree`
    且方向一致的端口。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:94`、:77。

    「触碰」的判定是代码侧命名约定闭合的（见 `HIPS_PRODUCER_CALL` /
    `HIPS_CONSUMER_CALL`）。实测触发规模（HEAD bf944fad；用本判定器自己的
    `find_definitions` + `masked` 函数体区间复算，**不是**整文件拼接）：

    | 角色 | 计数 | module |
    |---|---|---|
    | 生产者角色 | **2** | `acsd.phase1.drizzle`、`acsd.phase2.write` |
    | 消费者角色 | **4** | `acsd.phase1.writer`、`acsd.phase2.upm-apply`、`acsd.phase2.reject`、`acsd.phase2.integrate` |
    | 触发 module 去重 | **6** | 上两集合的并 |

    角色判定的**最小反例**（防止把「有任意 `aio_hips_*` 调用」误当「生产者角色」）：
    `acsd.phase1.writer` 的 `p1_op_writer` 函数体里有 6 个 `aio_hips_*` 调用
    （`aio_hips_open` / `aio_hips_close` / `aio_hips_read_tile_f32` /
    `aio_hips_tile_count` / `aio_hips_tile_ipix` / `aio_hips_reader_last_error`），
    但 `HIPS_PRODUCER_CALL.search(body)` = **False**、`HIPS_CONSUMER_CALL.search(body)` =
    **True**；对称地，`acsd.phase1.drizzle` 的 `p1_op_drizzle` + `write_hips_phase1`
    函数体里 `HIPS_PRODUCER_CALL.search` = **True**、`HIPS_CONSUMER_CALL.search` =
    **False**。两个角色集合**不重叠**，并集 6；把并集 6 报成「生产 6 / 消费 6」是
    计数错误（两侧各把对方的成员填了进来）。
    """
    out: List[Violation] = []
    for m in facts.modules:
        mid = m.get("module_id")
        sym_files: Dict[str, str] = {}
        for op in m.get("operations") or []:
            for p in op.get("ports") or []:
                for c in p.get("code") or []:
                    sym_files.setdefault(c["symbol"], c["file"])
        body = ""
        for symbol, rel in sorted(sym_files.items()):
            unit = facts.units.get(rel)
            if unit is None:
                raise ParityError("RESOLUTION_FAILED",
                                  "C3b 无法取到锚文件 %s（module %s）" % (rel, mid))
            spans = unit.find_definitions(symbol)
            if len(spans) != 1:
                raise ParityError("RESOLUTION_FAILED",
                                  "C3b 前置：%s 中 %r 解析出 %d 个函数体"
                                  % (rel, symbol, len(spans)))
            body += unit.masked[spans[0].body_start:spans[0].body_end + 1]

        hips_ports = [(p["name"], p["direction"]) for op in (m.get("operations") or [])
                      for p in (op.get("ports") or [])
                      if p.get("carrier") == "hips_product_tree"]
        if HIPS_PRODUCER_CALL.search(body) or HIPS_CONSUMER_CALL.search(body):
            if not hips_ports:
                out.append(Violation("C3B/NO_HIPS_PORT", mid,
                                     "代码侧触碰 HiPS 产品树，注册表无 carrier=hips_product_tree 端口"))
        if HIPS_PRODUCER_CALL.search(body):
            if not any(d == "output" for _n, d in hips_ports):
                out.append(Violation("C3B/DIRECTION", mid,
                                     "代码侧为 HiPS 生产者，但无 direction=output 的 hips 端口"))
        if HIPS_CONSUMER_CALL.search(body):
            if not any(d == "input" for _n, d in hips_ports):
                out.append(Violation("C3B/DIRECTION", mid,
                                     "代码侧为 HiPS 消费者，但无 direction=input 的 hips 端口"))
    return out


def judge_pc_c4_direction(facts: Facts) -> List[Violation]:
    """PC-C4 方向一致 —— **只落注册表侧可判的三条**，代码侧角色推断如实登记为不可靠。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:95`；三条子句的取值来源：

    - (a) `carrier=config_path` ⇒ `direction=input`。依据 PIPELINE_BLOCK.md:21 逐字：
      「`config_path`（阶段外部输入，本阶段无生产者）」——无生产者者只能是输入。
    - (b) 同一 module 内端口 `name` 唯一。依据 PIPELINE_BLOCK.md:30「块名（阶段内唯一，
      小写蛇形）」+ :40-44 生命周期 DAG 校验第 1 条「每个被消费的块有且仅有一个生产者」。
    - (c) 同一产物身份在同一 phase 内不得被两个 module 同时生产。依据 PIPELINE_BLOCK.md:42
      第 3 条「同一块被重复生产 ⇒ 非法」。

    **代码侧角色推断未实现**（如实登记，见交付报告）：PIPELINE_BLOCK.md:95 要求
    「变量流分析推断出的读写角色必须包含声明的 `direction`」。在本仓的 101 条锚点上，
    多数 token 出现在 helper 调用的实参位（例如 `p1_read_image(doc["master_bias"]…)`），
    判定读写需要覆盖 `aio_fs::read_all` / `std::fopen(..., "wb")` / 落盘 helper /
    跨函数传参的完整数据流；只做局部判定会给出与真实角色相反的结论。故本条不写
    「假装能判红」的测试，改由 (a)(b)(c) 三条承担，负例 N3 注入 (a)。
    """
    out: List[Violation] = []
    seen: Dict[str, set] = {}
    for (m, _op, p) in facts.ports():
        mid = m["module_id"]
        name = p["name"]
        if p.get("carrier") == "config_path" and p.get("direction") != "input":
            out.append(Violation("PC-C4a/CONFIG_PATH_ROLE", "%s:%s" % (mid, name),
                                 "carrier=config_path 必须是 input（阶段外部输入、本阶段无生产者），"
                                 "实际 %r" % p.get("direction")))
        seen.setdefault(mid, set())
        if name in seen[mid]:
            out.append(Violation("PC-C4b/DUPLICATE_PORT_NAME", "%s:%s" % (mid, name),
                                 "同一 module 内端口名重复（阶段内块名唯一）"))
        seen[mid].add(name)

    by_phase: Dict[Tuple[str, str], List[str]] = {}
    for (m, _op, p) in facts.ports():
        if p["direction"] == "output":
            by_phase.setdefault((m["phase"], p["name"]), []).append(m["module_id"])
    for (phase, name), mods in sorted(by_phase.items()):
        if len(mods) > 1:
            out.append(Violation("PC-C4c/DUPLICATE_PRODUCER", "%s:%s" % (phase, name),
                                 "同阶段产物身份被 %d 个 module 同时生产: %s" % (len(mods), mods)))
    return out


def judge_pc_c5_carrier_contract(facts: Facts) -> List[Violation]:
    """PC-C5 载体合同，四条子句逐字对应 PIPELINE_BLOCK.md:96 与 :21：

    1. 节点间端口 `carrier` ∈ {`output_dir_file`, `hips_product_tree`}；
    2. `output_dir` 产物身份限本阶段（不得跨阶段）；
    3. `carrier_contract` 必须显式声明 HiPS 产品树为跨阶段载体；
    4. （子句 1 的载体枚举推论）`direction=input` 且 `carrier ∈ {output_dir_file,
       hips_product_tree}` 的端口，其产物身份必须在本注册表内有生产者——否则它就是
       PIPELINE_BLOCK.md:21 逐字定义的「阶段外部输入（本阶段无生产者）」，而那一档的
       载体枚举值是 `config_path`，不是 `output_dir_file` / `hips_product_tree`。

    子句 4 的实测命中（HEAD bf944fad）：**恰好 1 条**
    `acsd.phase1.photometry:p1_photscale`（`carrier=output_dir_file`、
    `direction=input`、全注册表无任何生产者）。代码侧它是可选外部标定通道，
    `lib/infrastructure/scheduler/src/module_adapters.cpp:6071`
    读 `out_dir + "/p1_photscale.json"`；派生 IR 也记成外部输入
    （`eng/contracts/block_flow/stage_block_flow.json:411-417`：
    `"lifecycle": "EXTERNAL_IN"`、`"produced_by": []`）。
    同形态但**合规**的对照：`lights` / `master_bias` / `master_dark` / `master_flat`
    / `run_context` 五条都是 `carrier=config_path` + `direction=input`。
    两种读法并列（**不替负责人裁定**，见交付报告）：
    (a) 真漏声明——`p1_photscale.json` 应由某个 phase1 节点生产却没进注册表；
    (b) 正本口径缺失——若「可选外部标定通道」本就不该进端口面，判红的是
        `carrier` 的取值选择（应为 `config_path`）而不是「漏声明」。
    """
    out: List[Violation] = []
    prod = facts.producers()
    cons = facts.consumers()
    carrier = facts.port_carrier()
    phase = facts.port_phase()
    modules_of = {m["module_id"]: m for m in facts.modules}

    # 「节点间端口」的定义：该端口的产物身份**在本注册表内至少有一个生产者**。
    # 只有这类产物身份才真的在节点之间流转。`config_path` 按 `PIPELINE_BLOCK.md:21`
    # 逐字就是「阶段外部输入，本阶段无生产者」，因此天然不在本子句的检查面内
    # （否则会把 5 条合法的 config_path 外部输入误判成违例）。
    for (m, _op, p) in facts.ports():
        name = p["name"]
        mid = m["module_id"]
        if name in prod:
            if p.get("carrier") not in INTER_NODE_CARRIERS:
                out.append(Violation("PC-C5/INTER_NODE_CARRIER", "%s:%s" % (mid, name),
                                     "节点间端口（有生产者 %s）carrier=%r，不在 %s"
                                     % (prod[name], p.get("carrier"),
                                        sorted(INTER_NODE_CARRIERS))))

    for artifact in sorted(set(prod) & set(cons)):
        p_phases = set(phase[(mid, artifact)] for mid in prod[artifact])
        c_phases = set(phase[(mid, artifact)] for mid in cons[artifact])
        for mid in prod[artifact]:
            if carrier[(mid, artifact)] != "output_dir_file":
                continue
            for cm in cons[artifact]:
                if phase[(cm, artifact)] not in p_phases:
                    out.append(Violation("PC-C5/CROSS_STAGE_OUTPUT_DIR", artifact,
                                         "output_dir 产物身份跨阶段：生产 %s(%s) → 消费 %s(%s)"
                                         % (mid, ",".join(sorted(p_phases)), cm,
                                            ",".join(sorted(c_phases)))))

    cc = facts.registry.get("carrier_contract")
    if not isinstance(cc, dict) or "hips_product_tree" not in (cc.get("carriers") or {}):
        out.append(Violation("PC-C5/NO_HIPS_CARRIER_DECL", "carrier_contract",
                             "carrier_contract 未显式声明 hips_product_tree 为跨阶段载体"))
    else:
        statement = cc.get("statement") or ""
        if "HiPS" not in statement:
            out.append(Violation("PC-C5/NO_HIPS_CARRIER_DECL", "carrier_contract.statement",
                                 "statement 未显式点名 HiPS 产品树为跨阶段唯一载体"))

    # 子句 4：非 config_path 载体的输入端口必须有生产者
    prod_all = facts.producers()
    for (m, _op, p) in facts.ports():
        if p.get("direction") != "input":
            continue
        if p.get("carrier") in INTER_NODE_CARRIERS and p["name"] not in prod_all:
            out.append(Violation("PC-C5/EXTERNAL_INPUT_CARRIER",
                                 "%s:%s" % (m["module_id"], p["name"]),
                                 "carrier=%r + direction=input 且全注册表无生产者；"
                                 "PIPELINE_BLOCK.md:21 把「阶段外部输入（本阶段无生产者）」"
                                 "的载体枚举值定为 config_path"
                                 % p.get("carrier")))
    return out


def judge_pc_c6_non_degenerate(facts: Facts) -> List[Violation]:
    """PC-C6 非退化。**每个下界的来源都写在 `violation.detail` 里**，无魔数
    （`VALIDATION_EVIDENCE.md:192` S5「规模锚用实算值…禁止任何无来源的魔数阈值」）。

    下界与其来源：

    | 下界 | 取值 | 来源 |
    |---|---|---|
    | module 数 | ≥ 3 | `gen_block_flow_spec.py:24` 的三相↔三阶段映射（normalize/mosaic/export）+ PIPELINE_BLOCK.md:6「`normalize` 产出…`mosaic` 产出…`export` 只读…」三个命令各占一个阶段 ⇒ 每阶段至少一个节点 |
    | operation 数 | = module 数 | PIPELINE_BLOCK.md:91「每 module 恰一个 operation」 |
    | 端口数 | ≥ 2 × module 数 | PIPELINE_BLOCK.md:13「模块从帧读入参块、产出新块写回」⇒ 每 module 至少 1 输入 + 1 输出 |
    | 锚点数 | ≥ 端口数 | PIPELINE_BLOCK.md:91「端口 `name/direction/carrier/artifacts/code` 齐全」⇒ 每端口至少 1 条 `code` 锚点 |
    | 生产→消费边数 | 每阶段 ≥ 1 | `VALIDATION_EVIDENCE.md:170-173` 零对象守卫第 1 条「声明非空且实算对象数 > 0」，逐阶段施加 |
    | 空注册表 | 判红 | PIPELINE_BLOCK.md:97「注册表为空…⇒ 判红」 |
    """
    out: List[Violation] = []
    modules = facts.modules
    n_mod = len(modules)
    if n_mod == 0:
        out.append(Violation("PC-C6/EMPTY_REGISTRY", "modules",
                             "注册表为空（PIPELINE_BLOCK.md:97 明列「注册表为空 ⇒ 判红」）"))
        return out
    n_ops = sum(len(m.get("operations") or []) for m in modules)
    n_ports = sum(len(op.get("ports") or []) for m in modules for op in (m.get("operations") or []))
    n_anchors = sum(len(p.get("code") or []) for m in modules
                    for op in (m.get("operations") or []) for p in (op.get("ports") or []))

    if n_mod < 3:
        out.append(Violation("PC-C6/MODULE_FLOOR", "modules",
                             "module 数 %d < 3（来源：gen_block_flow_spec.py:24 三相映射 "
                             "+ PIPELINE_BLOCK.md:6 三命令各占一阶段）" % n_mod))
    if n_ops != n_mod:
        out.append(Violation("PC-C6/OP_FLOOR", "operations",
                             "operation 数 %d ≠ module 数 %d（来源：PIPELINE_BLOCK.md:91 "
                             "「每 module 恰一个 operation」）" % (n_ops, n_mod)))
    if n_ports < 2 * n_mod:
        out.append(Violation("PC-C6/PORT_FLOOR", "ports",
                             "端口数 %d < 2×%d（来源：PIPELINE_BLOCK.md:13「模块从帧读入参块、"
                             "产出新块写回」⇒ 每 module 至少 1 输入 1 输出）"
                             % (n_ports, n_mod)))
    if n_anchors < n_ports:
        out.append(Violation("PC-C6/ANCHOR_FLOOR", "code",
                             "锚点数 %d < 端口数 %d（来源：PIPELINE_BLOCK.md:91 端口 code 齐全）"
                             % (n_anchors, n_ports)))

    stage_of = facts.ir_stage_of()
    prod = facts.producers()
    cons = facts.consumers()
    per_stage: Dict[str, int] = {}
    for artifact in set(prod) & set(cons):
        for pm in prod[artifact]:
            for cm in cons[artifact]:
                if stage_of.get(pm) == stage_of.get(cm):
                    per_stage[stage_of.get(pm)] = per_stage.get(stage_of.get(pm), 0) + 1
    for stage in sorted(set(stage_of.values())):
        if per_stage.get(stage, 0) < 1:
            out.append(Violation("PC-C6/EDGE_FLOOR", stage,
                                 "阶段 %s 的生产→消费边实算数 %d < 1（来源："
                                 "VALIDATION_EVIDENCE.md:170-173 零对象守卫第 1 条）"
                                 % (stage, per_stage.get(stage, 0))))
    return out


# ==========================================================================
# 6. 判定器 —— 插值算子侧（IR-C4..IR-C8）
# ==========================================================================


def judge_ir_c4_no_phantom_edge(facts: Facts) -> List[Violation]:
    """IR-C4 无幻边，**双向**：

    (i)  IR 声明的每条读边必须由注册表端口图支持——该产物身份在生产模块上是
         `output` 端口、在消费模块上是 `input` 端口；
    (ii) 注册表端口图的每条边必须在 IR 中出现（`PIPELINE_BLOCK.md:22`「IR 的边集与
         注册表边集逐条相等」）。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:112`（(i)）与 :22（(ii)）。
    (i) 跨阶段也逐条核对（PIPELINE_BLOCK.md:22 的边定义以「产物身份」为准，不分阶段）。
    实测（HEAD bf944fad）：IR 读引用 57 条、其中 51 条构成「生产→消费」边（其余 6 条是
    阶段外部输入 `lights` / `master_bias` / `master_dark` / `master_flat` / `run_context` /
    `p1_photscale`），全部有注册表支持；注册表边 51 条，全部出现在 IR。

    「无生产者」的读引用分两路（`VALIDATION_EVIDENCE.md:412` fail-closed 要求的具名分支，
    不得「读不到就按合规处理」）：
    - 消费模块**自己声明**了同名 input 端口 ⇒ 合法的阶段外部输入，不判红；
    - 消费模块**没声明** ⇒ `IR-C4/PHANTOM_EDGE`（无任何注册表依据的幻边）。

    **未实现子句（如实登记）**：`PIPELINE_BLOCK.md:112` 还要求「IR 产物身份须在
    `PHASE1_ARTIFACT_TO_PORT` 桥表中登记（未登记即判红）」。该桥表**全仓不存在**
    （`git grep ARTIFACT_TO_PORT -- eng lib` 命中 0；仅 `PIPELINE_BLOCK.md:112` 与
    `run/GOVERN-08/审核包-R2/审稿-R3-T3-代码与文档交叉面.md:284` 命中）。判红依据
    本身无在位执行面，故不写成永远红的测试。
    """
    out: List[Violation] = []
    direction = facts.port_direction()
    prod = facts.producers()
    reads = facts.ir_reads()

    for mid, arts in sorted(reads.items()):
        for a in sorted(arts):
            producers = prod.get(a, [])
            if not producers:
                # 全注册表无生产者。若消费模块也没把它声明成 input 端口，
                # 这条 IR 读边没有任何注册表依据 ⇒ 幻边（`VALIDATION_EVIDENCE.md:412`
                # fail-closed：不得「读不到就按合规处理」）。
                # 合法的阶段外部输入（`lights` / `master_*` / `run_context` /
                # `p1_photscale`）都由消费模块自己声明成 input 端口，走不到这里。
                if direction.get((mid, a)) != "input":
                    out.append(Violation(
                        "IR-C4/PHANTOM_EDGE", "%s<-%s" % (mid, a),
                        "IR 声明的读边在注册表端口图上无任何依据：全注册表无 %r 的生产者，"
                        "消费模块也未把它声明为 input 端口" % a))
                continue
            if not any(direction.get((pm, a)) == "output" for pm in producers):
                out.append(Violation("IR-C4/PHANTOM_PRODUCER", "%s<-%s" % (mid, a),
                                     "无模块把 %r 声明为 output 端口" % a))
            if direction.get((mid, a)) != "input":
                out.append(Violation("IR-C4/NOT_INPUT", "%s<-%s" % (mid, a),
                                     "消费模块未把 %r 声明为 input 端口（实际 %r）"
                                     % (a, direction.get((mid, a)))))

    for (artifact, pm, cm) in facts.registry_edges():
        if artifact not in reads.get(cm, set()):
            out.append(Violation("IR-C4/MISSING_EDGE", "%s->%s:%s" % (pm, cm, artifact),
                                 "注册表端口图有此边，IR 的 %s.reads 未声明 %r"
                                 % (cm, artifact)))
    return out


def judge_ir_c5_topological_order(facts: Facts) -> List[Violation]:
    """IR-C5 序为拓扑序，**限同阶段内**。

    限同阶段的依据：`PIPELINE_BLOCK.md:22`「**同阶段内**「A 的输出端口名 == B 的输入
    端口名」即一条依赖边；管线 IR 的节点序必须是该 DAG 的拓扑序」。跨阶段交换是三个
    平级命令之间的磁盘交接（`PIPELINE_BLOCK.md:17-21`），不是同一条内存管线的节点边。

    必须限同阶段的实测依据：`gen_block_flow_spec.py:50` 的排序键是
    `lambda n: (n["stage"], reg_index(reg, n["module_id"]))`——`stage` 是**字典序**，
    因此 `stage_block_flow.json` 的节点数组序是 `export → mosaic → normalize`。若把
    跨阶段边也纳入本判据，8 条跨阶段边必然逆序（`frame_hips` 由
    `acsd.phase1.drizzle`[18] 产、`acsd.phase2.coverage`[5] 消，等），即该判据与
    派生脚本自相矛盾、恒红。限同阶段后实测 43 条同阶段边、0 违例。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:113`（含「IR 自身声明的边
    也必须与节点数组序一致」这后半句）。
    """
    out: List[Violation] = []
    stage_of = facts.ir_stage_of()
    pos = facts.ir_pos_of()
    reads = facts.ir_reads()
    prod = facts.producers()

    for (artifact, pm, cm) in facts.registry_edges():
        if stage_of.get(pm) != stage_of.get(cm):
            continue
        if pos.get(pm, -1) >= pos.get(cm, -1):
            out.append(Violation("IR-C5/NOT_TOPOLOGICAL", "%s->%s:%s" % (pm, cm, artifact),
                                 "同阶段边逆序：pos(%s)=%d ≥ pos(%s)=%d"
                                 % (pm, pos.get(pm, -1), cm, pos.get(cm, -1))))

    for cm, arts in sorted(reads.items()):
        for a in sorted(arts):
            for pm in prod.get(a, []):
                if stage_of.get(pm) != stage_of.get(cm):
                    continue
                if pos.get(pm, -1) >= pos.get(cm, -1):
                    out.append(Violation("IR-C5/IR_EDGE_ORDER", "%s->%s:%s" % (pm, cm, a),
                                         "IR 自身声明的边逆序：pos(%s)=%d ≥ pos(%s)=%d"
                                         % (pm, pos.get(pm, -1), cm, pos.get(cm, -1))))
    return out


#: `gen_block_flow_spec.py:24` 的三相↔三阶段映射（本单元唯一的阶段名来源）。
PHASE_TO_STAGE = {"phase1": "normalize", "phase2": "mosaic", "phase3": "export"}


def judge_ir_c5b_declared_order(facts: Facts) -> List[Violation]:
    """IR-C5b 声明序一致：IR 的同阶段节点序必须等于注册表 `modules` 数组里同阶段
    模块的出现序。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:114`（「块流规格
    `stage_block_flow.json` 的 declared order 与 R4 同源」）+
    `eng/tools/quality/gen_block_flow_spec.py:50` 的排序键第二项
    `reg_index(reg, n["module_id"])`（:105-109 定义为 `modules` 数组下标，未命中返回 999）。
    实测（HEAD bf944fad）三阶段全部相等：
      normalize = [calibration, cosmetic, wcs-platesolve, star-psf, photometry,
                   noise-snr, drizzle, writer]
      mosaic    = [coverage, sample, upm-fit, upm-apply, reject, integrate, write]
      export    = [properties, wcs, resample2, writer, verify]
    """
    out: List[Violation] = []
    for phase, stage in sorted(PHASE_TO_STAGE.items()):
        declared = [m["module_id"] for m in facts.modules if m.get("phase") == phase]
        derived = [n["module_id"] for n in facts.ir_nodes() if n.get("stage") == stage]
        if declared != derived:
            out.append(Violation("IR-C5b/DECLARED_ORDER", stage,
                                 "注册表声明序 %s ≠ IR 派生序 %s" % (declared, derived)))
    return out


def judge_ir_c6_psf_after_wcs(facts: Facts) -> List[Violation]:
    """IR-C6 psf 在 wcs 之后：`pos(psf) > pos(wcs)`，且 `psf` 节点必须声明
    `artifact:p1_wcs` 输入边（取向先验的真实来源，取值先验）。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:115`。
    「取向先验的真实来源」的代码侧出处：`module_adapters.cpp:907-913`
    （`p1_star_psf_descriptor` 里 `{"wcs", "DATA-P1-WCS", true, …}` 的注释块：
    「星表引导检测要把 Gaia 星表逆投影到像素域, 逆投影必须知道像面取向与镜像…
    声明为 typed 输入端口 ⇒ 调度器保证 wcs 节点先落盘」）。
    实测（HEAD bf944fad）：pos(acsd.phase1.star-psf)=15 > pos(acsd.phase1.wcs-platesolve)=14，
    且 star-psf 的 reads 含 `p1_wcs`。
    """
    out: List[Violation] = []
    psf, wcs = "acsd.phase1.star-psf", "acsd.phase1.wcs-platesolve"
    pos = facts.ir_pos_of()
    reads = facts.ir_reads()
    if psf not in pos or wcs not in pos:
        missing = [x for x in (psf, wcs) if x not in pos]
        raise ParityError("RESOLUTION_FAILED",
                          "IR-C6 解析不到节点: %s（IR nodes 共 %d 个）"
                          % (missing, len(facts.ir_nodes())))
    if not pos[psf] > pos[wcs]:
        out.append(Violation("IR-C6/ORDER", "acsd.phase1.star-psf",
                             "pos(psf)=%d 未大于 pos(wcs)=%d" % (pos[psf], pos[wcs])))
    if "p1_wcs" not in reads.get(psf, set()):
        out.append(Violation("IR-C6/MISSING_WCS_INPUT", "acsd.phase1.star-psf",
                             "psf 节点未声明 p1_wcs 输入边（取向先验的真实来源）"))
    return out


def judge_ir_c7_non_degenerate(facts: Facts) -> List[Violation]:
    """IR-C7 非退化：phase1 节点数 / IR 边数 / 注册表端口边数均有下界；解析不到即
    fail-closed（判据 = 解析成功）。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:116`。下界来源：

    | 下界 | 取值 | 来源 |
    |---|---|---|
    | normalize 阶段 IR 节点数 | ≥ 3 | `gen_block_flow_spec.py:30` `STAGE_TERMINALS["normalize"] = {frame_hips, p1_final, p1_products}` 三个终产物；:59 `produced.setdefault((st,b),[]).append(...)` 逐节点记录 ⇒ 每个终产物各有其生产节点 ⇒ 至少 3 个节点 |
    | IR 边数（normalize 阶段内） | ≥ 1 | `VALIDATION_EVIDENCE.md:170-173` 零对象守卫第 1 条 |
    | 注册表端口边数（normalize 阶段内） | ≥ 1 | 同上 |
    | fail-closed | 解析不到即红 | `PIPELINE_BLOCK.md:116` 明列 |
    """
    out: List[Violation] = []
    nodes = facts.ir_nodes()
    if not nodes:
        raise ParityError("RESOLUTION_FAILED", "IR-C7：IR nodes 为空/解析不到（判红）")
    stage_of = facts.ir_stage_of()
    pos = facts.ir_pos_of()
    reads = facts.ir_reads()
    prod = facts.producers()

    n_nodes = sum(1 for n in nodes if n.get("stage") == "normalize")
    if n_nodes < 3:
        out.append(Violation("IR-C7/NODE_FLOOR", "normalize",
                             "normalize 节点数 %d < 3（来源：gen_block_flow_spec.py:30 "
                             "STAGE_TERMINALS['normalize'] 三个终产物各需生产节点）" % n_nodes))

    ir_edges = 0
    for cm, arts in reads.items():
        for a in arts:
            for pm in prod.get(a, []):
                if stage_of.get(pm) == "normalize" and stage_of.get(cm) == "normalize":
                    ir_edges += 1
    if ir_edges < 1:
        out.append(Violation("IR-C7/IR_EDGE_FLOOR", "normalize",
                             "IR 声明的 normalize 阶段内边数 %d < 1（来源："
                             "VALIDATION_EVIDENCE.md:170-173 零对象守卫第 1 条）" % ir_edges))

    reg_edges = 0
    for (artifact, pm, cm) in facts.registry_edges():
        if stage_of.get(pm) == "normalize" and stage_of.get(cm) == "normalize":
            reg_edges += 1
    if reg_edges < 1:
        out.append(Violation("IR-C7/REG_EDGE_FLOOR", "normalize",
                             "注册表 normalize 阶段内端口边数 %d < 1（来源："
                             "VALIDATION_EVIDENCE.md:170-173 零对象守卫第 1 条）" % reg_edges))
    return out


#: descriptor 端口表的源码形态：`ModuleDescriptor <fn>() { … d.ports = { {"name",
#: "DATA-ID", true|false, UnitId::…, CoordinateFrame::…}, … }; … }`。
DESCRIPTOR_FN = re.compile(r"ModuleDescriptor\s+([A-Za-z0-9_]+)\s*\(\s*\)\s*\{")
DESCRIPTOR_PORT = re.compile(
    r'\{"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(true|false)\s*,')


def _masked_group(raw_body: str, masked_body: str, pattern: str, group: int = 1) -> Optional[str]:
    """在**掩码**上定位捕获组的位置，再从**原文**同区间取内容。

    用途：字符串字面量的内容在掩码里已被抹成空格，直接对掩码做 `([^"]+)` 只能拿到
    空白；必须用掩码定位、用原文取值。两者等长，故区间可直接对应。
    """
    m = re.search(pattern, masked_body)
    if m is None:
        return None
    return raw_body[m.start(group):m.end(group)]


def parse_descriptors(unit: CppUnit) -> Dict[str, Dict[str, Any]]:
    """解析 `module_adapters.cpp` 的全部 `ModuleDescriptor <fn>()` 定义。

    返回 `module_id -> {"line": int, "fn": str, "ports": [(name, is_input), …],
    "data_ids": [DATA-ID, …]}`。端口项形态（实测）：
    `{"<port_name>", "<DATA-ID>", true|false, UnitId::…, CoordinateFrame::…}`，
    第三项 `true` = 输入、`false` = 输出（`module_adapters.cpp:711-712`）。

    实测（HEAD bf944fad）解析出 22 个 descriptor 定义，其中 20 个 `module_id` 命中
    注册表的 20 个 module；另 3 个是聚合 descriptor（`phase2_descriptor` /
    `phase3_descriptor` 的 `module_id` 是 `acsd.phase2.resample` /
    `acsd.phase3.resample`，不在注册表内，见 `module_adapters.cpp:724-731`、
    `:753-759` 自带的「已知不一致（未裁）」注释块）。
    """
    out: Dict[str, Dict[str, Any]] = {}
    for m in DESCRIPTOR_FN.finditer(unit.raw):
        open_brace = unit.raw.index("{", m.start())
        close_brace = _match_pair(unit.masked, open_brace, "{", "}")
        if close_brace < 0:
            continue
        body_raw = unit.raw[open_brace:close_brace + 1]
        body_msk = unit.masked[open_brace:close_brace + 1]
        module_id = _masked_group(body_raw, body_msk, r'd\.module_id\s*=\s*"([^"]*)"')
        if not module_id:
            continue
        ports = [(mm.group(1), mm.group(3) == "true")
                 for mm in DESCRIPTOR_PORT.finditer(body_raw)]
        data_ids = re.findall(r'\{"[^"]+"\s*,\s*"([^"]+)"\s*,\s*(?:true|false)\s*,', body_raw)
        out[module_id] = {"line": unit.line_of(m.start()), "ports": ports,
                          "data_ids": data_ids, "fn": m.group(1)}
    return out


def ir_c8_execution_surface(facts: Facts, adapters_rel: str = ADAPTERS_PATH) -> Dict[str, Any]:
    """度量 IR-C8（`PIPELINE_BLOCK.md:117`）在当前代码上**有没有在位执行面**。

    IR-C8 的字面判据是「IR 每个节点的输入/输出端口**名**必须出现在
    `module_adapters.cpp` 对应 descriptor 的端口表里」。要执行它，必须存在一套两侧
    共用的端口名词表（或一条有正本的桥表）。本函数把这件事量化成三个实测数：

    | 指标 | 含义 |
    |---|---|
    | `n_ir_refs` | IR 节点的 `reads`+`writes` 端口引用总条数 |
    | `n_name_hits` | 其中**逐字**出现在对应 descriptor 端口表里的条数 |
    | `n_dataid_hits` / `n_dataid_misses` | 退一步用 `data_schema_id` ↔ descriptor `data_id` 桥接的命中/未命中 |
    | `has_execution_surface` | `n_name_hits > 0`（逐字口径可执行）或 `n_dataid_misses == 0`（桥接口径可执行） |

    实测（HEAD bf944fad）：
      - `n_ir_refs = 85`，`n_name_hits = 0`；
      - `n_dataid_hits = 55`、`n_dataid_misses = 30`；
      - descriptor 端口表**在位**（20/20 module 有对应 descriptor，解析出 22 个
        `ModuleDescriptor` 定义，另 3 个是 `module_id` 不在注册表内的聚合 descriptor），
        但两套端口词表不相交 ⇒ **IR-C8 缺在位对象，不是「判红」而是「判据不可执行」**。

    两条端口词表（实测摘录）：
      IR / 注册表（产物身份词表）：`p1_cleaned` `p1_wcs` `p1_sources` `p1_psf`
        `p1_calibrated` `p3_props` `p3_wcs` `mosaic_hips` `frame_hips` `run_context` …
      descriptor（typed 绑定短名）：`cleaned` `wcs` `sources` `psf` `calibrated`
        `props` `wcs_plan` `resampled` `fits` `verified` `hips` `mosaic` `stacked` …
      出处：`module_adapters.cpp:710-713`（phase1_descriptor）、
      `:905-916`（p1_star_psf_descriptor）、`:828-832`（p3_resample2_descriptor）。

    descriptor 端口表**本身不完整**（与注册表逐模块对比实测）：
      `p1_writer_descriptor`（:1052）只有 `stacked`/`fits` 两个端口，注册表同模块声明 5 个；
      `p2_upm_apply_descriptor`（:1133）只有 3 个，注册表声明 6 个；
      `p2_write_descriptor`（:1192）只有 `integrated`/`mosaic`，注册表声明 8 个。
    """
    unit = facts.units.get(adapters_rel)
    if unit is None:
        raise ParityError("RESOLUTION_FAILED",
                          "IR-C8 无法取到 %s（descriptor 端口表解析面缺失）" % adapters_rel)
    descs = parse_descriptors(unit)
    if not descs:
        raise ParityError("RESOLUTION_FAILED",
                          "IR-C8：在 %s 中解析不到任何 ModuleDescriptor 定义" % adapters_rel)

    reg_data_id: Dict[Tuple[str, str], str] = {}
    for (m, _op, p) in facts.ports():
        reg_data_id[(m["module_id"], p["name"])] = p.get("data_schema_id")

    n_ir_refs = 0
    n_name_hits = 0
    n_dataid_hits = 0
    n_dataid_misses = 0
    missing_descriptor: List[str] = []
    misses: List[str] = []
    for n in facts.ir_nodes():
        mid = n["module_id"]
        d = descs.get(mid)
        if d is None:
            missing_descriptor.append(mid)
            continue
        names = {pname for pname, _is_in in d["ports"]}
        dids = set(d["data_ids"])
        for pname in list(n.get("reads") or []) + list(n.get("writes") or []):
            n_ir_refs += 1
            if pname in names:
                n_name_hits += 1
            else:
                misses.append("%s:%s" % (mid, pname))
            rid = reg_data_id.get((mid, pname))
            if rid is not None and rid in dids:
                n_dataid_hits += 1
            else:
                n_dataid_misses += 1

    return {
        "n_descriptors": len(descs),
        "n_registry_modules": len(facts.modules),
        "missing_descriptor": missing_descriptor,
        "n_ir_refs": n_ir_refs,
        "n_name_hits": n_name_hits,
        "n_dataid_hits": n_dataid_hits,
        "n_dataid_misses": n_dataid_misses,
        "misses": misses,
        "has_execution_surface": (n_name_hits > 0) or (n_ir_refs > 0 and n_dataid_misses == 0),
    }


#: `PIPELINE_BLOCK.md:112`（IR-C4）点名的桥表标识。
IR_C4_BRIDGE_SYMBOL = "PHASE1_ARTIFACT_TO_PORT"

#: 桥表搜索面。**排除 `eng/tests/`**：本单元自身把该标识写成常量与检索词，
#: 把它算成「命中」会让判据读成「桥表存在」，是自指的假绿。
BRIDGE_SEARCH_ROOTS = ("lib", "eng")
BRIDGE_SEARCH_EXCLUDE = ("eng/tests", "run")


def find_bridge_symbol(base: str = REPO_ROOT,
                       search_roots: Sequence[str] = BRIDGE_SEARCH_ROOTS,
                       exclude: Sequence[str] = BRIDGE_SEARCH_EXCLUDE) -> Optional[str]:
    """在给定仓库子目录里找 IR-C4 点名的桥表符号；返回命中的相对路径，找不到返回 None。

    只扫**已跟踪的源/合同文本**面（`.cpp`/`.h`/`.hpp`/`.py`/`.json`），不做 git 调用。
    默认排除 `eng/tests`（本单元自指）与 `run`（历史交付件，不是产品面）。
    """
    excluded = tuple(os.path.join(base, x) for x in exclude)
    for root_rel in search_roots:
        root = os.path.join(base, root_rel)
        if not os.path.isdir(root):
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [d for d in dirnames if d not in (".git", "__pycache__")]
            if any(dirpath.startswith(e) for e in excluded):
                dirnames[:] = []
                continue
            for fn in filenames:
                if not fn.endswith((".cpp", ".h", ".hpp", ".py", ".json")):
                    continue
                path = os.path.join(dirpath, fn)
                try:
                    with open(path, "r", encoding="utf-8", errors="replace") as fh:
                        content = fh.read()
                except OSError:
                    continue
                if IR_C4_BRIDGE_SYMBOL in content:
                    return os.path.relpath(path, base)
    return None


# ==========================================================================
# 7. 事实面装载（仓库实况）与临时副本沙箱（负例隔离）
# ==========================================================================


def build_repo_facts() -> Facts:
    """装载仓库实况事实面。锚点缺失 ⇒ `ANCHOR_STALE`（`VALIDATION_EVIDENCE.md:413`）。"""
    for name, rel in ANCHOR_TABLE:
        if not os.path.isfile(os.path.join(REPO_ROOT, rel)):
            raise ParityError("ANCHOR_STALE", "%s %s" % (name, rel))
    registry = parse_json(read_text(os.path.join(REPO_ROOT, REGISTRY_PATH)), REGISTRY_PATH)
    ir = parse_json(read_text(os.path.join(REPO_ROOT, IR_PATH)), IR_PATH)
    rels = set()
    for m in (registry.get("modules") or []):
        for op in (m.get("operations") or []):
            for p in (op.get("ports") or []):
                for c in (p.get("code") or []):
                    rels.add(c["file"])
    units: Dict[str, CppUnit] = {}
    for rel in sorted(rels):
        units[rel] = CppUnit(rel, read_text(os.path.join(REPO_ROOT, rel)))
    if IR_SCHEMA != ir.get("schema"):
        raise ParityError("RESOLUTION_FAILED",
                          "IR schema 期望 %r，实际 %r" % (IR_SCHEMA, ir.get("schema")))
    return Facts(registry, ir, units)


class Sandbox:
    """负例隔离沙箱（`VALIDATION_EVIDENCE.md:197`「反例复跑必须隔离」）。

    在 `tempfile` 造的临时目录里复制注册表 JSON、管线 IR JSON 与两份 C++ 源码，
    保持仓库相对路径结构；注入只改副本，仓库一个字节都不动。`close()` 或 `with`
    退出即删除临时目录。
    """

    KEYS = ("registry", "ir", "adapters", "sink")

    def __init__(self) -> None:
        self._tmp = tempfile.mkdtemp(prefix="acsd_registry_parity_")
        self._rel = {
            "registry": REGISTRY_PATH,
            "ir": IR_PATH,
            "adapters": ADAPTERS_PATH,
            "sink": SINK_PATH,
        }
        for key, rel in self._rel.items():
            src = os.path.join(REPO_ROOT, rel)
            if not os.path.isfile(src):
                raise ParityError("ANCHOR_STALE", "SANDBOX_SOURCE %s" % rel)
            dst = os.path.join(self._tmp, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(src, dst)

    @property
    def tmp_root(self) -> str:
        """临时副本根目录（负例需要往里注入夹具时用；仓库不受影响）。"""
        return self._tmp

    # -- 路径 --
    def repo_rel(self, key: str) -> str:
        return self._rel[key]

    def path(self, key: str) -> str:
        return os.path.join(self._tmp, self._rel[key])

    # -- 读 --
    def load(self, key: str) -> Any:
        return parse_json(read_text(self.path(key), "SANDBOX_COPY_MISSING"),
                          self._rel[key])

    def text(self, key: str) -> str:
        return read_text(self.path(key), "SANDBOX_COPY_MISSING")

    # -- 写（只改副本） --
    def save(self, key: str, obj: Any) -> None:
        with open(self.path(key), "w", encoding="utf-8") as fh:
            fh.write(json.dumps(obj, ensure_ascii=False, indent=2) + "\n")

    def set_text(self, key: str, text: str) -> None:
        with open(self.path(key), "w", encoding="utf-8") as fh:
            fh.write(text)

    # -- 事实面 --
    def facts(self) -> Facts:
        units = {}
        for key in ("adapters", "sink"):
            units[self._rel[key]] = CppUnit(self._rel[key], self.text(key))
        return Facts(self.load("registry"), self.load("ir"), units)

    def close(self) -> None:
        shutil.rmtree(self._tmp, ignore_errors=True)

    def __enter__(self) -> "Sandbox":
        return self

    def __exit__(self, *_exc: Any) -> None:
        self.close()


# --------------------------------------------------------------------------
# 负例注入原语（**只作用于沙箱副本**）
# --------------------------------------------------------------------------


#: 缺陷登记 R1：C3 在基线上判红的**具名身份集合** `(产物 token, 触发的 module@symbol)`。
#:
#: 这是**缺陷钉桩**（钉「已登记的缺陷是谁」），**不是规模锚**：预期值逐条由
#: `文件:行` 的代码证据给出，不是「拿当前输出生成」。因此产品合法增删端口时，
#: 本集合不变；若这三条真的被修好，负例会红并要求复核方重新裁决预期值。
#:
#: 证据：
#: - `/master_refs.json` @ `acsd.phase1.calibration@p1_op_calibrate`
#:   —— `lib/infrastructure/scheduler/src/module_adapters.cpp:2761` 写
#:      `const std::string refs_path = out_dir + "/master_refs.json";`
#: - `/master_refs.json` @ `acsd.phase1.cosmetic@p1_op_cosmetic`
#:   —— 同文件 `:2910` 读 `doc.value("output_dir", …) + "/master_refs.json"` 做母版回填
#: - `/badcol_report.json` @ `acsd.phase1.cosmetic@p1_op_cosmetic`
#:   —— 同文件 `:3290` 写 `… + "/badcol_report.json"`
#:
#: 注册表 `module_ports.registry.json` 对 `master_refs` / `badcol` / `bad_column`
#: 全文命中均为 **0**。
BASELINE_UNDECLARED: Tuple[Tuple[str, str], ...] = (
    ("/master_refs.json", "acsd.phase1.calibration@p1_op_calibrate"),
    ("/master_refs.json", "acsd.phase1.cosmetic@p1_op_cosmetic"),
    ("/badcol_report.json", "acsd.phase1.cosmetic@p1_op_cosmetic"),
)

#: 缺陷登记 R2：PC-C5 第 4 子句 `EXTERNAL_INPUT_CARRIER` 在基线上判红的**具名身份**。
#: 证据：`module_ports.registry.json:424-439` 声明 `acsd.phase1.photometry:p1_photscale`
#: 为 `carrier=output_dir_file` + `direction=input`，而全注册表无任何生产者；
#: 代码侧 `lib/infrastructure/scheduler/src/module_adapters.cpp:6071`
#: 读 `out_dir + "/p1_photscale.json"`；派生 IR
#: `eng/contracts/block_flow/stage_block_flow.json:411-417` 记 `EXTERNAL_IN` /
#: `produced_by: []`。按 `PIPELINE_BLOCK.md:21`，阶段外部输入的载体枚举值是 `config_path`。
BASELINE_PC_C5_EXTERNAL_INPUT = (("acsd.phase1.photometry:p1_photscale",),)


def module_by_id(facts: Facts, module_id: str) -> Dict[str, Any]:
    for m in facts.modules:
        if m.get("module_id") == module_id:
            return m
    raise ParityError("RESOLUTION_FAILED", "沙箱注册表里找不到 module %r" % module_id)


def ir_node_by_id(ir: Dict[str, Any], module_id: str) -> Dict[str, Any]:
    for n in ir.get("nodes") or []:
        if n.get("module_id") == module_id:
            return n
    raise ParityError("RESOLUTION_FAILED", "沙箱 IR 里找不到 node %r" % module_id)


def first_port(facts: Facts, module_id: str, direction: str) -> Dict[str, Any]:
    m = module_by_id(facts, module_id)
    for op in m.get("operations") or []:
        for p in op.get("ports") or []:
            if p.get("direction") == direction:
                return p
    raise ParityError("RESOLUTION_FAILED",
                      "%s 没有 direction=%s 的端口" % (module_id, direction))