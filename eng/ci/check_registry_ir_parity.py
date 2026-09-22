#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-REGISTRY-IR-PARITY —— 生产注册表 ↔ Pipeline IR 节点集合双向一致门。

防的复发缺口（B 类）：**注册了但不在管线**（模块 descriptor 已 register_module，
但 build_pipeline_ir 的 preset→IR 里没有该节点 ⇒ 该模块永远不被调度，静默死代码）。

判据（双向集合，fail-closed）：
  P1 registered_not_in_ir：register_phase_modules() 里 register_module 的每个
     descriptor 的 module_id 必须出现在 build_pipeline_ir() 的节点 module_id 集合；
  P2 ir_not_registered：IR 里每个节点 module_id 必须有已注册 descriptor；
  P3 任一侧为空（解析不到）⇒ 判红（不得把「解析不到」当「一致」）。

锚点（硬编码引用，缺失即 rc=2 并点名 ANCHOR_MISSING）：
  - lib/infrastructure/scheduler/src/module_adapters.cpp（生产注册面）
  - lib/infrastructure/cli/runtime_client.cpp（生产 IR 面）
  - eng/ci/ledgers/registry_ir_parity.json（显式台账：已知差异 + 理由/负责人/解除条件）

用法：
  python3 eng/ci/check_registry_ir_parity.py [--repo ROOT] [--json-out F] [--self-test]
exit 0 = 一致；exit 1 = 有 finding；exit 2 = 锚点/台账不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gate_common as gc  # noqa: E402

CHECK_ID = "CHK-REGISTRY-IR-PARITY"
REGISTER_CPP = "lib/infrastructure/scheduler/src/module_adapters.cpp"
IR_CPP = "lib/infrastructure/cli/runtime_client.cpp"
LEDGER = "eng/ci/ledgers/registry_ir_parity.json"
PORTS_REGISTRY = "lib/infrastructure/pipeline/module_ports.registry.json"

# ── 节点序 / 边保真判据（phase1）────────────────────────────────────────────
# IR 的 artifact 身份 ↔ 注册表端口身份的唯一桥（fail-closed: 未登记即判红）。
# 依据 = 注册表 artifacts 字段与该产物在 output_dir 下的真实文件名/前缀。
PHASE1_ARTIFACT_TO_PORT = {
    "artifact:in": "lights",
    "artifact:cal": "p1_calibrated",
    "artifact:cos": "p1_cleaned",
    "artifact:p1_sources": "p1_sources",
    "artifact:p1_psf": "p1_psf",
    "artifact:p1_wcs": "p1_wcs",
    "artifact:p1_flux": "p1_flux",
    "artifact:p1_phot": "p1_phot",
    "artifact:p1_snr": "p1_snr",
    "artifact:p1_stack": "p1_stack",
    "artifact:p1_hips": "p1_final",
}
MIN_PHASE1_NODES = 8
MIN_PHASE1_EDGES = 10


def _brace_body(text: str, marker: str):
    idx = text.find(marker)
    if idx < 0:
        return None
    start = text.find("{", idx)
    if start < 0:
        return None
    depth = 0
    for i in range(start, len(text)):
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:i]
    return None


def parse_descriptor_module_ids(adapters_text: str) -> dict:
    out = {}
    for m in re.finditer(r"ModuleDescriptor\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*\)\s*\{", adapters_text):
        fn = m.group(1)
        window = adapters_text[m.end():m.end() + 4000]
        mid = re.search(r"\.module_id\s*=\s*\"([^\"]+)\"", window)
        if mid:
            out[fn] = mid.group(1)
    return out


def parse_registered(adapters_text: str) -> set:
    body = _brace_body(adapters_text, "Result<void> register_phase_modules(")
    if body is None:
        raise gc.GateError("ANCHOR_STALE: register_phase_modules not found in %s" % REGISTER_CPP)
    desc_ids = parse_descriptor_module_ids(adapters_text)
    registered = set()
    for fn in set(re.findall(r"([A-Za-z_][A-Za-z0-9_]*_descriptor)\s*\(\s*\)", body)):
        if fn in desc_ids:
            registered.add(desc_ids[fn])
    if not registered:
        raise gc.GateError("ANCHOR_STALE: no registered descriptors parsed from %s" % REGISTER_CPP)
    return registered


