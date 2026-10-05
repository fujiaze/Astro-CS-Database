"""模块层块流共享解析器与判定逻辑（纯标准库，不链接产品、不起子进程）。

## 用途

`test_block_lifecycle.py` 的唯一支撑模块。职责只有两件：

1. **锚存活 + fail-closed 装载**：把本层硬编码引用的仓内锚（块流规格、端口注册表、
   派生脚本源码、块 schema）解析出来；任一锚缺失、不可读、JSON 解析失败、结构缺字段
   一律抛具名异常，**绝不 skip、绝不降级为通过**。
2. **独立 Oracle 与判定逻辑**：按 `eng/tools/quality/gen_block_flow_spec.py` 文件头
   docstring 逐字写下的派生规则，从 `lib/infrastructure/pipeline/module_ports.registry.json`
   **独立重算**每个 `(stage, block)` 的生命周期；并把 A1–A9 每条判据实现成
   `judge_*(spec) -> list[str]`（返回违规清单，空清单 = 绿）。

判定逻辑放在这里而不是散在用例里，是为了让负例能**复用同一条判定**做注入复跑
（`docs/engineering/testing/VALIDATION_EVIDENCE.md` §8 S6/S2：注入点必须是该判定自身的
判定逻辑，且未注入时同一路径必须给绿）。

## 正本依据（逐条 `文件:行`）

| 本模块用到的量 | 来源 |
|---|---|
| 生命周期派生规则（消费者数 ≥2 ⇒ STAGE 等 5 条） | `eng/tools/quality/gen_block_flow_spec.py:7-13`（docstring 逐字） |
| 阶段映射 `phase1/2/3 → normalize/mosaic/export` | `eng/tools/quality/gen_block_flow_spec.py:24` |
| 阶段终产物 `STAGE_TERMINALS` | `eng/tools/quality/gen_block_flow_spec.py:30-32` |
| 块名 = 注册表端口名、不发明新名 | `eng/tools/quality/gen_block_flow_spec.py:5` |
| 节点序 = `(stage, 注册表出现序)` | `eng/tools/quality/gen_block_flow_spec.py:50` |
| 块级统计按**阶段作用域** | `eng/tools/quality/gen_block_flow_spec.py:52-61` |
| DAG 校验四条 | `docs/engineering/contracts/PIPELINE_BLOCK.md:40-44` |
| `consumers` 可空 ⇒ 终态块 | `docs/engineering/contracts/PIPELINE_BLOCK.md:36` |
| 阶段结束块残留数 = 0 | `docs/engineering/contracts/PIPELINE_BLOCK.md:55` |
| 阶段终产物只以磁盘产品发布、不占运行期块 | `docs/engineering/contracts/PIPELINE_BLOCK.md:66` |
| 跨阶段唯一载体 = HiPS 产品树 | `docs/engineering/contracts/PIPELINE_BLOCK.md:20-21` |
| 块名小写蛇形正则 | `eng/contracts/schemas/pipeline_block.schema.json:32` |
| `consumers` 为 string 数组 | `eng/contracts/schemas/pipeline_block.schema.json:57-61` |
| schema 侧 `lifecycle` 枚举 = `short/frame/run` | `eng/contracts/schemas/pipeline_block.schema.json:68-74` |
| 三命令合同 / 阶段独立调度器 | `docs/ACSD_DESIGN.md:364-379` |
| 块被全部消费者用完即销毁、内存立即归还 | `docs/ACSD_DESIGN.md:384` |
| 容差档（元数据/端口/选择结果 = 精确一致） | `docs/engineering/testing/TEST.md:46` |
| fail-closed 与锚存活 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:412-413` |
| 零对象守卫两条 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:170-176` |
| 判别力 S2（同时验证绿）/ S6（内容级负例） | `docs/engineering/testing/VALIDATION_EVIDENCE.md:189,193` |
| 反例复跑必须隔离（临时副本） | `docs/engineering/testing/VALIDATION_EVIDENCE.md:197` |

## 独立 Oracle 的来源

本模块的 Oracle **不读** `eng/contracts/block_flow/stage_block_flow.json` 的任何现值来生成
预期值。预期值的来源只有两处：

- **规则文本**：`gen_block_flow_spec.py` 文件头 docstring 的 5 条派生规则 +
  `STAGE_TERMINALS` 字面量（用 `ast.literal_eval` 从**源码文本**提取，不执行该脚本）；
- **注册表实算**：`module_ports.registry.json` 的 `modules[].operations[].ports[]`，
  按端口 `direction` 重建每阶段的 `reads` / `writes`，再按消费者数 / 生产者数套规则。

块流规格在 A1 中是**被测对象**，不是 Oracle 的输入面。

## 容差档

本层不比较任何浮点量，全部比较对象是**元数据、端口名、选择结果与计数**，
落 `TEST.md:46` 第一档「精确一致」（整数与字符串元数据的相等即 `==`）。因此本层不引入
`rtol`/`atol`，`EXACT` 常量即 `0.0`，无「适用量级域 `scale`」问题（`TEST.md:73` 的绝对容差
可满足性下限只对绝对容差成立）。

## Oracle 独立性声明

- `oracle.truth` = `structural`（`VALIDATION_EVIDENCE.md:11` 白名单项）+ 规则文本；
- `oracle.must_not` = 产品可执行程序 `acsd`、libacsd、调度器代码面。本模块**只**读
  `lib/infrastructure/pipeline/module_ports.registry.json`、`eng/contracts/**` 两面数据文件与
  `eng/tools/quality/gen_block_flow_spec.py` 的**源码文本**，不执行任何产品二进制、不起子进程、
  不链接任何库。

## 不产出阻塞退出码

本模块与同目录用例**不注册进 CMakeLists、不接 CI、不产出退出码判决**，不调用
`sys.exit` / `raise SystemExit` / `os._exit`。依据 `AGENTS.md` §8「测试集…不与产品代码混放」
与 `TEST.md:115`「执行者是人：结论由人读对抗审核给出，每条结论都附可复算的证据。测试集不产出
流水线判决」。
"""

from __future__ import annotations

import ast
import json
import os
import re
from typing import Any, Dict, List, Sequence, Set, Tuple

# ---------------------------------------------------------------------------
# §0 锚点常量（VALIDATION_EVIDENCE.md:413「锚存活」：硬编码路径必须存在）
# ---------------------------------------------------------------------------

#: 仓根。`_blockflow.py` 位于 `<repo>/eng/tests/module/`，向上四级到仓根。
REPO_ROOT = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
)

#: 块流规格（被测对象，A1–A9 的主输入面）。
SPEC_REL = "eng/contracts/block_flow/stage_block_flow.json"
#: 端口注册表（**独立 Oracle 的真值来源**，A1 的重算输入面）。
REGISTRY_REL = "lib/infrastructure/pipeline/module_ports.registry.json"
#: 派生脚本（**规则文本正本**：docstring 5 条规则 + STAGE_TERMINALS 字面量）。
GEN_SCRIPT_REL = "eng/tools/quality/gen_block_flow_spec.py"
#: 块元数据机器 schema（块名字段正则、consumers 形态、schema 侧 lifecycle 枚举）。
BLOCK_SCHEMA_REL = "eng/contracts/schemas/pipeline_block.schema.json"

#: 容差档引用（`TEST.md:46` 第一档）。本层无数值比较，保留常量只为把口径写进可执行面。
EXACT_TOLERANCE = 0.0  # TEST.md:46「元数据、掩膜、计数、索引、端口、选择结果 = 精确一致」

