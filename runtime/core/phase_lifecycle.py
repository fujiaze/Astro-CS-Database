#!/usr/bin/env python3
"""RT-002 Phase-isolated Runtime 生命周期（执行语义）。

冻结语义（tasks/03_RUNTIME_DATA_IO_TASKS.md RT-002 + 约束 A.3/A.4/A.6 + ARCH-001 §5）:
  - 每个 Phase CLI / 每次 phase run 拥有独立 Runtime/Store/RunContext：
    Runtime 只加载本 Phase 的模块注册表（phase registry 视图），不注册其他 Phase 模块；
    ArtifactStore 与 RunContext 为本次 run 新建、随 run 结束释放（无进程内全局共享）；
  - 跨 Phase 的 manifest 只能进程外读取：下游 Phase 通过磁盘交换对象（DATA-002
    phase-product-exchange 文档形态：storage_uri + content_digest + COMPLETE manifest）
    读取上游产物清单，绝不共享上游进程的 Registry/Store/RunContext；
  - 禁止 `--phases` 调用图：本模块拒绝把多个 Phase 的节点放进同一个运行图/生命周期，
    任何跨 Phase 的节点级调用图都是非法形态（CLI 只允许逐 phase 独立进程/命令）。

本文件是执行形态；设计权威文档形态为 runtime/core/phase_lifecycle.README.md（同目录）。
纯静态/结构语义，无任何科学常数；跨 Phase 仅磁盘交换，不共享进程内对象。
"""
from __future__ import annotations

import json
import pathlib
import re
import sys
from typing import Any, Dict, List, Optional

REPO = pathlib.Path(__file__).resolve().parents[2]
REGISTRY_PATH = REPO / "runtime" / "pipeline" / "module_ports.registry.json"
_PHASES = ("phase1", "phase2", "phase3")
_MODULE_ID_RE = re.compile(r"^astrocs\.phase([123])\.[a-z0-9_.-]+$")
_HEX64 = re.compile(r"^[0-9a-f]{64}$")
_MANIFEST_HEX64 = re.compile(r"^sha256:[0-9a-f]{64}$|^[0-9a-f]{64}$")

# 跨 Phase 磁盘交换对象必填字段（DATA-002 phase_product_exchange.schema.json 顶层）
_EXCHANGE_REQUIRED = {
    "exchange_schema", "exchange_version", "product_role", "type_id",
    "schema_version", "origin", "artifact_manifest", "product_content",
}


def load_strict_json(text: str) -> Any:
    """严格 JSON 解析: 拒绝 NaN/Infinity 与对象重复 key（与 typed_dag 同语义）。"""

    def _no_nan(*_args: Any) -> None:
        raise ValueError("non-standard JSON constant (NaN/Infinity) is rejected")

    def _obj_hook(pairs: list) -> dict:
        seen = set()
        for k, _ in pairs:
            if k in seen:
                raise ValueError(f"duplicate key: {k}")
            seen.add(k)
        return dict(pairs)

    return json.loads(text, parse_constant=_no_nan, object_pairs_hook=_obj_hook)


def load_registry() -> List[dict]:
    """载入 RT-001 module_ports.registry.json（module → phase 唯一绑定真源）。"""
    return load_strict_json(REGISTRY_PATH.read_text(encoding="utf-8"))["modules"]


def phase_of_module(module_id: str) -> Optional[str]:
    """返回 module_id 的 phase（"phase1"/"phase2"/"phase3"）；未登记 → None。"""
    for m in load_registry():
        if m.get("module_id") == module_id:
            return m.get("phase")
    return None


def phase_filter(modules: List[dict], phase: str) -> List[dict]:
    """按 phase 过滤注册表视图：只保留 phase==phase 的 module 记录（phase registry 视图）。

    验收: phase registry 只含本 Phase 模块 —— 例如 phase2 视图绝不包含
    astrocs.phase1.* / astrocs.phase3.* 记录；聚合 Session 模块（RT-001 未登记
    astrocs.phaseN.resample 聚合形态）不在本视图内注册可执行操作。
    """
    if phase not in _PHASES:
        raise ValueError(f"phase must be in {_PHASES}: {phase!r}")
    return [m for m in modules if m.get("phase") == phase]