def parse_ir(ir_text: str) -> set:
    body = _brace_body(ir_text, "std::string build_pipeline_ir(")
    if body is None:
        raise gc.GateError("ANCHOR_STALE: build_pipeline_ir not found in %s" % IR_CPP)
    clean = gc.strip_comments(body)
    nodes = set(re.findall(r"\"(astrocs\.phase[123]\.[a-z0-9\-]+)\"", clean))
    if not nodes:
        raise gc.GateError("ANCHOR_STALE: no IR node module_id parsed from %s" % IR_CPP)
    return nodes


# ── phase1 节点序 / 边保真（C4–C7）──────────────────────────────────────────
# 防的复发缺口：**IR 声明的边与序偏离注册表的真实数据流**——
#   ① 幻边（注册表里该模块没有这个输入端口）会把调度器钉在错误序上。典型:
#      wcs 声明消费 p1_sources 而实现按帧自读 calibrated 像素自行检测 ⇒ 该边不存在,
#      它把"检测"钉在"解算"之前, 使"权威检测需要含取向的 WCS"变成循环依赖;
#   ② 节点数组序不是注册表 DAG 的拓扑序 ⇒ 数组序只是文档, 与调度真实序脱节;
#   ③ psf 节点被排到 wcs 之前 ⇒ 取向先验只能靠配置或"北向上/东向左"假设
#      （ASTROCS_DESIGN.md §4.2: 权威路径不得以假设取向冒充）。

def _split_top_level(arg_text: str):
    """按顶层逗号切分 mk() 实参（只认 {} () [] 深度；字符串内不切）。"""
    parts, cur, depth, in_str, i = [], [], 0, False, 0
    while i < len(arg_text):
        ch = arg_text[i]
        if in_str:
            cur.append(ch)
            if ch == "\\" and i + 1 < len(arg_text):
                cur.append(arg_text[i + 1])
                i += 2
                continue
            if ch == '"':
                in_str = False
            i += 1
            continue
        if ch == '"':
            in_str = True
            cur.append(ch)
            i += 1
            continue
        if ch in "{([":
            depth += 1
        elif ch in "})]":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append("".join(cur))
            cur = []
            i += 1
            continue
        cur.append(ch)
        i += 1
    parts.append("".join(cur))
    return [p.strip() for p in parts if p.strip()]


_MK_PAIR = re.compile(r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\}')
_STR_LIT = re.compile(r'"([^"]*)"')