#: 派生规则文本里出现的生命周期词表（`gen_block_flow_spec.py:8-12` 五条规则的右值）。
#: 这是**规格侧**的合法取值集合，与 schema 侧枚举不同 —— 见 `SCHEMA_LIFECYCLE_VOCAB`。
DERIVED_LIFECYCLE_VOCAB = ("EXTERNAL_IN", "EXTERNAL_OUT", "STAGE", "SHORT")

#: 块流规格块的冻结字段集。来源不是 schema：`pipeline_block.schema.json` 描述的是
#: **块物理元数据**文档（name/shape/dtype/unit/optional/producer/consumers/lifecycle），
#: 而 `stage_block_flow.json` 是**流拓扑**文档（stage/block/lifecycle/produced_by/consumed_by），
#: 两者是两份不同 `$id` 的产物（`acsd.pipeline-block/v1` vs `acsd.stage-block-flow/v1`）。
#: 详见 `README.md` 登记项 REG-03。
FLOW_BLOCK_FIELDS = frozenset({"stage", "block", "lifecycle", "produced_by", "consumed_by"})

#: `SCHEMA_LIFECYCLE_VOCAB` 差异的登记项（REG-03）。
#: 结构化记录「规格侧词表 / schema 侧词表 / 交集」，用于让 README 的登记项与代码里的
#: 断言保持同步：任一侧漂移而登记项未同步时，漂移守卫判红并点名。
SCHEMA_LIFECYCLE_VOCAB = {
    "spec_side": ["EXTERNAL_IN", "EXTERNAL_OUT", "SHORT", "STAGE"],
    "schema_side": ["frame", "run", "short"],
    "intersection": [],
    "schema_block_fields_absent_from_flow_spec": [
        "dtype", "name", "optional", "producer", "consumers",
        "provenance", "shape", "unit",
    ],
}

#: `gen_block_flow_spec.py` 文件头 docstring 里必须逐字存在的 5 条规则行。
#: Oracle 的真值来源就是这 5 行；规则文本被改写时本常量先判红，强制复核方同步 Oracle。
RULE_LINES = (
    "消费者数 >= 2 ⇒ STAGE",
    "消费者数 == 0 且是阶段终产物 ⇒ EXTERNAL_OUT",
    "消费者数 == 0 且非终产物 ⇒ STAGE",
    "消费者数 == 1 ⇒ SHORT",
    "无生产者 ⇒ EXTERNAL_IN",
)


class AnchorStale(RuntimeError):
    """锚存活失败（`VALIDATION_EVIDENCE.md:413`）。一律 fail-closed，绝不 skip。"""


class SpecUnparsable(RuntimeError):
    """块流规格不可解析或结构缺字段（`VALIDATION_EVIDENCE.md:412`）。一律判红。"""


# ---------------------------------------------------------------------------
# §1 锚装载：fail-closed
# ---------------------------------------------------------------------------

def anchor_abs(rel_path: str, const_name: str) -> str:
    """返回锚的绝对路径；不存在即抛 `ANCHOR_STALE: <常量名> <路径>`。"""
    abs_path = os.path.join(REPO_ROOT, rel_path)
    if not os.path.isfile(abs_path):
        raise AnchorStale(f"ANCHOR_STALE: {const_name} {rel_path}")
    return abs_path


def load_text(rel_path: str, const_name: str) -> str:
    """读锚文本；不可读即 `ANCHOR_STALE`。"""
    abs_path = anchor_abs(rel_path, const_name)
    with open(abs_path, "r", encoding="utf-8") as fh:
        return fh.read()


def load_json_anchor(rel_path: str, const_name: str) -> Any:
    """读锚 JSON；缺失即 `ANCHOR_STALE`，解析失败即 `SpecUnparsable`。"""
    abs_path = anchor_abs(rel_path, const_name)
    try:
        with open(abs_path, "r", encoding="utf-8") as fh:
            return json.load(fh)
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise SpecUnparsable(f"ANCHOR_UNPARSABLE: {const_name} {rel_path} :: {exc}") from exc


def _literal_assign(src: str, name: str, rel_path: str) -> Any:
    """用 `ast` 从**源码文本**里取模块级字面量赋值（不执行被引脚本）。

    这样 Oracle 的常量来源是派生脚本的源文本，而不是它的运行结果——
    满足「预期值必须来自规则文本、不得由当前程序输出生成」（本单纪律 C.1）。
    """
    try:
        tree = ast.parse(src)
    except SyntaxError as exc:  # pragma: no cover - 源文件语法错时 fail-closed
        raise SpecUnparsable(f"ANCHOR_UNPARSABLE: GEN_SCRIPT {rel_path} :: {exc}") from exc
    for node in tree.body:
        if not isinstance(node, ast.Assign):
            continue
        for target in node.targets:
            if isinstance(target, ast.Name) and target.id == name:
                try:
                    return ast.literal_eval(node.value)
                except ValueError as exc:
                    raise SpecUnparsable(
                        f"ANCHOR_NOT_LITERAL: {name} @ {rel_path} :: {exc}"
                    ) from exc
    raise AnchorStale(f"ANCHOR_STALE: {name} @ {rel_path}")


def stage_terminals() -> Dict[str, frozenset]:
    """从 `gen_block_flow_spec.py:30-32` 的 `STAGE_TERMINALS` 取阶段终产物集合。"""
    src = load_text(GEN_SCRIPT_REL, "GEN_SCRIPT")
    raw = _literal_assign(src, "STAGE_TERMINALS", GEN_SCRIPT_REL)
    if not isinstance(raw, dict) or not raw:
        raise SpecUnparsable(f"ANCHOR_BAD_SHAPE: STAGE_TERMINALS @ {GEN_SCRIPT_REL}")
    return {stage: frozenset(names) for stage, names in raw.items()}


def phase_to_stage() -> Dict[str, str]:
    """从 `gen_block_flow_spec.py:24` 的 `PHASE_TO_STAGE` 取阶段映射。"""
    src = load_text(GEN_SCRIPT_REL, "GEN_SCRIPT")
    raw = _literal_assign(src, "PHASE_TO_STAGE", GEN_SCRIPT_REL)
    if not isinstance(raw, dict) or len(raw) != 3:
        raise SpecUnparsable(f"ANCHOR_BAD_SHAPE: PHASE_TO_STAGE @ {GEN_SCRIPT_REL}")
    return dict(raw)


def derivation_docstring() -> str:
    """取 `gen_block_flow_spec.py` 的模块 docstring（规则文本正本）。"""
    src = load_text(GEN_SCRIPT_REL, "GEN_SCRIPT")
    tree = ast.parse(src)
    doc = ast.get_docstring(tree)
    if doc is None:
        raise AnchorStale(f"ANCHOR_STALE: GEN_SCRIPT_DOCSTRING @ {GEN_SCRIPT_REL}")
    return doc


def block_schema() -> Dict[str, Any]:
    """取块元数据 schema（`pipeline_block.schema.json`）。"""
    schema = load_json_anchor(BLOCK_SCHEMA_REL, "BLOCK_SCHEMA")
    if not isinstance(schema, dict) or "properties" not in schema:
        raise SpecUnparsable(f"ANCHOR_BAD_SHAPE: BLOCK_SCHEMA @ {BLOCK_SCHEMA_REL}")
    return schema


def registry() -> Dict[str, Any]:
    """取端口注册表（独立 Oracle 真值来源）。"""
    reg = load_json_anchor(REGISTRY_REL, "REGISTRY")
    if not isinstance(reg, dict) or not isinstance(reg.get("modules"), list):
        raise SpecUnparsable(f"ANCHOR_BAD_SHAPE: REGISTRY @ {REGISTRY_REL}")
    return reg