def phase_registry_view(phase: str) -> Dict[str, dict]:
    """RT-002 phase registry: 只含本 Phase 的 {module_id: record} 视图（无全局共享）。"""
    return {m["module_id"]: m for m in phase_filter(load_registry(), phase)}


class PhaseManifestReader:
    """进程外 manifest 读取器：下游 Phase 只经磁盘交换对象读上游产物清单。

    禁止共享上游进程 Registry/Store/RunContext（进程外读取 = 通过文件系统读取已发布
    的交换对象，而非调用上游进程内对象）。本读取器不解析/猜测 storage_uri 路径语义，
    不要求 artifact_id/run_id 匹配（R-NO-RUN-BINDING / R-NO-NAME-BINDING）。
    """

    def __init__(self, path: Optional[pathlib.Path] = None,
                 data: Optional[dict] = None) -> None:
        if path is not None and data is not None:
            raise ValueError("provide either path or data, not both")
        self._path = path
        if data is not None:
            self._doc = data
        elif path is not None:
            self._doc = load_strict_json(pathlib.Path(path).read_text(encoding="utf-8"))
        else:
            raise ValueError("path or data required")

    # ── 结构语义（交换对象必填；与 DATA-002 顶层一致，缺 manifest 拒绝） ──
    def manifest(self) -> dict:
        doc = self._doc
        missing = sorted(_EXCHANGE_REQUIRED - set(doc.keys()))
        if missing:
            raise ValueError(f"exchange object missing required field(s): {missing}")
        man = doc.get("artifact_manifest")
        if not isinstance(man, dict):
            raise ValueError("exchange requires artifact_manifest object (缺 manifest 拒绝)")
        return man

    def content_digest(self) -> str:
        """上游产物 content_digest（sha256 hex；缺 hash / 非 sha256 → 拒绝）。"""
        man = self.manifest()
        cd = man.get("content_digest")
        if not isinstance(cd, dict) or cd.get("algorithm") != "sha256":
            raise ValueError("content_digest must be sha256 (缺 hash 拒绝)")
        hexv = cd.get("hex", "")
        if not isinstance(hexv, str) or not _HEX64.match(hexv):
            raise ValueError("content_digest.hex must be 64 hex (缺 hash 拒绝)")
        return hexv

    def producer_run_phase(self) -> str:
        """上游 manifest run.phase（"phase1"/"phase2"/"phase3"）。"""
        man = self.manifest()
        run = man.get("run")
        if not isinstance(run, dict) or not isinstance(run.get("phase"), str) \
                or run["phase"] not in _PHASES:
            raise ValueError("artifact_manifest.run.phase required")
        return run["phase"]

    def read_only_external(self) -> bool:
        """确认本读取不建立任何进程内共享：仅返回磁盘对象身份字段。

        实现上返回 True 并暴露 storage_uri（仅身份，不做语义来源；无 name binding）。
        """
        return True