def mk_call_span(text: str, start: int):
    """返回 text[start:] 内 mk( ... ) 的实参文本与结束下标（括号配平；已去注释）。"""
    open_idx = text.find("(", start)
    if open_idx < 0:
        raise gc.GateError("ANCHOR_STALE: mk( has no argument list in %s" % IR_CPP)
    depth = 0
    for i in range(open_idx, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return text[open_idx + 1:i], i + 1
    raise gc.GateError("ANCHOR_STALE: unbalanced mk( in %s" % IR_CPP)


def parse_phase1_ir_nodes(ir_text: str):
    """按源码顺序解析 phase1_nodes 的 mk() 调用 → [{node_id, module_id, inputs, outputs}]。"""
    body = _brace_body(ir_text, "auto phase1_nodes =")
    if body is None:
        raise gc.GateError("ANCHOR_STALE: phase1_nodes lambda not found in %s" % IR_CPP)
    clean = _strip_line_comments(body)
    nodes, cursor = [], 0
    while True:
        idx = clean.find("mk(", cursor)
        if idx < 0:
            break
        args_text, end = mk_call_span(clean, idx)
        cursor = end
        args = _split_top_level(args_text)
        if len(args) < 4:
            continue
        nid = _STR_LIT.search(args[0])
        mid = _STR_LIT.search(args[1])
        if not nid or not mid or not mid.group(1).startswith("astrocs.phase1."):
            continue
        nodes.append({
            "node_id": nid.group(1),
            "module_id": mid.group(1),
            "inputs": dict(_MK_PAIR.findall(args[2])),
            "outputs": dict(_MK_PAIR.findall(args[3])),
        })
    if len(nodes) < MIN_PHASE1_NODES:
        raise gc.GateError(
            "ANCHOR_STALE: only %d phase1 node(s) parsed from %s (min %d) —— 抽取退化"
            % (len(nodes), IR_CPP, MIN_PHASE1_NODES))
    return nodes


def load_registry_port_graph(repo: pathlib.Path):
    """注册表 → {端口名: [输出模块]} / {端口名: [输入模块]} / {模块: phase}。"""
    path = repo / PORTS_REGISTRY
    if not path.is_file():
        raise gc.GateError("ANCHOR_MISSING: %s" % PORTS_REGISTRY)
    try:
        reg = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        raise gc.GateError("ANCHOR_STALE: %s unparsable: %s" % (PORTS_REGISTRY, exc))
    out_ports, in_ports, phase_of = {}, {}, {}
    for m in reg.get("modules") or []:
        mid = m.get("module_id")
        if not mid:
            continue
        phase_of[mid] = m.get("phase")
        for op in m.get("operations") or []:
            for p in op.get("ports") or []:
                name, direction = p.get("name"), p.get("direction")
                if not name or direction not in ("input", "output"):
                    continue
                (out_ports if direction == "output" else in_ports).setdefault(name, []).append(mid)
    if not out_ports or not in_ports:
        raise gc.GateError("ANCHOR_STALE: registry port graph empty (%s)" % PORTS_REGISTRY)
    return out_ports, in_ports, phase_of


# ── descriptor 端口面（IR 端口名的静态校验源）────────────────────────────────
_DESC_FN = re.compile(r"ModuleDescriptor\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*\)\s*\{")
_PORT_ENTRY = re.compile(r'\{\s*"([^"]+)"\s*,\s*"[^"]*"\s*,\s*(true|false)\s*,')


def _strip_line_comments(text: str) -> str:
    """只去 `//` 行注释（**不**做块注释扫描）。

    gc.strip_comments 的块注释正则会把源码注释里出现的 `/*`（如路径 `aio/src/**`）当成
    块注释起点并吞掉其后大段代码 ⇒ 锚点解析静默退化。本门只依赖行结构，故只去行注释。
    """
    return re.sub(r"//[^\n]*", "", text)


def parse_descriptor_ports(adapters_text: str):
    """→ {module_id: (in_ports:set, out_ports:set)}（来自 module_adapters.cpp descriptor）。"""
    clean = _strip_line_comments(adapters_text)
    out = {}
    for m in _DESC_FN.finditer(clean):
        body = _brace_body(clean[m.start():], "ModuleDescriptor")
        # _brace_body 的 marker 命中函数名后的首个 '{' = 函数体
        if body is None:
            continue
        mid = re.search(r'\.module_id\s*=\s*"([^"]+)"', body)
        if not mid:
            continue
        ports_body = _brace_body(body, "d.ports =")
        if ports_body is None:
            continue
        ins, outs = set(), set()
        for pm in _PORT_ENTRY.finditer(ports_body):
            (ins if pm.group(2) == "true" else outs).add(pm.group(1))
        out[mid.group(1)] = (ins, outs)
    if not out:
        raise gc.GateError("ANCHOR_STALE: no descriptor ports parsed from %s" % REGISTER_CPP)
    return out


def _registry_phase1_order(repo: pathlib.Path):
    """注册表 modules 数组里 phase1 模块的出现序（= 块流规格的 declared order）。"""
    path = repo / PORTS_REGISTRY
    reg = json.loads(path.read_text(encoding="utf-8"))
    return [m["module_id"] for m in reg.get("modules") or []
            if m.get("phase") == "phase1" and m.get("module_id")]


def evaluate_order(repo: pathlib.Path):
    """C4 无幻边 / C5 序为注册表 DAG 拓扑序 / C6 psf 在 wcs 之后 / C7 非退化。"""
    ir_text = gc.read_text(repo / IR_CPP, IR_CPP)
    nodes = parse_phase1_ir_nodes(ir_text)
    out_ports, in_ports, _phase_of = load_registry_port_graph(repo)
    findings = []
    pos = {}
    for i, n in enumerate(nodes):
        if n["node_id"] in pos:
            findings.append("C7 duplicate IR node_id %r" % n["node_id"])
        pos[n["node_id"]] = i
    ir_modules = {n["module_id"] for n in nodes}
    module_pos = {}
    for n in nodes:
        module_pos.setdefault(n["module_id"], pos[n["node_id"]])

    # C4 边保真：IR 每条边必须由注册表端口图支持（无幻边）。
    producer_of = {}
    for n in nodes:
        for port, art in n["outputs"].items():
            producer_of[art] = n
    n_edges = 0
    for n in nodes:
        for port, art in n["inputs"].items():
            prod = producer_of.get(art)
            if prod is None:
                continue          # 外部输入（artifact:in）: 由注册表 config_path 端口承载
            n_edges += 1
            reg_port = PHASE1_ARTIFACT_TO_PORT.get(art)
            if reg_port is None:
                findings.append(
                    "C4 unbridged IR artifact %r (%s -> %s): 未在 PHASE1_ARTIFACT_TO_PORT "
                    "登记, 无法与注册表端口对齐（新增产物必须同步该桥）"
                    % (art, prod["node_id"], n["node_id"]))
                continue
            if prod["module_id"] not in out_ports.get(reg_port, []):
                findings.append(
                    "C4 %s(%s) declares output %r=%s but registry declares no output port %r"
                    % (prod["node_id"], prod["module_id"], port, art, reg_port))
            if n["module_id"] not in in_ports.get(reg_port, []):
                findings.append(
                    "C4 phantom edge %s(%s) -> %s(%s): registry module %s declares no input "
                    "port %r (产物 %s) —— IR 声明的边在注册表/代码里不存在"
                    % (prod["node_id"], prod["module_id"], n["node_id"], n["module_id"],
                       n["module_id"], reg_port, art))

    # C5 序一致：注册表 DAG（端口同名即边）的每条边必须满足 pos(上游) < pos(下游)。
    n_reg_edges = 0
    for port in sorted(set(out_ports) & set(in_ports)):
        for a in out_ports[port]:
            for b in in_ports[port]:
                if a == b or a not in ir_modules or b not in ir_modules:
                    continue
                n_reg_edges += 1
                if module_pos[a] >= module_pos[b]:
                    findings.append(
                        "C5 IR order violates registry edge %s --[%s]--> %s (pos %d >= %d): "
                        "IR 节点数组序不是注册表 DAG 的拓扑序"
                        % (a, port, b, module_pos[a], module_pos[b]))
    # IR 自身声明的边也必须与数组序一致（否则数组序只是文档）。
    for n in nodes:
        for port, art in n["inputs"].items():
            prod = producer_of.get(art)
            if prod is None or prod["node_id"] == n["node_id"]:
                continue
            if pos[prod["node_id"]] >= pos[n["node_id"]]:
                findings.append(
                    "C5 IR order contradicts its own edge %s -> %s (pos %d >= %d)"
                    % (prod["node_id"], n["node_id"], pos[prod["node_id"]], pos[n["node_id"]]))

    # C5b 注册表**声明序**一致：IR 的 phase1 节点序必须等于注册表 modules 数组里
    # 同阶段模块的出现序（block flow 规格的 "declared order" 与 R4 同源）。
    reg_phase1_order = [m for m in _registry_phase1_order(repo) if m in ir_modules]
    ir_phase1_order = []
    for n in nodes:
        if n["module_id"] not in ir_phase1_order:
            ir_phase1_order.append(n["module_id"])
    if reg_phase1_order != ir_phase1_order:
        findings.append(
            "C5b IR phase1 node order %s != registry declared order %s"
            % (ir_phase1_order, reg_phase1_order))

    # C6 psf 必须排在 wcs 之后, 且以 typed 边消费本帧解算产物。
    if "psf" not in pos or "wcs" not in pos:
        findings.append("C6 IR lacks psf/wcs node (pos=%s)" % sorted(pos))
    else:
        if pos["psf"] <= pos["wcs"]:
            findings.append(
                "C6 psf node (pos %d) must come AFTER wcs node (pos %d): 取向先验来自本帧"
                "解算产物 <frame_dir>/p1_wcs.json" % (pos["psf"], pos["wcs"]))
        psf_node = next(n for n in nodes if n["node_id"] == "psf")
        if "artifact:p1_wcs" not in psf_node["inputs"].values():
            findings.append(
                "C6 psf node declares no input artifact:p1_wcs (inputs=%s): 取向先验缺少 "
                "typed 边, 调度器不保证解算先落盘" % psf_node["inputs"])

    # C8 IR 端口名必须存在于对应模块的 C++ descriptor 端口表（IR 静态验证 MISSING_PORT
    # 的 CI 侧等价判据；不依赖运行产品二进制即可发现 IR ↔ descriptor 漂移）。
    adapters_text = gc.read_text(repo / REGISTER_CPP, REGISTER_CPP)
    desc_ports = parse_descriptor_ports(adapters_text)
    for n in nodes:
        mid = n["module_id"]
        if mid not in desc_ports:
            findings.append("C8 %s(%s) has no descriptor in %s" % (n["node_id"], mid, REGISTER_CPP))
            continue
        d_in, d_out = desc_ports[mid]
        for port in sorted(n["inputs"]):
            if port not in d_in:
                findings.append("C8 %s(%s) input port %r not in descriptor input ports %s"
                                % (n["node_id"], mid, port, sorted(d_in)))
        for port in sorted(n["outputs"]):
            if port not in d_out:
                findings.append("C8 %s(%s) output port %r not in descriptor output ports %s"
                                % (n["node_id"], mid, port, sorted(d_out)))

    # C7 非退化
    if n_edges < MIN_PHASE1_EDGES:
        findings.append("C7 only %d IR phase1 edges (min %d) —— 抽取退化"
                        % (n_edges, MIN_PHASE1_EDGES))
    if n_reg_edges < MIN_PHASE1_EDGES:
        findings.append("C7 only %d registry phase1 edges among IR modules (min %d) —— "
                        "注册表端口图退化" % (n_reg_edges, MIN_PHASE1_EDGES))
    return findings, {"phase1_nodes": len(nodes), "phase1_ir_edges": n_edges,
                      "phase1_registry_edges": n_reg_edges,
                      "phase1_order": [n["node_id"] for n in nodes]}


def collect(repo: pathlib.Path):
    adapters = gc.read_text(repo / REGISTER_CPP, REGISTER_CPP)
    ir_text = gc.read_text(repo / IR_CPP, IR_CPP)
    ledger = gc.load_ledger(repo / LEDGER, LEDGER)
    return parse_registered(adapters), parse_ir(ir_text), ledger


def evaluate_module_set(repo: pathlib.Path):
    """P1–P3：注册表 ↔ IR 模块集合双向一致（台账可豁免既有差异）。"""
    registered, ir_nodes, ledger = collect(repo)
    findings = []
    for mid in sorted(registered - ir_nodes):
        key = "registered_not_in_ir:%s" % mid
        if key in ledger:
            continue
        findings.append(key)
    for mid in sorted(ir_nodes - registered):
        key = "ir_not_registered:%s" % mid
        if key in ledger:
            continue
        findings.append(key)
    return findings, {"registered_count": len(registered), "ir_node_count": len(ir_nodes),
                      "registered_not_in_ir": sorted(registered - ir_nodes),
                      "ir_not_registered": sorted(ir_nodes - registered)}


def evaluate(repo: pathlib.Path):
    findings, extra = evaluate_module_set(repo)
    # C4–C7：节点序与边保真**不**走台账豁免（幻边/序错不得以已知差异盖过）。
    order_findings, order_stats = evaluate_order(repo)
    findings.extend(order_findings)
    extra.update(order_stats)
    return findings, extra


# --------------------------------------------------------------------------- selftest ----
_FIXTURE_ADAPTERS = """
ModuleDescriptor phase2_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.resample";
  return d;
}
ModuleDescriptor p2_coverage_descriptor() {
  ModuleDescriptor d;
  d.module_id = "astrocs.phase2.coverage";
  return d;
}
Result<void> register_phase_modules(ModuleRegistry& registry) {
  auto d2 = phase2_descriptor();
  registry.register_module(d2);
  const std::pair<ModuleDescriptor, int> p2_nodes[] = {
      {p2_coverage_descriptor(), {0}},
  };
  for (const auto& [d, spec] : p2_nodes) { registry.register_module(d); }
  return Result<void>::success();
}
"""
_FIXTURE_IR_OK = """
std::string build_pipeline_ir(const std::vector<int>& phases, const std::string& config_json, std::string* err) {
  const std::vector<std::tuple<std::string, std::string>> chain = {
      {"coverage", "astrocs.phase2.coverage"},
      {"resample", "astrocs.phase2.resample"},
  };
  return "{}";
}
"""
_FIXTURE_IR_BAD = """
std::string build_pipeline_ir(const std::vector<int>& phases, const std::string& config_json, std::string* err) {
  const std::vector<std::tuple<std::string, std::string>> chain = {
      {"coverage", "astrocs.phase2.coverage"},
  };
  return "{}";
}
"""
_LEDGER_EMPTY = {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fixture", "entries": []}


# ── 节点序负例注入 fixture（真实注册表/描述符 + 受控变异的真实 IR）────────────
def _mk_call_span_by_id(text: str, node_id: str):
    idx = text.find('mk("%s"' % node_id)
    if idx < 0:
        raise gc.GateError("fixture: mk(%s) not found" % node_id)
    args_text, end = mk_call_span(text, idx)
    return idx, end, args_text


def _fixture_ir_mutate(mutator):
    """真实 IR（去注释后）→ 变异文本。去注释是必须的: 参数表内含未配平括号的注释。"""
    real = (gc.repo_root() / IR_CPP).read_text(encoding="utf-8")
    return mutator(_strip_line_comments(real))


def _swap_nodes(text: str, a: str, b: str) -> str:
    a0, a1, _ = _mk_call_span_by_id(text, a)
    b0, b1, _ = _mk_call_span_by_id(text, b)
    if a0 > b0:
        a0, a1, b0, b1 = b0, b1, a0, a1
    return text[:a0] + text[b0:b1] + text[a1:b0] + text[a0:a1] + text[b1:]


def _set_node_inputs(text: str, node_id: str, new_inputs: str) -> str:
    i0, i1, args = _mk_call_span_by_id(text, node_id)
    parts = _split_top_level(args)
    parts[2] = new_inputs
    return text[:i0] + "mk(" + ", ".join(parts) + ")" + text[i1:]


def _write_order_fixture(root: pathlib.Path, ir_text: str):
    """真实注册表/描述符/台账 + 给定 IR 文本（覆盖 C4–C7 面）。"""
    for rel in (REGISTER_CPP, PORTS_REGISTRY, LEDGER):
        src = gc.repo_root() / rel
        dst = root / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_text(src.read_text(encoding="utf-8"), encoding="utf-8")
    (root / IR_CPP).parent.mkdir(parents=True, exist_ok=True)
    (root / IR_CPP).write_text(ir_text, encoding="utf-8")


def _write_fixture(root: pathlib.Path, ir_text: str, ledger=None):
    (root / "lib/infrastructure/scheduler/src").mkdir(parents=True, exist_ok=True)
    (root / "lib/infrastructure/cli").mkdir(parents=True, exist_ok=True)
    (root / "eng/ci/ledgers").mkdir(parents=True, exist_ok=True)
    (root / REGISTER_CPP).write_text(_FIXTURE_ADAPTERS, encoding="utf-8")
    (root / IR_CPP).write_text(ir_text, encoding="utf-8")
    import json
    (root / LEDGER).write_text(json.dumps(ledger or _LEDGER_EMPTY), encoding="utf-8")


def _selftest() -> int:
    import json
    import tempfile

    failures = []
    with tempfile.TemporaryDirectory() as td:
        base = pathlib.Path(td)
        cases = []
        order_cases = []
        d_ok = base / "ok"
        _write_fixture(d_ok, _FIXTURE_IR_OK)
        cases.append(("green_parity", False, d_ok))
        d_bad = base / "bad"
        _write_fixture(d_bad, _FIXTURE_IR_BAD)
        cases.append(("red_registered_not_in_ir", True, d_bad))
        ledger = {"ledger_schema": gc.LEDGER_SCHEMA, "ledger_id": "fixture",
                  "entries": [{"id": "registered_not_in_ir:astrocs.phase2.resample",
                               "kind": "known_divergence", "reason": "fixture 已知项",
                               "owner": "fixture", "exit_condition": "fixture 移除"}]}
        d_led = base / "ledgered"
        _write_fixture(d_led, _FIXTURE_IR_BAD, ledger)
        cases.append(("green_ledgered", False, d_led))
        d_extra = base / "extra"
        _write_fixture(d_extra, _FIXTURE_IR_OK.replace(
            '"astrocs.phase2.resample"', '"astrocs.phase2.ghost"'))
        cases.append(("red_ir_not_registered", True, d_extra))
        # ── C4–C7：真实注册表 + 受控变异的真实 IR（正例 + 4 类负例）──────────
        d_order_ok = base / "order_ok"
        _write_order_fixture(d_order_ok, _fixture_ir_mutate(lambda t: t))
        order_cases.append(("green_order_real_ir", False, d_order_ok))
        d_swap = base / "order_swapped"
        _write_order_fixture(d_swap, _fixture_ir_mutate(lambda t: _swap_nodes(t, "psf", "wcs")))
        order_cases.append(("red_order_psf_before_wcs", True, d_swap))
        d_phantom = base / "order_phantom_edge"
        _write_order_fixture(d_phantom, _fixture_ir_mutate(
            lambda t: _set_node_inputs(t, "wcs", '{{"sources", "artifact:p1_sources"}}')))
        order_cases.append(("red_order_phantom_wcs_sources_edge", True, d_phantom))
        d_noprior = base / "order_no_wcs_prior"
        _write_order_fixture(d_noprior, _fixture_ir_mutate(
            lambda t: _set_node_inputs(t, "psf", '{{"cleaned", "artifact:cos"}}')))
        order_cases.append(("red_order_psf_without_wcs_prior", True, d_noprior))
        d_phot_psf = base / "order_phantom_phot_psf"
        _write_order_fixture(d_phot_psf, _fixture_ir_mutate(lambda t: _set_node_inputs(
            t, "phot", '{{"psf", "artifact:p1_psf"}, {"sources", "artifact:p1_sources"},'
                       ' {"wcs", "artifact:p1_wcs"}}')))
        order_cases.append(("red_order_phantom_phot_psf_edge", True, d_phot_psf))
        d_badport = base / "order_bad_port_name"
        _write_order_fixture(d_badport, _fixture_ir_mutate(lambda t: _set_node_inputs(
            t, "psf", '{{"cleaned_typo", "artifact:cos"}, {"wcs", "artifact:p1_wcs"}}')))
        order_cases.append(("red_order_ir_port_not_in_descriptor", True, d_badport))

        d_missing = base / "missing"
        _write_fixture(d_missing, _FIXTURE_IR_OK)
        (d_missing / IR_CPP).unlink()
        try:
            evaluate(d_missing)
            failures.append("missing_anchor_should_raise: expected GateError")
        except gc.GateError:
            print("SELFTEST_PASS missing_anchor (fail-closed GateError)")
        # 负例 fail-closed：台账缺 reason ⇒ GateError
        d_badledger = base / "badledger"
        _write_fixture(d_badledger, _FIXTURE_IR_BAD, {"ledger_schema": gc.LEDGER_SCHEMA,
                      "ledger_id": "fixture", "entries": [{"id": "x", "kind": "k"}]})
        try:
            evaluate(d_badledger)
            failures.append("bad_ledger_should_raise: expected GateError")
        except gc.GateError:
            print("SELFTEST_PASS bad_ledger (fail-closed GateError)")
        rc_module = gc.selftest_main(cases, lambda repo: evaluate_module_set(repo)[0])
        rc_order = gc.selftest_main(order_cases, lambda repo: evaluate(repo)[0])
        rc = 1 if (rc_module or rc_order) else 0
    if failures:
        for item in failures:
            print("SELFTEST_FAIL: " + item)
        return 1
    return rc


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CHK-REGISTRY-IR-PARITY 注册表↔IR 双向一致门")
    ap.add_argument("--repo", default=str(gc.repo_root()))
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)
    if args.self_test:
        return _selftest()
    repo = pathlib.Path(args.repo).resolve()
    try:
        findings, extra = evaluate(repo)
    except gc.GateError as exc:
        print("%s_FAIL: %s" % (CHECK_ID, exc), file=sys.stderr)
        return 2
    if findings:
        gc.print_findings(CHECK_ID, findings)
        gc.report("FAIL", CHECK_ID, findings, extra, args.json_out)
        return 1
    print("%s_PASS registered=%d ir_nodes=%d（双向差集空）"
          % (CHECK_ID, extra["registered_count"], extra["ir_node_count"]))
    gc.report("PASS", CHECK_ID, [], extra, args.json_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