def repo_spec() -> Dict[str, Any]:
    """取仓内块流规格（**被测对象**）。负例通过 monkeypatch 本函数换注入副本。"""
    doc = load_json_anchor(SPEC_REL, "SPEC")
    validate_spec_shape(doc)
    return doc


def load_spec_from(path: str) -> Dict[str, Any]:
    """从任意 JSON 文件读块流规格（负例用临时目录副本，绝不写仓内留证）。"""
    if not os.path.isfile(path):
        raise AnchorStale(f"ANCHOR_STALE: INJECTED_SPEC {path}")
    try:
        with open(path, "r", encoding="utf-8") as fh:
            doc = json.load(fh)
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise SpecUnparsable(f"ANCHOR_UNPARSABLE: INJECTED_SPEC {path} :: {exc}") from exc
    validate_spec_shape(doc)
    return doc


def validate_spec_shape(doc: Any) -> None:
    """块流规格结构守卫：缺 `nodes` / `blocks` / 必需键即判红（fail-closed）。"""
    if not isinstance(doc, dict):
        raise SpecUnparsable("ANCHOR_BAD_SHAPE: SPEC :: 顶层不是 object")
    for key in ("nodes", "blocks"):
        if key not in doc:
            raise SpecUnparsable(f"ANCHOR_MISSING_FIELD: SPEC.{key}")
        if not isinstance(doc[key], list):
            raise SpecUnparsable(f"ANCHOR_BAD_SHAPE: SPEC.{key} 不是 array")
    for idx, node in enumerate(doc["nodes"]):
        for key in ("module_id", "stage", "reads", "writes"):
            if key not in node:
                raise SpecUnparsable(f"ANCHOR_MISSING_FIELD: SPEC.nodes[{idx}].{key}")
        if not isinstance(node["reads"], list) or not isinstance(node["writes"], list):
            raise SpecUnparsable(f"ANCHOR_BAD_SHAPE: SPEC.nodes[{idx}].reads/writes 不是 array")
    for idx, blk in enumerate(doc["blocks"]):
        for key in FLOW_BLOCK_FIELDS:
            if key not in blk:
                raise SpecUnparsable(f"ANCHOR_MISSING_FIELD: SPEC.blocks[{idx}].{key}")
        if not isinstance(blk["produced_by"], list) or not isinstance(blk["consumed_by"], list):
            raise SpecUnparsable(f"ANCHOR_BAD_SHAPE: SPEC.blocks[{idx}].produced_by/consumed_by")


def guard_zero_object(doc: Dict[str, Any]) -> None:
    """零对象守卫（`VALIDATION_EVIDENCE.md:170-176` 两条中的第 1 条）。

    `inputs` 声明可以为空或假的，所以守卫不是「声明非空」而是**实算对象数 > 0**。
    空 `blocks` / 空 `nodes` 一律判红，**空集不得判绿**。
    """
    n_nodes = len(doc.get("nodes") or [])
    n_blocks = len(doc.get("blocks") or [])
    if n_nodes <= 0 or n_blocks <= 0:
        raise AssertionError(
            "ZERO_OBJECT_GUARD: 实算对象数为 0（VALIDATION_EVIDENCE.md:172 第 1 条）"
            f"：nodes={n_nodes} blocks={n_blocks}；空集必须判红，不得判绿"
        )


def checked_spec() -> Dict[str, Any]:
    """正例入口：`repo_spec()` + 结构守卫 + 零对象守卫。"""
    doc = repo_spec()
    guard_zero_object(doc)
    return doc


def object_counts(doc: Dict[str, Any]) -> Dict[str, int]:
    """零对象守卫第 2 条要用的 `n_objects`（`VALIDATION_EVIDENCE.md:173`）。"""
    blocks = doc.get("blocks") or []
    n_edges = sum(len(b["consumed_by"]) for b in blocks)
    return {
        "n_nodes": len(doc.get("nodes") or []),
        "n_blocks": len(blocks),
        "n_stages": len({b["stage"] for b in blocks}),
        "n_prod_consume_edges": n_edges,
        "n_modules": len({n["module_id"] for n in (doc.get("nodes") or [])}),
    }


# ---------------------------------------------------------------------------
# §2 独立 Oracle：从注册表重算节点与块级统计（来源 = 规则文本 + 端口事实）
# ---------------------------------------------------------------------------

def derive_nodes(reg: Dict[str, Any]) -> List[Dict[str, Any]]:
    """从注册表重算 `nodes` 数组（块名 = 端口名，不发明新名）。

    排序键 = `(stage 名, 该 module_id 在注册表 modules 数组里的出现序)`，
    与 `gen_block_flow_spec.py:50` 的 `nodes.sort(key=...)` 同构；stage 名按字典序比较
    （Python 元组比较默认行为，与派生脚本一致）。
    """
    mapping = phase_to_stage()
    order = {m["module_id"]: i for i, m in enumerate(reg["modules"])}
    nodes: List[Dict[str, Any]] = []
    for module in reg["modules"]:
        stage = mapping.get(module["phase"])
        if stage is None:
            raise SpecUnparsable(
                f"ANCHOR_BAD_SHAPE: REGISTRY.modules[{module['module_id']}].phase "
                f"={module['phase']!r} 不在 PHASE_TO_STAGE"
            )
        for op in module["operations"]:
            reads = [p["name"] for p in op["ports"] if p["direction"] == "input"]
            writes = [p["name"] for p in op["ports"] if p["direction"] == "output"]
            nodes.append({
                "module_id": module["module_id"],
                "stage": stage,
                "operation": op["operation"],
                "entry": op["entry"],
                "reads": reads,
                "writes": writes,
            })
    nodes.sort(key=lambda n: (n["stage"], order[n["module_id"]]))
    return nodes


def production_and_consumption(nodes: Sequence[Dict[str, Any]]) -> Tuple[Dict, Dict]:
    """按**阶段作用域**统计生产者 / 消费者（`gen_block_flow_spec.py:52-61`）。"""
    produced: Dict[Tuple[str, str], List[str]] = {}
    consumed: Dict[Tuple[str, str], List[str]] = {}
    for node in nodes:
        stage = node["stage"]
        for name in node["writes"]:
            produced.setdefault((stage, name), []).append(node["module_id"])
        for name in node["reads"]:
            consumed.setdefault((stage, name), []).append(node["module_id"])
    return produced, consumed


def oracle_lifecycle(n_producers: int, n_consumers: int, is_stage_terminal: bool) -> str:
    """按 `gen_block_flow_spec.py:7-12` 的 5 条规则独立定生命周期。

    **优先级裁决（登记项 REG-01）**：docstring 的 5 条是并列条目、没有写「先判哪条」，
    因此多条同时命中时存在歧义。本 Oracle 采用的优先级是
    「无生产者 → 阶段终产物 → 消费者数」，理由：

    1. 「无生产者 ⇒ EXTERNAL_IN」是一条**溯源事实**（块根本不是本阶段产的），
       与 `PIPELINE_BLOCK.md:21`「`config_path`（阶段外部输入，本阶段无生产者）」同向；
    2. 「阶段终产物」是**发布义务**，它决定块必须活到阶段末并以磁盘产品发布
       （`PIPELINE_BLOCK.md:66`），优先于「消费者数」的内存占用推断；
    3. 若按 docstring 的**书写顺序**逐条 first-match（`>= 2 ⇒ STAGE` 排第一），
       `(mosaic, frame_hips)` 与 `(export, mosaic_hips)` 这两个「无生产者但有 3–5 个消费者」
       的跨阶段 HiPS 输入会被判成 STAGE，与合同「跨阶段唯一载体 = HiPS 产品树、
       不跨阶段共享内存」（`PIPELINE_BLOCK.md:20`）矛盾。

    `precedence_sensitive_keys()` 把受该裁决影响的块列出来，供人复核。
    """
    if n_producers == 0:
        return "EXTERNAL_IN"
    if is_stage_terminal:
        return "EXTERNAL_OUT"
    if n_consumers >= 2:
        return "STAGE"
    if n_consumers == 0:
        return "STAGE"
    return "SHORT"


