#!/usr/bin/env python3
"""RT-001 typed DAG compiler（静态编译 + 校验 + 计划图生成）。

冻结语义（tasks/03_RUNTIME_DATA_IO_TASKS.md RT-001 + 约束 F.1 + AUD-001 IRF-0007 修复）:
  - 每 node 定义: node_id/module_id/operation/input ports(output artifact: 引用)/output
    ports/config/resource_class; 图顶层 phase ∈ {phase1,phase2,phase3}。
  - 每 node 只绑定一个真实 operation: (module_id, operation) 必须存在于
    module_ports.registry.json 且唯一解析到该 module 的单一 operation record
    (entry = 真实入口名; 旧"多节点委托同一 Session"的聚合 module 不入表 → 拒绝)。
  - compiler 检查:
      结构/必填字段;          -> STRUCT
      phase scope(module.phase == 顶层 phase);  -> PHASE_SCOPE
      operation 绑定唯一;      -> UNKNOWN_OPERATION / AMBIGUOUS_OPERATION
      必需端口齐全、非法端口拒; -> MISSING_PORT / UNKNOWN_PORT
      类型(DATA schema/scalar) / 单位 / 坐标 / shape 沿边一致; -> DATA_MISMATCH /
        TYPE_MISMATCH / UNIT_MISMATCH / COORDINATE_MISMATCH / SHAPE_MISMATCH
      无隐式文件路径(port 值必须 artifact: 引用); -> IMPLICIT_PATH
      DAG 无环;                -> CYCLE
      无重复 producer;         -> DUPLICATE_PRODUCER
      无跨 Phase edge;         -> CROSS_PHASE
      无多节点复用同一 module/session 包装; -> MODULE_REUSED
  成功时输出 typed plan graph JSON (供 RT-002..007 scheduler 消费)。

schema 文件 typed_dag.schema.json 为本合同权威 JSON 形态; 本文件为执行形态,
二者约束一一对应(见文件头 self-check 函数)。
"""
from __future__ import annotations

import json
import pathlib
import re
import sys
from typing import Any, Dict, List, Optional, Tuple

REPO = pathlib.Path(__file__).resolve().parents[2]
SCHEMA_PATH = REPO / "runtime" / "pipeline" / "typed_dag.schema.json"
REGISTRY_PATH = REPO / "runtime" / "pipeline" / "module_ports.registry.json"
SCHEMA_CONST = "astrocs.typed-dag/v1"
PLAN_SCHEMA_CONST = "astrocs.plan-graph/v1"

_PHASES = ("phase1", "phase2", "phase3")
_RESOURCE_CLASSES = ("metadata", "io", "cpu_light", "cpu_heavy")
_DIRECTIONS = ("input", "output")
_NODE_ID_RE = re.compile(r"^[a-z][a-z0-9_.-]*$")
_MODULE_ID_RE = re.compile(r"^astrocs\.[a-z0-9_.-]+$")
_OPERATION_RE = re.compile(r"^[a-z][a-z0-9_.-]*$")
_ARTIFACT_RE = re.compile(r"^artifact:[A-Za-z0-9_.:-]+$")
# shape 语法: [] | [H] | [H,W] | [N,2] ... 字母或 * 或 正整数; 亦允许 [*,*]
_SHAPE_TOKENS = re.compile(r"^(\[[A-Za-z0-9*,]*\])$")