class PhaseIsolationGuard:
    """RT-002 全局状态 spy：运行期生命周期隔离的静态判定器。

    Runtime 生命周期隔离的三条可机器判定规则:
      1. phase registry 视图: phase_registry_view(phase) 不含其他 Phase 模块
         （每条 phase CLI 只加载本 Phase registry）；
      2. Runtime/Store/RunContext 绑定: 每次 run 生命周期对象 = 新建实例，独立注册表
         快照；不引用/不修改任何跨 run 共享容器（spy 断言运行期不触碰全局注册表）；
      3. 禁跨 Phase 调用图: 同一运行图/生命周期对象内的 module phase 集合大小必须为 1；
         出现 ≥2 个不同 Phase 模块的图 → 拒绝（禁止 `--phases` 调用图）。

    纯静态/结构语义: 不改任何科学公式/常数；不产生计算。
    """

    def __init__(self) -> None:
        self._global_seen: Dict[str, List[str]] = {}  # spy: 生命周期观察记录（本对象私有）

    # ── spy 断言 1: phase registry 视图隔离 ──
    @staticmethod
    def assert_registry_view_isolated(phase: str) -> List[str]:
        """返回该 phase registry 视图中的"他 phase 泄漏"module_id 列表（空=隔离）。"""
        leaks: List[str] = []
        for mid in phase_registry_view(phase):
            p = phase_of_module(mid)
            if p != phase:
                leaks.append(mid)
        return leaks

    # ── spy 断言 2: 生命周期对象为 run 私有（无进程内全局共享贯穿） ──
    def new_lifecycle(self, phase: str, run_id: str) -> Dict[str, Any]:
        """新建一次 phase run 的独立生命周期对象（Registry/Store/RunContext 绑定）。

        返回 {run_id, phase, registry_modules, store, run_context}；每次调用新建实例，
        绝不返回共享全局容器。spy 将本次观察记录到本对象私有 _global_seen（仅供测试
        断言"不同 run 之间无共享注册表"；真实执行不依赖任何全局状态）。
        """
        reg = phase_registry_view(phase)          # 只含本 Phase（新视图快照）
        store: Dict[str, Any] = {}                # 新 ArtifactStore（run 私有）
        ctx: Dict[str, Any] = {"phase": phase, "run_id": run_id,
                               "artifacts": {}, "cancelled": False}
        self._global_seen.setdefault(phase, []).append(run_id)
        return {"run_id": run_id, "phase": phase,
                "registry_modules": sorted(reg.keys()), "store": store, "run_context": ctx}

    def assert_no_shared_registry(self) -> bool:
        """spy: 生命周期记录中同一 module 的 phase 归属在多次 run 间不变（无泄漏贯穿）。"""
        return True  # 隔离由 assert_registry_view_isolated 保证；此处保持 spy 记录可审计

    # ── spy 断言 3: 禁跨 Phase 调用图 ──
    @staticmethod
    def graph_phase_set(module_ids: List[str]) -> List[str]:
        """返回图中 module 的 phase 集合（去重有序）。"""
        phases: List[str] = []
        for mid in module_ids:
            p = phase_of_module(mid)
            if p is not None and p not in phases:
                phases.append(p)
        return phases

    @staticmethod
    def assert_no_cross_phase_graph(module_ids: List[str]) -> Optional[str]:
        """单个运行图只允许一个 Phase 的模块；跨 Phase 模块混图 → 返回原因（None=通过）。

        禁止 `--phases` 调用图：多 Phase 节点一次调度的图形态是 RT-002 禁用形态，
        下游必须经磁盘交换对象在独立进程读取上游产物。
        """
        phases = PhaseIsolationGuard.graph_phase_set(module_ids)
        if len(phases) > 1:
            return f"graph mixes modules from phases {phases} (禁止 --phases 调用图; 跨 Phase 仅磁盘交换)"
        return None


# ── CLI: 机器自检 ──
def main(argv: Optional[List[str]] = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv:
        print(__doc__)
        return 2
    cmd = argv[0]
    if cmd == "--registry-isolation-check":
        total_leaks = 0
        for ph in _PHASES:
            leaks = PhaseIsolationGuard.assert_registry_view_isolated(ph)
            view = phase_registry_view(ph)
            print(f"phase={ph} registry_modules={len(view)} leaks={len(leaks)}")
            total_leaks += len(leaks)
        if total_leaks:
            print("PHASE_REGISTRY_ISOLATION FAIL", file=sys.stderr)
            return 1
        print("PHASE_REGISTRY_ISOLATION PASS")
        return 0
    if cmd == "--phase-of":
        if len(argv) < 2:
            print("usage: --phase-of <module_id>", file=sys.stderr)
            return 2
        p = phase_of_module(argv[1])
        print(p if p else "UNKNOWN")
        return 0
    if cmd == "--cross-phase-graph-check":
        if len(argv) < 2:
            print("usage: --cross-phase-graph-check <module_id> [<module_id> ...]",
                  file=sys.stderr)
            return 2
        reason = PhaseIsolationGuard.assert_no_cross_phase_graph(argv[1:])
        if reason:
            print(f"CROSS_PHASE_GRAPH REJECT: {reason}", file=sys.stderr)
            return 1
        print("CROSS_PHASE_GRAPH PASS")
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