def precedence_sensitive_keys(nodes: Sequence[Dict[str, Any]]) -> List[Tuple[str, str]]:
    """列出「按 docstring 书写顺序 first-match 会得出不同结论」的 `(stage, block)`。"""
    produced, consumed = production_and_consumption(nodes)
    terminals = stage_terminals()
    out: List[Tuple[str, str]] = []
    for key in sorted(produced.keys() | consumed.keys()):
        stage, name = key
        n_prod = len(produced.get(key, []))
        n_cons = len(consumed.get(key, []))
        adopted = oracle_lifecycle(n_prod, n_cons, name in terminals.get(stage, frozenset()))
        literal = _literal_order_lifecycle(n_prod, n_cons, name in terminals.get(stage, frozenset()))
        if adopted != literal:
            out.append(key)
    return out


def _literal_order_lifecycle(n_producers: int, n_consumers: int, is_terminal: bool) -> str:
    """按 docstring 5 条的**书写顺序** first-match 求值（仅用于暴露歧义，不作判据）。"""
    if n_consumers >= 2:
        return "STAGE"                                  # 规则 1（docstring 第 1 条）
    if n_consumers == 0 and is_terminal:
        return "EXTERNAL_OUT"                           # 规则 2
    if n_consumers == 0 and not is_terminal:
        return "STAGE"                                  # 规则 3
    if n_consumers == 1:
        return "SHORT"                                  # 规则 4
    return "EXTERNAL_IN" if n_producers == 0 else "?"   # 规则 5


def spec_index(doc: Dict[str, Any]) -> Dict[Tuple[str, str], Dict[str, Any]]:
    """`(stage, block) → 块条目`。重复键即判红（同一块被重复声明）。"""
    out: Dict[Tuple[str, str], Dict[str, Any]] = {}
    for entry in doc["blocks"]:
        key = (entry["stage"], entry["block"])
        if key in out:
            raise SpecUnparsable(f"ANCHOR_DUPLICATE_BLOCK_KEY: SPEC {key}")
        out[key] = entry
    return out


def stage_positions(doc: Dict[str, Any]) -> Dict[Tuple[str, str], int]:
    """按 `nodes` 数组的**声明序**给每阶段每个节点一个位置 `pos`（从 0 起）。

    `pos` 是「块流规格的节点声明序」，供 A7 的释放点 / 存活跨度判定使用。
    它与 `PIPELINE_BLOCK.md:22` 的「节点序由端口图唯一确定、须是该 DAG 的拓扑序」是
    同一事实的两个读面：IR-C5（`PIPELINE_BLOCK.md:113`）负责保真，本层只在其之上
    判「释放点是否及时」。
    """
    pos: Dict[Tuple[str, str], int] = {}
    counters: Dict[str, int] = {}
    for node in doc["nodes"]:
        stage = node["stage"]
        idx = counters.get(stage, 0)
        key = (stage, node["module_id"])
        if key in pos:
            raise SpecUnparsable(f"ANCHOR_DUPLICATE_NODE: SPEC {key}")
        pos[key] = idx
        counters[stage] = idx + 1
    return pos


def node_derived_edges(doc: Dict[str, Any]) -> Tuple[Dict, Dict]:
    """**从 `nodes` 数组独立重建**生产者 / 消费者（不复用 `blocks` 声明）。

    A4（重复生产）与 A7（释放点及时性）必须走这个面，才能与 A2 / A1 走 `blocks` 面的
    判据区分开，使负例能各自命中「该判定自身的判定逻辑」（`VALIDATION_EVIDENCE.md:193`）。
    """
    return production_and_consumption(doc["nodes"])


def _reader_positions(doc: Dict[str, Any], key: Tuple[str, str]) -> List[int]:
    """`(stage, block)` 在**节点面**上的全部读者位置（声明序），按 pos 升序。

    刻意不复用 `blocks[*].consumed_by`：A7 的「释放点之后无读者」必须把声明面与
    节点面对照，否则该子句是 `max(consumed_by) == max(consumed_by)` 的恒真比较
    （`TEST.md:26`「恒真的比较没有证据资格」）。
    """
    stage, name = key
    out: List[int] = []
    counter = 0
    for node in doc["nodes"]:
        if node["stage"] == stage:
            if name in node["reads"]:
                out.append(counter)
            counter += 1
    return sorted(out)


def _fmt(violations: Sequence[str], limit: int = 10) -> str:
    """判红输出格式：检查名 + 对象计数 + 前 10 条对象（`VALIDATION_EVIDENCE.md:241`）。"""
    head = "\n".join(f"  [{i + 1}] {v}" for i, v in enumerate(violations[:limit]))
    more = "" if len(violations) <= limit else f"\n  … 另有 {len(violations) - limit} 条"
    return f"判红对象数={len(violations)}\n{head}{more}"


# ---------------------------------------------------------------------------
# §2.5 观察项（非判红面）
#
# 以下三组形态**按合同是合法的**，本层把它们显式枚举出来写进用例输出与 README，
# 使「判绿但值得负责人裁定」的形态不被静默吞掉。**它们不是判红项**：
#   - 零消费者却 STAGE 的块：按 `gen_block_flow_spec.py:10`「消费者数 == 0 且非终产物
#     ⇒ STAGE」是规则**直接支持**的结论；`PIPELINE_BLOCK.md:36`「consumers 可空 ⇒ 终态块」
#     把它定义成终态块。口径分歧登记见 README REG-02。
#   - EXTERNAL_OUT 且有本阶段内部消费者：`gen_block_flow_spec.py:78` 注释逐字
#     「阶段终产物：必须发布（**也可被本阶段内部消费**）」。
#   - 同名块在不同阶段是两个不同的块：`gen_block_flow_spec.py:52-53` 注释逐字
#     「**按阶段作用域**：阶段间只通过磁盘产品交换，同名块在不同阶段是不同块」。
# ---------------------------------------------------------------------------

def observe_zero_consumer_stage_blocks(doc: Dict[str, Any]) -> List[Tuple[str, str, str]]:
    """观察项：零消费者、`lifecycle=STAGE`、且不在合同点名终产物集合里的块。"""
    terminals = stage_terminals()
    out = []
    for entry in doc["blocks"]:
        key = (entry["stage"], entry["block"])
        if (not entry["consumed_by"] and entry["lifecycle"] == "STAGE"
                and entry["block"] not in terminals.get(entry["stage"], frozenset())):
            out.append((key[0], key[1], entry["produced_by"][0] if entry["produced_by"] else "<无>"))
    return sorted(out)


def observe_external_out_with_consumer(doc: Dict[str, Any]) -> List[Tuple[str, str, int]]:
    """观察项：`EXTERNAL_OUT` 但本阶段仍消费它的块（合同允许，见函数上方注释）。"""
    return sorted(
        (e["stage"], e["block"], len(e["consumed_by"]))
        for e in doc["blocks"]
        if e["lifecycle"] == "EXTERNAL_OUT" and e["consumed_by"]
    )