# ── 错误码（数值冻结, 追加不改; 字符串即错误分类） ──
class DagError:
    STRUCT = "STRUCT"                    # JSON 结构/必填字段违例
    PHASE_SCOPE = "PHASE_SCOPE"          # module 的 phase 与图顶层 phase 不一致
    UNKNOWN_MODULE = "UNKNOWN_MODULE"    # module_id 未在绑定表登记(含聚合 Session 模块)
    UNKNOWN_OPERATION = "UNKNOWN_OPERATION"   # operation 未在绑定表
    AMBIGUOUS_OPERATION = "AMBIGUOUS_OPERATION"  # 绑定表同一 module 多 operation(禁用)
    MISSING_PORT = "MISSING_PORT"        # operation 必需输入端口未被 node 提供
    UNKNOWN_PORT = "UNKNOWN_PORT"        # node 声明端口不在 operation 端口集
    DATA_MISMATCH = "DATA_MISMATCH"      # data_schema_id 冲突
    TYPE_MISMATCH = "TYPE_MISMATCH"      # scalar 类型冲突(错类型)
    UNIT_MISMATCH = "UNIT_MISMATCH"      # 单位冲突
    COORDINATE_MISMATCH = "COORDINATE_MISMATCH"  # 坐标系冲突
    SHAPE_MISMATCH = "SHAPE_MISMATCH"    # shape 冲突
    IMPLICIT_PATH = "IMPLICIT_PATH"      # 隐式文件路径/非 artifact 引用
    CYCLE = "CYCLE"                      # DAG 环
    DUPLICATE_PRODUCER = "DUPLICATE_PRODUCER"  # 重复 producer
    CROSS_PHASE = "CROSS_PHASE"          # 跨 Phase edge
    MODULE_REUSED = "MODULE_REUSED"      # 多节点复用同一 module(同 Session 包装)
    UNPRODUCED_OUTPUT = "UNPRODUCED_OUTPUT"  # pipeline outputs 未被产出
    CONFIG_INVALID = "CONFIG_INVALID"    # config 非对象/缺必填


# 错误码 → 说明（机器可读）
DAG_ERROR_DOC = {
    DagError.STRUCT: "JSON 结构/必填字段违例(缺字段/多字段/类型错)",
    DagError.PHASE_SCOPE: "节点 module 的 phase 与图顶层 phase 不一致(禁止跨 phase 图)",
    DagError.UNKNOWN_MODULE: "module_id 未登记为可绑定 operation 的真实模块(聚合 Session 模块禁止入图)",
    DagError.UNKNOWN_OPERATION: "operation 未在绑定表登记(每节点必须绑定唯一真实 operation)",
    DagError.AMBIGUOUS_OPERATION: "同一 module_id 绑定多个 operation 属禁用(绑定必须唯一)",
    DagError.MISSING_PORT: "operation 声明的必需输入端口未被该节点提供",
    DagError.UNKNOWN_PORT: "节点声明的端口不在该 operation 端口集(非法端口)",
    DagError.DATA_MISMATCH: "边两端 data_schema_id 不一致",
    DagError.TYPE_MISMATCH: "边两端 scalar 类型不一致(错类型)",
    DagError.UNIT_MISMATCH: "边两端单位不一致",
    DagError.COORDINATE_MISMATCH: "边两端坐标系不一致",
    DagError.SHAPE_MISMATCH: "边两端 shape 声明不一致",
    DagError.IMPLICIT_PATH: "端口值不是 artifact: 显式引用(隐式文件路径绑定禁止)",
    DagError.CYCLE: "依赖图存在环(DAG 要求无环)",
    DagError.DUPLICATE_PRODUCER: "同一 artifact 被多个输出端口产出",
    DagError.CROSS_PHASE: "数据边跨越不同 Phase(阶段间仅磁盘产品交换, 不在同一图内连线)",
    DagError.MODULE_REUSED: "多节点复用同一 module_id(同一 Session/包装 被多个节点调用禁止)",
    DagError.UNPRODUCED_OUTPUT: "顶层 outputs 引用的 artifact 未被任何节点产出",
    DagError.CONFIG_INVALID: "node config 非法",
}


def _no_nan(*_args: Any) -> None:
    raise ValueError("non-standard JSON constant (NaN/Infinity) is rejected")


def load_strict_json(text: str) -> Any:
    """严格 JSON 解析: 拒绝 NaN/Infinity 与对象重复 key。"""

    def _obj_hook(pairs: list) -> dict:
        seen = set()
        for k, _ in pairs:
            if k in seen:
                raise ValueError(f"duplicate key: {k}")
            seen.add(k)
        return dict(pairs)

    return json.loads(text, parse_constant=_no_nan, object_pairs_hook=_obj_hook)