def observe_cross_stage_same_name(doc: Dict[str, Any]) -> Dict[str, List[Tuple[str, str]]]:
    """观察项：同名块出现在多个阶段（跨阶段磁盘产品，不是重复生产）。"""
    by_name: Dict[str, List[Tuple[str, str]]] = {}
    for entry in doc["blocks"]:
        by_name.setdefault(entry["block"], []).append((entry["stage"], entry["lifecycle"]))
    return {name: sorted(v) for name, v in sorted(by_name.items()) if len(v) > 1}


# ---------------------------------------------------------------------------
# §3 判定逻辑 A1–A9（每条返回违规清单；空清单 = 绿）
# ---------------------------------------------------------------------------

def judge_lifecycle_from_documented_rules(doc: Dict[str, Any], reg: Dict[str, Any]) -> List[str]:
    """A1：按 docstring 的 5 条派生规则**独立重算**生命周期，与规格声明逐项精确一致。

    比较面：`(stage, block)` 键集合 + 每键的 `lifecycle` / `produced_by` / `consumed_by`
    + 整个 `nodes` 数组。落 `TEST.md:46` 精确一致档。
    """
    guard_zero_object(doc)
    bad: List[str] = []

    # (0) 规则文本本身存活：Oracle 的真值来源必须还在
    doc_text = derivation_docstring()
    for line in RULE_LINES:
        if line not in doc_text:
            bad.append(f"RULE_TEXT_STALE: gen_block_flow_spec.py docstring 缺规则行 {line!r}")

    expected_nodes = derive_nodes(reg)
    if doc["nodes"] != expected_nodes:
        exp_idx = {(n["stage"], n["module_id"]): n for n in expected_nodes}
        got_idx = {(n["stage"], n["module_id"]): n for n in doc["nodes"]}
        for key in sorted(set(exp_idx) | set(got_idx)):
            if exp_idx.get(key) != got_idx.get(key):
                bad.append(f"NODE_MISMATCH: {key} 期望 {exp_idx.get(key)} 实得 {got_idx.get(key)}")

    produced, consumed = production_and_consumption(expected_nodes)
    terminals = stage_terminals()
    expected_lc: Dict[Tuple[str, str], str] = {}
    expected_edges: Dict[Tuple[str, str], Tuple[List[str], List[str]]] = {}
    for key in sorted(produced.keys() | consumed.keys()):
        stage, name = key
        expected_lc[key] = oracle_lifecycle(
            len(produced.get(key, [])), len(consumed.get(key, [])),
            name in terminals.get(stage, frozenset()),
        )
        expected_edges[key] = (produced.get(key, []), consumed.get(key, []))

    index = spec_index(doc)
    for key in sorted(set(expected_lc) | set(index)):
        if key not in index:
            bad.append(f"BLOCK_MISSING_IN_SPEC: {key}（注册表重算出该块，规格未声明）")
            continue
        if key not in expected_lc:
            bad.append(f"BLOCK_ONLY_IN_SPEC: {key}（规格声明了该块，注册表重算不出来）")
            continue
        entry = index[key]
        if entry["lifecycle"] != expected_lc[key]:
            bad.append(
                f"LIFECYCLE_MISMATCH: {key} 规则重算={expected_lc[key]} "
                f"规格声明={entry['lifecycle']}"
            )
        exp_p, exp_c = expected_edges[key]
        if entry["produced_by"] != exp_p:
            bad.append(f"PRODUCED_BY_MISMATCH: {key} 重算={exp_p} 规格={entry['produced_by']}")
        if entry["consumed_by"] != exp_c:
            bad.append(f"CONSUMED_BY_MISMATCH: {key} 重算={exp_c} 规格={entry['consumed_by']}")
    return bad


def judge_single_producer(doc: Dict[str, Any]) -> List[str]:
    """A2：每个被消费**且在本阶段声明了生产者**的块，有且仅有一个生产者。

    作用域声明（README「作用域声明」一节）：`PIPELINE_BLOCK.md:41` 的「有且仅有一个
    生产者」在**阶段作用域**下判定；本阶段无生产者的合法类别是 `EXTERNAL_IN`
    （`PIPELINE_BLOCK.md:21`、`gen_block_flow_spec.py:12`）。「被消费但无生产者且未声明
    EXTERNAL_IN」的那一支归 A3 判，不在本判定里重复计。
    """
    guard_zero_object(doc)
    bad: List[str] = []
    pos = stage_positions(doc)
    for entry in doc["blocks"]:
        key = (entry["stage"], entry["block"])
        if entry["lifecycle"] == "EXTERNAL_IN":
            # 本阶段无生产者是 EXTERNAL_IN 的**定义**（`PIPELINE_BLOCK.md:21`、
            # `gen_block_flow_spec.py:12`），不落在「有且仅有一个生产者」的红侧；
            # 该类的自洽性归 A3 判。
            continue
        if len(entry["produced_by"]) != 1:
            bad.append(
                f"PRODUCER_NOT_UNIQUE: {key} 生产者数={len(entry['produced_by'])} "
                f"{entry['produced_by']}"
            )
            continue
        producer = entry["produced_by"][0]
        if (key[0], producer) not in pos:
            bad.append(
                f"PRODUCER_NOT_IN_STAGE: {key} 生产者 {producer!r} 不在本阶段节点集合里"
            )
    return bad


def judge_no_dangling_consumption(doc: Dict[str, Any]) -> List[str]:
    """A3：不存在「被某节点消费但不可解析」的块（`PIPELINE_BLOCK.md:42` 第 2 条）。

    两条红侧：
    - **消费不存在的块**：`nodes[*].reads` 里的块名在本阶段没有 `blocks` 条目；
    - **被消费但无生产者且未声明 EXTERNAL_IN**：既不是本阶段产物，也不是合法的外部输入，
      在内存管线里无从取得。
    另附 EXTERNAL_IN 自洽两条（声明为外部输入的块必须确实零生产者、且本阶段确实消费它），
    否则「外部输入」可以拿来给一个悬空块开免票。
    """
    guard_zero_object(doc)
    bad: List[str] = []
    index = spec_index(doc)
    produced, consumed = node_derived_edges(doc)

    for stage, name in sorted(consumed.keys()):
        if (stage, name) not in index:
            bad.append(f"CONSUME_NONEXISTENT_BLOCK: ({stage}, {name}) 被节点消费但无块声明")

    for entry in doc["blocks"]:
        key = (entry["stage"], entry["block"])
        n_prod = len(entry["produced_by"])
        if entry["lifecycle"] == "EXTERNAL_IN":
            if n_prod != 0:
                bad.append(f"EXTERNAL_IN_HAS_PRODUCER: {key} 声明外部输入却有 {n_prod} 个生产者")
            if not entry["consumed_by"]:
                bad.append(f"EXTERNAL_IN_NOT_CONSUMED: {key} 声明外部输入但本阶段无消费者")
            continue
        if n_prod == 0 and entry["consumed_by"]:
            bad.append(
                f"DANGLING_CONSUMPTION: {key} 被 {len(entry['consumed_by'])} 个节点消费、"
                f"本阶段无生产者、且未声明 EXTERNAL_IN"
            )
    return bad


def judge_no_duplicate_production(doc: Dict[str, Any]) -> List[str]:
    """A4：同一 `(stage, block)` 不被两个节点重复生产（`PIPELINE_BLOCK.md:43` 第 3 条）。

    本判定的输入面是 **`nodes[*].writes`**，不读 `blocks[*].produced_by`，因此与 A2
    的判定逻辑彼此独立：改 `blocks` 面只命中 A2，改 `nodes` 面只命中 A4。
    """
    guard_zero_object(doc)
    bad: List[str] = []
    produced, _ = node_derived_edges(doc)
    index = spec_index(doc)
    for key, producers in sorted(produced.items()):
        if len(producers) > 1:
            bad.append(f"DUPLICATE_PRODUCTION: {key} 被 {len(producers)} 个节点生产 {producers}")
        entry = index.get(key)
        if entry is not None and entry["produced_by"] != producers:
            bad.append(
                f"PRODUCER_FACE_DRIFT: {key} nodes 面重算={producers} "
                f"blocks 面声明={entry['produced_by']}"
            )
    return bad


def judge_lifecycle_consumer_span(doc: Dict[str, Any]) -> List[str]:
    """A5：声明的 `lifecycle` 与实际消费者跨度一致（`PIPELINE_BLOCK.md:44` 第 4 条）。

    只判**有生产者、非阶段终产物**的块（本阶段产物）：消费者数 1 ⇒ SHORT，
    消费者数 ≥2 ⇒ STAGE。零消费者的那一支是「终态块」，归 A8 判
    （`PIPELINE_BLOCK.md:36`「consumers 可空 ⇒ 终态块」）。
    """
    guard_zero_object(doc)
    bad: List[str] = []
    terminals = stage_terminals()
    for entry in doc["blocks"]:
        key = (entry["stage"], entry["block"])
        if not entry["produced_by"]:
            continue                                   # 外部输入：不是本阶段产物
        if entry["block"] in terminals.get(entry["stage"], frozenset()):
            continue                                   # 阶段终产物：发布义务优先
        n_cons = len(entry["consumed_by"])
        if n_cons == 0:
            continue                                   # 终态块：归 A8
        expected = "SHORT" if n_cons == 1 else "STAGE"
        if entry["lifecycle"] != expected:
            bad.append(
                f"CONSUMER_SPAN_MISMATCH: {key} 消费者数={n_cons} ⇒ 应为 {expected}，"
                f"规格声明 {entry['lifecycle']}"
            )
    return bad


def judge_stage_terminals(doc: Dict[str, Any], reg: Dict[str, Any]) -> List[str]:
    """A6：阶段终产物集合逐字等于合同点名的集合，且跨阶段只走 HiPS 产品树。

    - 集合来源 = `gen_block_flow_spec.py:30-32` 的 `STAGE_TERMINALS` 字面量
      （用 `ast` 从源码提取）+ `docs/ACSD_DESIGN.md:364-379` 的三命令合同；
    - 每个终产物名必须是注册表里的**真实端口名**（`gen_block_flow_spec.py:5`
      「不发明新名」）；
    - 跨阶段唯一载体 = HiPS 产品树（`PIPELINE_BLOCK.md:20-21`）：被其它阶段消费的
      产物名逐个查注册表 `carrier` 必须是 `hips_product_tree`，且逐阶段只允许
      `mosaic ← frame_hips`、`export ← mosaic_hips`。
    """
    guard_zero_object(doc)
    bad: List[str] = []
    terminals = stage_terminals()

    declared: Dict[str, set] = {stage: set() for stage in terminals}
    for entry in doc["blocks"]:
        if entry["lifecycle"] == "EXTERNAL_OUT":
            declared.setdefault(entry["stage"], set()).add(entry["block"])
    for stage in sorted(set(terminals) | set(declared)):
        want = set(terminals.get(stage, frozenset()))
        got = declared.get(stage, set())
        for name in sorted(want - got):
            bad.append(f"TERMINAL_NOT_EXTERNAL_OUT: ({stage}, {name}) 合同点名的终产物未标 EXTERNAL_OUT")
        for name in sorted(got - want):
            bad.append(f"EXTRA_EXTERNAL_OUT: ({stage}, {name}) 不在合同点名的终产物集合里")

    # 终产物必须是真实端口名
    port_names = _registry_port_names(reg)
    for stage, names in sorted(terminals.items()):
        for name in sorted(names):
            if name not in port_names:
                bad.append(f"TERMINAL_NOT_A_PORT: ({stage}, {name}) 注册表里没有同名端口")

    # 跨阶段载体：判据的事实源是**注册表的 phase 归属**（端口在哪个 phase 被产出、
    # 在哪个 phase 被消费），不是规格的 `produced_by`/`consumed_by`——后者只给
    # module_id，且「跨阶段」这件事在规格里被折叠成了 EXTERNAL_IN。
    mapping = phase_to_stage()
    out_phases, in_phases, carriers = _registry_phase_facts(reg)
    stage_order = {mapping[p]: i for i, p in enumerate(sorted(mapping))}
    for name in sorted(out_phases):
        prod_stages = {mapping[p] for p in out_phases[name]}
        cons_stages = {mapping[p] for p in in_phases.get(name, set())}
        downstream = {
            s for s in cons_stages
            if any(stage_order.get(s, -1) > stage_order.get(p, -1) for p in prod_stages)
        }
        if not downstream:
            continue
        got_carriers = carriers.get(name, set())
        if got_carriers != {"hips_product_tree"}:
            bad.append(
                f"CROSS_STAGE_CARRIER: 产物 {name!r} 在阶段 {sorted(downstream)} 被消费（跨阶段），"
                f"注册表 carrier={sorted(got_carriers) or '未找到'}，"
                f"必须且只能是 hips_product_tree（PIPELINE_BLOCK.md:20-21）"
            )
        expected_producer_stage = any(
            name in terminals.get(p, frozenset()) for p in prod_stages
        )
        if not expected_producer_stage:
            bad.append(
                f"UNEXPECTED_CROSS_STAGE_FLOW: 产物 {name!r} 由阶段 {sorted(prod_stages)} 的"
                f"节点产出（该阶段内中间产物，不是合同点名终产物），却被阶段 "
                f"{sorted(downstream)} 跨阶段消费；跨阶段只走合同点名的 HiPS 终产物"
            )
    return bad


def _registry_phase_facts(reg: Dict[str, Any]) -> Tuple[Dict[str, Set[str]], Dict[str, Set[str]], Dict[str, Set[str]]]:
    """注册表按 phase 统计每个端口名：被产出的 phase、被消费的 phase、carrier 集合。"""
    out_phases: Dict[str, Set[str]] = {}
    in_phases: Dict[str, Set[str]] = {}
    carriers: Dict[str, Set[str]] = {}
    for module in reg["modules"]:
        for op in module["operations"]:
            for port in op["ports"]:
                name, phase, carrier = port["name"], module["phase"], port["carrier"]
                carriers.setdefault(name, set()).add(carrier)
                if port["direction"] == "output":
                    out_phases.setdefault(name, set()).add(phase)
                else:
                    in_phases.setdefault(name, set()).add(phase)
    return out_phases, in_phases, carriers


def _registry_port_names(reg: Dict[str, Any]) -> set:
    names: set = set()
    for module in reg["modules"]:
        for op in module["operations"]:
            for port in op["ports"]:
                names.add(port["name"])
    return names


def _registry_carriers(reg: Dict[str, Any]) -> Dict[str, Set[str]]:
    out: Dict[str, Set[str]] = {}
    for module in reg["modules"]:
        for op in module["operations"]:
            for port in op["ports"]:
                out.setdefault(port["name"], set()).add(port["carrier"])
    return out