class Registry:
    """module_ports.registry.json 装载: (module_id, operation) → operation record。"""

    def __init__(self, data: Optional[dict] = None, errors: Optional[List[str]] = None):
        self.modules: Dict[str, dict] = {}
        self.issues: List[str] = errors if errors is not None else []
        if data is None:
            return
        for m in data.get("modules", []):
            mid = m.get("module_id", "")
            ops = m.get("operations", [])
            if not mid or not ops:
                self.issues.append(f"registry: module 缺 module_id/operations: {mid}")
                continue
            if mid in self.modules:
                self.issues.append(f"registry: 重复 module_id: {mid}")
                continue
            if len(ops) != 1:
                # 绑定唯一性合同: 每个 module 只能有一个可执行 operation
                self.issues.append(
                    f"registry: module {mid} 绑定 {len(ops)} 个 operation(必须唯一)")
                continue
            op = ops[0]
            if op.get("operation") != op.get("operation", "").strip() or \
                    not _OPERATION_RE.match(op.get("operation", "")):
                self.issues.append(f"registry: module {mid} operation 词法非法")
                continue
            names: Dict[str, str] = {}
            for p in op.get("ports", []):
                pname = p.get("name", "")
                direction = p.get("direction", "")
                if not pname or direction not in _DIRECTIONS:
                    self.issues.append(f"registry: module {mid} 端口定义非法: {pname}")
                    continue
                if pname in names:
                    self.issues.append(f"registry: module {mid} 重复端口名 {pname}")
                    continue
                names[pname] = direction
            self.modules[mid] = m

    def module(self, mid: str) -> Optional[dict]:
        return self.modules.get(mid)

    def phase(self, mid: str) -> Optional[str]:
        m = self.modules.get(mid)
        return m.get("phase") if m else None

    def operation(self, mid: str, op: str) -> Optional[dict]:
        m = self.modules.get(mid)
        if not m:
            return None
        for o in m.get("operations", []):
            if o.get("operation") == op:
                return o
        return None

    def port(self, mid: str, op: str, port: str) -> Optional[dict]:
        o = self.operation(mid, op)
        if not o:
            return None
        for p in o.get("ports", []):
            if p.get("name") == port:
                return p
        return None

    @classmethod
    def load_default(cls) -> "Registry":
        data = load_strict_json(REGISTRY_PATH.read_text(encoding="utf-8"))
        return cls(data)


class Plan:
    """编译成功的类型化计划图。"""

    def __init__(self, ir: dict, registry: Registry,
                 resolved: List[dict], deps: Dict[str, List[str]],
                 edges: List[dict], producer_of: Dict[str, Tuple[str, str]]):
        self.ir = ir
        self.registry = registry
        self.resolved = resolved      # 每 node: {node_id,module_id,operation,phase,resource_class,...}
        self.deps = deps              # node_id -> [依赖 node_id]
        self.edges = edges            # 数据边(已类型校验)
        self.producer_of = producer_of  # artifact -> (node_id, port)

    def to_json(self) -> str:
        ir = self.ir
        out = {
            "schema": PLAN_SCHEMA_CONST,
            "pipeline_id": ir["pipeline_id"],
            "phase": ir["phase"],
            "version": ir["version"],
            "nodes": [],
            "edges": [],
            "outputs": ir.get("outputs", {}),
            "acyclic": True,
            "single_operation_per_node": True,
            "module_bindings": {},
        }
        for n in self.resolved:
            out["nodes"].append({
                "node_id": n["node_id"],
                "module_id": n["module_id"],
                "operation": n["operation"],
                "entry": n["entry"],
                "phase": n["phase"],
                "resource_class": n["resource_class"],
                "config": n.get("config", {}),
                "inputs": n.get("inputs", {}),
                "outputs": n.get("outputs", {}),
            })
            out["module_bindings"][n["module_id"]] = n["operation"]
        for e in self.edges:
            out["edges"].append({
                "from_node": e["from_node"],
                "from_port": e["from_port"],
                "to_node": e["to_node"],
                "to_port": e["to_port"],
                "artifact": e["artifact"],
                "data_schema_id": e["data_schema_id"],
                "unit": e["unit"],
                "coordinate": e["coordinate"],
                "scalar": e["scalar"],
                "shape_hint": e["shape_hint"],
            })
        return json.dumps(out, indent=1, sort_keys=False)


class CompileResult:
    def __init__(self, plan: Optional[Plan], errors: List[dict]):
        self.plan = plan
        self.errors = errors

    @property
    def ok(self) -> bool:
        return self.plan is not None

    def error_lines(self) -> List[str]:
        return [
            f"{e.get('code', 'UNKNOWN')} node={e.get('node_id', '')} artifact={e.get('artifact', '')}: {e.get('detail', '')}"
            for e in self.errors
        ]


class TypedDagCompiler:
    """typed DAG 静态编译: 结构 → 绑定 → 端口 → 类型 → 拓扑 → 计划图。"""

    def __init__(self, registry: Optional[Registry] = None):
        self.registry = registry if registry is not None else Registry.load_default()

    # ── 顶层入口 ──
    def compile(self, ir: dict) -> CompileResult:
        errors: List[dict] = []
        plan = self._compile(ir, errors)
        return CompileResult(plan, errors)

    def compile_text(self, text: str) -> CompileResult:
        try:
            ir = load_strict_json(text)
        except ValueError as e:
            return CompileResult(None, [{
                "code": DagError.STRUCT, "node_id": "", "detail": f"JSON 解析失败: {e}"}])
        if not isinstance(ir, dict):
            return CompileResult(None, [{
                "code": DagError.STRUCT, "node_id": "",
                "detail": "顶层必须是对象(typed dag v1)"}])
        return self.compile(ir)

    # ── 结构校验（镜像 typed_dag.schema.json; self-check 见文件尾） ──
    def _struct_errors(self, ir: dict, errors: List[dict]) -> bool:
        ok = True
        for field in ("schema", "pipeline_id", "phase", "version", "nodes", "outputs"):
            if field not in ir:
                errors.append({"code": DagError.STRUCT, "node_id": "",
                               "detail": f"顶层缺必填字段: {field}"})
                ok = False
        if not ok:
            return False
        if ir["schema"] != SCHEMA_CONST:
            errors.append({"code": DagError.STRUCT, "node_id": "",
                           "detail": f"schema 必须为 {SCHEMA_CONST}"})
            return False
        if ir["phase"] not in _PHASES:
            errors.append({"code": DagError.STRUCT, "node_id": "",
                           "detail": f"phase 必须 ∈ {_PHASES}"})
            return False
        if not isinstance(ir.get("pipeline_id"), str) or \
                not _NODE_ID_RE.match(ir["pipeline_id"]):
            errors.append({"code": DagError.STRUCT, "node_id": "",
                           "detail": "pipeline_id 词法非法"})
            return False
        if not isinstance(ir.get("nodes"), list) or not ir["nodes"]:
            errors.append({"code": DagError.STRUCT, "node_id": "",
                           "detail": "nodes 必须为非空数组"})
            return False
        if not isinstance(ir.get("outputs"), dict) or not ir["outputs"]:
            errors.append({"code": DagError.STRUCT, "node_id": "",
                           "detail": "outputs 必须为非空对象"})
            return False
        for name, art in ir["outputs"].items():
            if not isinstance(art, str) or not _ARTIFACT_RE.match(art):
                errors.append({"code": DagError.IMPLICIT_PATH, "node_id": "",
                               "artifact": str(art),
                               "detail": f"顶层 output {name} 不是 artifact: 引用"})
                return False
        node_ids: set = set()
        for idx, nj in enumerate(ir["nodes"]):
            if not isinstance(nj, dict):
                errors.append({"code": DagError.STRUCT, "node_id": "",
                               "detail": f"nodes[{idx}] 不是对象"})
                return False
            missing = [f for f in ("node_id", "module_id", "operation",
                                   "inputs", "outputs", "config", "resources")
                       if f not in nj]
            if missing:
                errors.append({"code": DagError.STRUCT,
                               "node_id": str(nj.get("node_id", "")),
                               "detail": f"nodes[{idx}] 缺必填字段: {missing}"})
                return False
            nid = nj["node_id"]
            if not isinstance(nid, str) or not _NODE_ID_RE.match(nid):
                errors.append({"code": DagError.STRUCT, "node_id": str(nid),
                               "detail": "node_id 词法非法"})
                return False
            if nid in node_ids:
                errors.append({"code": DagError.STRUCT, "node_id": nid,
                               "detail": "node_id 重复"})
                return False
            node_ids.add(nid)
            if not isinstance(nj["module_id"], str) or \
                    not _MODULE_ID_RE.match(nj["module_id"]):
                errors.append({"code": DagError.STRUCT, "node_id": nid,
                               "detail": f"module_id 词法非法: {nj['module_id']}"})
                return False
            if not isinstance(nj["operation"], str) or \
                    not _OPERATION_RE.match(nj["operation"]):
                errors.append({"code": DagError.STRUCT, "node_id": nid,
                               "detail": f"operation 词法非法: {nj['operation']}"})
                return False
            rc = nj["resources"]
            if not isinstance(rc, dict) or "class" not in rc:
                errors.append({"code": DagError.STRUCT, "node_id": nid,
                               "detail": "resources.class 必填"})
                return False
            if rc["class"] not in _RESOURCE_CLASSES:
                errors.append({"code": DagError.STRUCT, "node_id": nid,
                               "detail": f"resources.class 非法: {rc['class']}"})
                return False
            if rc["class"] == "cpu_heavy" and rc.get("parallel") is not True:
                errors.append({"code": DagError.STRUCT, "node_id": nid,
                               "detail": "cpu_heavy 必须 parallel=true"})
                return False
            if not isinstance(nj.get("config"), dict):
                errors.append({"code": DagError.CONFIG_INVALID, "node_id": nid,
                               "detail": "config 必须为对象"})
                return False
            for side in ("inputs", "outputs"):
                pmap = nj[side]
                if not isinstance(pmap, dict) or not pmap:
                    errors.append({"code": DagError.STRUCT, "node_id": nid,
                                   "detail": f"{side} 必须为非空端口对象"})
                    return False
                for port, art in pmap.items():
                    if not isinstance(art, str) or not _ARTIFACT_RE.match(art):
                        errors.append({"code": DagError.IMPLICIT_PATH, "node_id": nid,
                                       "artifact": str(art),
                                       "detail": f"{side} 端口 {port} 不是 artifact: 引用"
                                                 f"(隐式文件路径/裸字符串禁止): {art}"})
                        return False
        return True

    def _compile(self, ir: dict, errors: List[dict]) -> Optional[Plan]:
        if not self._struct_errors(ir, errors):
            return None

        top_phase = ir["phase"]
        resolved: List[dict] = []
        producer_of: Dict[str, Tuple[str, str]] = {}
        consumers_of: Dict[str, List[Tuple[str, str, str]]] = {}  # art -> [(node, port, module_id)]
        module_nodes: Dict[str, List[str]] = {}

        # 第一轮: module/operation 绑定 + 端口存在性 + 必需端口
        for nj in ir["nodes"]:
            nid = nj["node_id"]
            mid = nj["module_id"]
            op = nj["operation"]
            rc = nj["resources"]["class"]
            rec = self.registry.operation(mid, op)
            if self.registry.module(mid) is None:
                errors.append({"code": DagError.UNKNOWN_MODULE, "node_id": nid,
                               "detail": f"module_id {mid} 未登记(聚合 Session 模块不可入 typed DAG)"})
                continue
            if rec is None:
                errors.append({"code": DagError.UNKNOWN_OPERATION, "node_id": nid,
                               "detail": f"module {mid} 无 operation {op}(每节点须绑定唯一真实 operation)"})
                continue
            # 绑定唯一性: registry 每 module 恰一个 operation(装载期强制)
            if len(self.registry.module(mid).get("operations", [])) != 1:
                errors.append({"code": DagError.AMBIGUOUS_OPERATION, "node_id": nid,
                               "detail": f"module {mid} 绑定不唯一"})
                continue
            mod_phase = self.registry.phase(mid)
            if mod_phase != top_phase:
                errors.append({"code": DagError.PHASE_SCOPE, "node_id": nid,
                               "detail": f"module {mid} 属 {mod_phase}, 图属 {top_phase}"
                                         f"(禁止跨 Phase 节点/图)"})
                continue
            if mid in module_nodes:
                errors.append({"code": DagError.MODULE_REUSED, "node_id": nid,
                               "detail": f"module {mid} 已被节点 {module_nodes[mid][0]} 使用;"
                                         f" 多节点复用同一 module/Session 包装禁止"})
                continue
            module_nodes[mid] = [nid]
            # 端口方向索引
            in_ports = {p["name"]: p for p in rec.get("ports", [])
                        if p.get("direction") == "input"}
            out_ports = {p["name"]: p for p in rec.get("ports", [])
                         if p.get("direction") == "output"}
            if not in_ports or not out_ports:
                errors.append({"code": DagError.STRUCT, "node_id": nid,
                               "detail": f"operation {op} 必须同时有 input/output 端口"})
                continue
            # node 声明的每个端口必须存在于 operation
            for port in nj["inputs"]:
                if port not in in_ports:
                    errors.append({"code": DagError.UNKNOWN_PORT, "node_id": nid,
                                   "detail": f"input 端口 {port} 不在 operation {op} 端口集"})
            for port in nj["outputs"]:
                if port not in out_ports:
                    errors.append({"code": DagError.UNKNOWN_PORT, "node_id": nid,
                                   "detail": f"output 端口 {port} 不在 operation {op} 端口集"})
            # 必需输入端口: operation 每个 input port 都必须被 node 提供
            for pname in in_ports:
                if pname not in nj["inputs"]:
                    errors.append({"code": DagError.MISSING_PORT, "node_id": nid,
                                   "detail": f"operation {op} 必需输入端口 {pname} 未提供"})
            # 收集 producer/consumer
            for port, art in nj["outputs"].items():
                if art in producer_of:
                    errors.append({"code": DagError.DUPLICATE_PRODUCER, "node_id": nid,
                                   "artifact": art,
                                   "detail": f"artifact {art} 已由 {producer_of[art][0]} 产出"})
                else:
                    producer_of[art] = (nid, port)
            for port, art in nj["inputs"].items():
                consumers_of.setdefault(art, []).append((nid, port, mid))
            resolved.append({
                "node_id": nid, "module_id": mid, "operation": op,
                "entry": rec.get("entry", ""), "phase": mod_phase,
                "resource_class": rc,
                "config": nj.get("config", {}),
                "inputs": nj["inputs"], "outputs": nj["outputs"],
                "in_port_meta": in_ports, "out_port_meta": out_ports,
            })

        if any(e["code"] in (DagError.UNKNOWN_MODULE, DagError.UNKNOWN_OPERATION,
                             DagError.PHASE_SCOPE, DagError.MODULE_REUSED)
               for e in errors):
            return None  # 绑定层失败: 无法继续类型校验

        # 第二轮: 沿数据边类型/单位/坐标/shape 校验 + 跨 phase 检测
        edges: List[dict] = []
        node_by_id = {n["node_id"]: n for n in resolved}
        deps: Dict[str, List[str]] = {nid: [] for nid in node_by_id}
        for art, (prod_node, prod_port) in producer_of.items():
            pn = node_by_id[prod_node]
            pm = pn["out_port_meta"][prod_port]
            prod_phase = pn["phase"]
            for (cons_node, cons_port, cons_mid) in consumers_of.get(art, []):
                cn = node_by_id[cons_node]
                cm = cn["in_port_meta"][cons_port]
                cons_phase = cn["phase"]
                if prod_phase != cons_phase:
                    errors.append({"code": DagError.CROSS_PHASE,
                                   "node_id": cons_node, "artifact": art,
                                   "detail": f"edge {prod_node}.{prod_port} -> {cons_node}."
                                             f"{cons_port} 跨 Phase({prod_phase}->{cons_phase});"
                                             f" 阶段间仅磁盘产品交换"})
                    continue
                for key, label, code in (
                        ("data_schema_id", "data_schema_id", DagError.DATA_MISMATCH),
                        ("unit", "unit", DagError.UNIT_MISMATCH),
                        ("coordinate", "coordinate", DagError.COORDINATE_MISMATCH),
                        ("scalar", "scalar 类型", DagError.TYPE_MISMATCH),
                        ("shape_hint", "shape", DagError.SHAPE_MISMATCH)):
                    pv = pm.get(key, "")
                    cv = cm.get(key, "")
                    if pv != cv and pv and cv:
                        errors.append({"code": code, "node_id": cons_node,
                                       "artifact": art,
                                       "detail": f"{label} 冲突: producer {prod_node}."
                                                 f"{prod_port}={pv} != consumer {cons_node}."
                                                 f"{cons_port}={cv}"})
                deps.setdefault(cons_node, []).append(prod_node)
                edges.append({
                    "from_node": prod_node, "from_port": prod_port,
                    "to_node": cons_node, "to_port": cons_port,
                    "artifact": art,
                    "data_schema_id": pm.get("data_schema_id", ""),
                    "unit": pm.get("unit", ""),
                    "coordinate": pm.get("coordinate", ""),
                    "scalar": pm.get("scalar", ""),
                    "shape_hint": pm.get("shape_hint", ""),
                })

        # 第三轮: 环检测 (producer 依赖)
        visiting: set = set()
        done: set = set()
        cycle_found = False

        def dfs(nid: str) -> bool:
            nonlocal cycle_found
            if cycle_found or nid in done:
                return False
            if nid in visiting:
                cycle_found = True
                return True
            visiting.add(nid)
            for d in deps.get(nid, []):
                if dfs(d):
                    return True
            visiting.discard(nid)
            done.add(nid)
            return False

        for nid in node_by_id:
            if dfs(nid):
                errors.append({"code": DagError.CYCLE, "node_id": nid,
                               "detail": "依赖图存在环(artifact producer 依赖)"})
                break

        # 顶层 outputs 必须被产出
        produced = set(producer_of.keys())
        for name, art in ir["outputs"].items():
            if art not in produced:
                errors.append({"code": DagError.UNPRODUCED_OUTPUT, "node_id": "",
                               "artifact": art,
                               "detail": f"pipeline output {name} 引用的 artifact 未被产出"})

        if errors:
            return None
        return Plan(ir, self.registry, resolved, deps, edges, producer_of)