def judge_prompt_release(doc: Dict[str, Any]) -> List[str]:
    """A7（旧块是否及时释放）：本单重点判定。

    四条独立红侧：

    1. **声明面与节点面一致**：`blocks[*].consumed_by` / `produced_by` 必须逐项等于从
       `nodes[*].reads` / `writes` 独立重建的结果。这条直接抓「声明的释放点之后仍有节点
       在读它」——`SHORT` 块被声明在 `pos=k` 释放、`pos>k` 的节点却出现在 `reads` 里，
       即释放后使用（use-after-release）。
    2. **拓扑不倒置**：任何块的最后一个消费者 `pos` 必须严格晚于其生产者 `pos`
       （`PIPELINE_BLOCK.md:69-73` 负例第 1、2 条）。
    3. **SHORT 的释放点 = 最后一个消费者的 pos**：即时消费即销毁
       （`PIPELINE_BLOCK.md:54`、`ACSD_DESIGN.md:384`）。
    4. **多消费者 STAGE 确实跨节点存活**：最后一个消费者 `pos` 严格晚于生产者 `pos`，
       既不是「生产即死」也不是「无用存活到阶段末」。
    另：每个 `EXTERNAL_OUT` 块必须确实标成 `EXTERNAL_OUT` 且在合同点名集合里
    （阶段末发布）；零消费者块必须是终态块（`STAGE` 或 `EXTERNAL_OUT`）。
    """
    guard_zero_object(doc)
    bad: List[str] = []
    pos = stage_positions(doc)
    index = spec_index(doc)
    terminals = stage_terminals()
    node_prod, node_cons = node_derived_edges(doc)

    for entry in doc["blocks"]:
        key = (entry["stage"], entry["block"])
        # (1) 声明面 ↔ 节点面逐项一致（释放点及时性的载体）
        if entry["consumed_by"] != node_cons.get(key, []):
            bad.append(
                f"READ_AFTER_RELEASE_OR_UNDECLARED_READ: {key} 声明消费者={entry['consumed_by']} "
                f"但 nodes 面重算={node_cons.get(key, [])}（声明的释放点之后仍有节点读它）"
            )
        if entry["produced_by"] != node_prod.get(key, []):
            bad.append(
                f"PRODUCER_FACE_DRIFT: {key} 声明生产者={entry['produced_by']} "
                f"但 nodes 面重算={node_prod.get(key, [])}"
            )
        # (2) 拓扑不倒置 + (3)(4) 存活跨度
        if entry["produced_by"] and entry["consumed_by"]:
            p_pos = pos[(key[0], entry["produced_by"][0])]
            c_pos = [pos[(key[0], c)] for c in entry["consumed_by"]]
            last = max(c_pos)
            if last <= p_pos:
                bad.append(
                    f"TOPOLOGY_INVERTED: {key} 最后一个消费者 pos={last} 未晚于生产者 pos={p_pos}"
                )
            if entry["lifecycle"] == "SHORT":
                # 释放点 = 最后一个消费者的 pos，且**没有任何 pos 更晚的节点读它**。
                # 两半都用「节点面」实算的读者位置与「声明面」的消费者位置对照；
                # 若两边都从 `consumed_by` 算，该子句退化成 `max(x) == max(x)` 的恒真比较。
                declared_release = max(pos[(key[0], c)] for c in entry["consumed_by"])
                actual_readers = _reader_positions(doc, key)
                actual_release = max(actual_readers) if actual_readers else None
                if actual_release != declared_release:
                    bad.append(
                        f"SHORT_RELEASE_POINT: {key} 声明释放点 pos={declared_release} "
                        f"≠ 节点面最后一个读者 pos={actual_release}（读者位置 {actual_readers}）"
                    )
                late = [r for r in actual_readers if r > declared_release]
                if late:
                    bad.append(
                        f"READ_AFTER_RELEASE: {key} 声明在 pos={declared_release} 释放，"
                        f"但 pos={late} 的节点仍在读它（释放后使用）"
                    )
            if entry["lifecycle"] == "STAGE" and len(entry["consumed_by"]) >= 2:
                # 多消费者必须**跨节点**：至少两个不同的节点位置。若所有消费者落在
                # 同一节点，它就不是「活到阶段末供后续节点复用」，声明 STAGE 是无用存活。
                consumer_pos = {pos[(key[0], c)] for c in entry["consumed_by"]}
                if len(consumer_pos) < 2:
                    bad.append(
                        f"STAGE_NOT_CROSS_NODE: {key} 多消费者 STAGE 块的消费者全部落在同一节点"
                        f"（pos={sorted(consumer_pos)}），不构成跨节点存活"
                    )
        # (5) 阶段末发布
        if entry["lifecycle"] == "EXTERNAL_OUT":
            if entry["block"] not in terminals.get(entry["stage"], frozenset()):
                bad.append(
                    f"EXTERNAL_OUT_NOT_NAMED: {key} 标成 EXTERNAL_OUT 但不在合同点名的终产物集合里"
                )
        # (6) 零消费者块必须是终态块
        if not entry["consumed_by"] and entry["lifecycle"] not in ("STAGE", "EXTERNAL_OUT"):
            bad.append(
                f"ZERO_CONSUMER_NOT_TERMINAL: {key} 零消费者却声明 {entry['lifecycle']}，"
                f"按 PIPELINE_BLOCK.md:36 应为终态块（STAGE / EXTERNAL_OUT）"
            )
    return bad


def judge_stage_residual_zero(doc: Dict[str, Any]) -> List[str]:
    """A8：阶段结束时块残留数 = 0（`PIPELINE_BLOCK.md:55`）。

    **判定口径（README「作用域声明」一节，重要）**：本判定**不**采用
    「所有非终态块都至少有一个消费者」的字面写法。合同自身在 `PIPELINE_BLOCK.md:36`
    把「`consumers` 可空」定义为**终态块**的充分条件，所以「零消费者 ∧ 非终态」这个合取
    在合同的字面读法下**恒不成立**，写成用例就是一条永远绿的空判据（`TEST.md:26`
    「恒真的比较没有证据资格」）。本判定取三条可红的读法：

    1. **零消费者 ⇒ 必须是终态块**：零消费者的块的 `lifecycle` 必须是 `STAGE`
       （终态中间产物，随阶段发布，`gen_block_flow_spec.py:82`）或 `EXTERNAL_OUT`
       （合同点名的阶段终产物）；不得是 `SHORT` 或 `EXTERNAL_IN`。
    2. **作用域闭合**：每个块的 `produced_by` / `consumed_by` 必须都是**本阶段**节点；
       跨阶段的块只允许是 HiPS 产品树那两条流（`PIPELINE_BLOCK.md:17,20-21,55`
       「命名块不跨阶段」「跨阶段只走磁盘产品」）。
    3. **残留面不得留 SHORT/EXTERNAL_IN 悬空块**：本阶段无任何消费者的块里，
       不得混进声明为 `SHORT`（会被调度器提前回收、产物却没人取）或 `EXTERNAL_IN`
       （被当外部输入、实际是本阶段产物）的条目。
    """
    guard_zero_object(doc)
    bad: List[str] = []
    pos = stage_positions(doc)
    index = spec_index(doc)
    node_prod, _ = node_derived_edges(doc)

    for entry in doc["blocks"]:
        key = (entry["stage"], entry["block"])
        n_cons = len(entry["consumed_by"])
        # (1) 零消费者 ⇒ 终态块
        if n_cons == 0 and entry["lifecycle"] not in ("STAGE", "EXTERNAL_OUT"):
            bad.append(
                f"STAGE_RESIDUAL: {key} 有生产者、零消费者、却声明 {entry['lifecycle']}；"
                f"按 PIPELINE_BLOCK.md:36 零消费者即为终态块，阶段末将残留不销毁"
            )
        # (2) 作用域闭合：生产者/消费者必须在本阶段
        for role, mids in (("produced_by", entry["produced_by"]),
                           ("consumed_by", entry["consumed_by"])):
            for mid in mids:
                if (key[0], mid) not in pos:
                    bad.append(
                        f"CROSS_STAGE_REFERENCE: {key} 的 {role} 引用了非本阶段节点 {mid!r}"
                    )
        # (3) 零消费者的块必须确有生产者（否则它不是「残留」而是「悬空声明」）
        if n_cons == 0 and not entry["produced_by"]:
            bad.append(f"ORPHAN_TERMINAL: {key} 既无生产者也无消费者")
    return bad


def judge_block_metadata(doc: Dict[str, Any], schema: Dict[str, Any]) -> List[str]:
    """A9：块条目的字段集与取值域符合 `pipeline_block.schema.json` 的冻结形态。

    **口径声明（REG-03，重要）**：`stage_block_flow.json` 的顶层 `$id` 是
    `acsd.stage-block-flow/v1`，`pipeline_block.schema.json` 的 `$id` 是
    `acsd.pipeline-block/v1`，两者是**两份不同的产物**：前者是流拓扑文档，
    后者是块物理元数据文档。因此「块流规格整体符合 pipeline_block.schema.json」这条
    判据在仓库现状下**判红**（lifecycle 词表不相交、物理元数据字段不存在），本判定
    只断言两者**真正共有**的那部分形态，并把不相交的部分交给
    `test_..._SchemaLifecycleVocabularyDivergenceRegistration` 做漂移守卫。

    本判定覆盖：
    - 块条目的字段集恰为 `{stage, block, lifecycle, produced_by, consumed_by}`；
    - `block` 匹配 schema 里 `name` 的小写蛇形正则（正则从 schema 读，不在此复写）；
    - `lifecycle` 落在派生规则文本的 4 值词表内；
    - `produced_by` / `consumed_by` 是 string 数组（对应 schema `consumers` 的形态）；
    - `stage` 落在 `PHASE_TO_STAGE` 的值域内。
    """
    guard_zero_object(doc)
    bad: List[str] = []
    name_pattern = schema["properties"]["blocks"]["items"]["properties"]["name"]["pattern"]
    rx = re.compile(name_pattern)

    mapping = phase_to_stage()
    for entry in doc["blocks"]:
        key = (entry["stage"], entry["block"])
        fields = set(entry.keys())
        if fields != FLOW_BLOCK_FIELDS:
            bad.append(
                f"BLOCK_FIELD_SET: {key} 字段集={sorted(fields)} "
                f"应恰为 {sorted(FLOW_BLOCK_FIELDS)}"
            )
        if not isinstance(entry["block"], str) or not rx.match(entry["block"]):
            bad.append(
                f"BLOCK_NAME_PATTERN: {key} 块名不匹配 schema 的 {name_pattern!r}"
            )
        if entry["lifecycle"] not in DERIVED_LIFECYCLE_VOCAB:
            bad.append(
                f"LIFECYCLE_NOT_IN_DERIVED_VOCAB: {key} {entry['lifecycle']!r} "
                f"不在派生规则词表 {list(DERIVED_LIFECYCLE_VOCAB)}"
            )
        for field in ("produced_by", "consumed_by"):
            value = entry[field]
            if not isinstance(value, list) or any(not isinstance(x, str) for x in value):
                bad.append(f"NODE_ID_LIST_SHAPE: {key}.{field} 不是 string 数组")
        if entry["stage"] not in set(mapping.values()):
            bad.append(
                f"STAGE_NOT_IN_MAPPING: {key} stage={entry['stage']!r} 不在 PHASE_TO_STAGE 值域内"
            )
    return bad


def judge_non_degenerate(doc: Dict[str, Any], reg: Dict[str, Any]) -> List[str]:
    """B8：非退化下界。下界来源全部是正本文面，**无无来源魔数**（S5）。

    - **阶段数 = 3（精确）**：`docs/ACSD_DESIGN.md:364-379` 三个平级独立命令
      normalize / mosaic / export；`gen_block_flow_spec.py:24` 的 `PHASE_TO_STAGE`
      恰 3 键；`gen_block_flow_spec.py:64` 的阶段循环字面量恰 3 个。
    - **每阶段节点数 ≥ 1**：每个阶段至少有一个工序，否则阶段不存在。
    - **块数 ≥ 7**：`gen_block_flow_spec.py:30-32` 的 `STAGE_TERMINALS` 全集共
      7 个块（normalize 3 + mosaic 2 + export 2），由 `ACSD_DESIGN.md:375-378` 的
      三命令合同与 `PIPELINE_BLOCK.md:66` 的阶段终产物清单逐字点名。
    - **注册表模块数 ≥ 3**：每阶段至少一个节点 ⇒ 至少 3 个不同 module_id。
    - **生产→消费边数 ≥ 阶段数**：每个阶段至少一条生产→消费边，否则阶段无数据流。
    """
    guard_zero_object(doc)
    bad: List[str] = []
    counts = object_counts(doc)
    terminals = stage_terminals()
    mapping = phase_to_stage()

    expected_stages = len(mapping)                 # 3，来自 PHASE_TO_STAGE
    if counts["n_stages"] != expected_stages:
        bad.append(
            f"NONDEGENERATE_STAGES: 实算阶段数={counts['n_stages']}，"
            f"下界={expected_stages}（PHASE_TO_STAGE 的键数）"
        )
    n_terminals = sum(len(v) for v in terminals.values())
    if counts["n_blocks"] < n_terminals:
        bad.append(
            f"NONDEGENERATE_BLOCKS: 实算块数={counts['n_blocks']}，"
            f"下界={n_terminals}（STAGE_TERMINALS 全集）"
        )
    if counts["n_modules"] < expected_stages:
        bad.append(
            f"NONDEGENERATE_MODULES: 实算模块数={counts['n_modules']}，"
            f"下界={expected_stages}（每阶段至少一个工序节点）"
        )
    if counts["n_prod_consume_edges"] < expected_stages:
        bad.append(
            f"NONDEGENERATE_EDGES: 实算生产→消费边数={counts['n_prod_consume_edges']}，"
            f"下界={expected_stages}（每阶段至少一条数据流）"
        )
    per_stage_nodes: Dict[str, int] = {}
    for node in doc["nodes"]:
        per_stage_nodes[node["stage"]] = per_stage_nodes.get(node["stage"], 0) + 1
    for stage in sorted(per_stage_nodes):
        if per_stage_nodes[stage] < 1:
            bad.append(f"NONDEGENERATE_STAGE_NODES: 阶段 {stage} 节点数=0")
    # 零对象守卫第 2 条：声明的对象数必须与判定实际消费的数一致
    if counts["n_blocks"] != len(spec_index(doc)):
        bad.append(
            f"ZERO_OBJECT_GUARD_MISMATCH: n_blocks={counts['n_blocks']} "
            f"但判定消费的 (stage, block) 键数={len(spec_index(doc))}"
        )
    # 每个 (stage, block) 键在 blocks 里恰好出现一次
    if len(doc["blocks"]) != len({(b["stage"], b["block"]) for b in doc["blocks"]}):
        bad.append("DUPLICATE_BLOCK_KEY: blocks 数组里有重复的 (stage, block)")
    # 注册表侧非退化：模块数 ≥ 3，且每模块恰一个 operation（C1 结构的可判子集）
    if len(reg["modules"]) < expected_stages:
        bad.append(f"NONDEGENERATE_REGISTRY: 注册表模块数={len(reg['modules'])} < {expected_stages}")
    for module in reg["modules"]:
        if len(module["operations"]) != 1:
            bad.append(
                f"REGISTRY_OPERATION_COUNT: {module['module_id']} "
                f"operation 数={len(module['operations'])}，应为 1"
            )
    return bad