# ── schema 自检: 本执行形态与 typed_dag.schema.json 约束一一对应 ──
def schema_self_check() -> List[str]:
    """返回不一致清单; 空列表 = PASS。"""
    issues: List[str] = []
    schema = load_strict_json(SCHEMA_PATH.read_text(encoding="utf-8"))
    if schema.get("properties", {}).get("schema", {}).get("const") != SCHEMA_CONST:
        issues.append("schema const 与本文件 SCHEMA_CONST 不一致")
    phase_enum = schema.get("properties", {}).get("phase", {}).get("enum", [])
    if list(phase_enum) != list(_PHASES):
        issues.append(f"schema phase enum 与编译器不一致: {phase_enum}")
    rc_enum = schema["properties"]["nodes"]["items"]["properties"]["resources"] \
        ["properties"]["class"]["enum"]
    if list(rc_enum) != list(_RESOURCE_CLASSES):
        issues.append("schema resources.class enum 与编译器不一致")
    art_pat = schema["properties"]["nodes"]["items"]["properties"]["inputs"] \
        ["additionalProperties"]["pattern"]
    if art_pat != _ARTIFACT_RE.pattern:
        issues.append(f"schema artifact pattern 与编译器不一致: {art_pat}")
    node_req = schema["properties"]["nodes"]["items"]["required"]
    if node_req != ["node_id", "module_id", "operation", "inputs",
                    "outputs", "config", "resources"]:
        issues.append(f"schema node required 与编译器不一致: {node_req}")
    return issues


def main(argv: Optional[List[str]] = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv:
        print(__doc__)
        return 2
    if argv[0] == "--schema-self-check":
        issues = schema_self_check()
        if issues:
            for i in issues:
                print(f"SCHEMA_MISMATCH: {i}", file=sys.stderr)
            return 1
        print("TYPED_DAG_SCHEMA_SELF_CHECK PASS")
        return 0
    if argv[0] == "--registry-check":
        reg = Registry.load_default()
        if reg.issues:
            for i in reg.issues:
                print(f"REGISTRY_ISSUE: {i}", file=sys.stderr)
            return 1
        print(f"TYPED_DAG_REGISTRY PASS modules={len(reg.modules)}")
        return 0
    path = argv[0]
    try:
        text = pathlib.Path(path).read_text(encoding="utf-8")
    except OSError as e:
        print(f"READ_FAIL: {e}", file=sys.stderr)
        return 2
    compiler = TypedDagCompiler()
    res = compiler.compile_text(text)
    if res.ok:
        print(res.plan.to_json())
        return 0
    for line in res.error_lines():
        print(f"TYPED_DAG_ERROR: {line}", file=sys.stderr)
    print("TYPED_DAG_FAIL", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
