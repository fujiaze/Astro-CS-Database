#!/usr/bin/env python3
"""DATA-001 artifact manifest 验证器（运行时 schema 层，自包含语义校验）。

职责:
  1. 严格 JSON 解析: 拒绝非标准 NaN/Infinity 字面量（parse_constant 钩子抛错）、
     拒绝对象重复 key; 宽松/损坏 JSON 一律拒绝。
  2. 自包含语义校验（不依赖第三方 jsonschema 库, 保证任何 python3 环境行为一致）:
     - 顶层必填字段齐全(缺字段拒绝) / 多余字段拒绝(additionalProperties=false);
     - manifest_schema const、manifest_version const;
     - artifact_id 词法(pattern);
     - type_id 词法(pattern) + 注册表登记(未知 type 拒绝) + schema_version 匹配;
     - storage_uri 必须为 URI 形态(file/https/run:...), 裸绝对路径/Windows 盘符路径拒绝;
     - content_digest/config_digest: {algorithm=sha256, hex=64 hex};
     - size: 非负 int (负值/NaN 字符串/Infinity 拒绝);
     - producer: module_id+module_build_id 必填, module_id 词法;
     - run: run_id+phase 必填, phase ∈ {phase1,phase2,phase3};
     - node: node_id+pipeline_id 必填;
     - input_digests: 数组, 每项 {artifact_id, digest}, artifact_id 唯一(重复拒绝);
     - status ∈ {COMPLETE,INCOMPLETE,FAILED,CANCELLED,PENDING};
     - created_utc 格式。
  3. 与 contracts/data/artifact_manifest.schema.json 的约束一一对应（schema 为权威文档形态,
     本文件为执行形态; 二者须同步修改——机器检查见 tests/artifact/）。

本文件是 DATA-001 冻结合同的机器侧; DATA-003 接入生产 ArtifactStore 时由 Store 调用同一逻辑。
"""
from __future__ import annotations

import json
import pathlib
import re
import sys
from typing import Any, Callable

REPO = pathlib.Path(__file__).resolve().parents[2]
SCHEMA_PATH = REPO / "contracts" / "data" / "artifact_manifest.schema.json"
REGISTRY_PATH = REPO / "contracts" / "data" / "artifact_types.registry.json"
MANIFEST_SCHEMA_CONST = "astrocs.artifact-manifest/v1"

_HEX64 = re.compile(r"^[0-9a-f]{64}$")
_TYPE_ID_RE = re.compile(r"^astrocs\.[a-z0-9]+\.[a-z0-9_]+\.v[0-9]+$")
_ARTIFACT_ID_RE = re.compile(r"^[A-Za-z0-9_-]+$")
_MODULE_ID_RE = re.compile(r"^astrocs\.[a-z0-9_.-]+$")
_STORAGE_URI_RE = re.compile(r"^(file|https?)://\S+$|^[a-z][a-z0-9]*:[A-Za-z0-9._/+-]+$")
_NODE_ID_RE = re.compile(r"^[a-z][a-z0-9_.-]*$")
_UTC_RE = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$")
_STATUS_SET = {"COMPLETE", "INCOMPLETE", "FAILED", "CANCELLED", "PENDING"}
_PHASE_SET = {"phase1", "phase2", "phase3"}


def _no_nan(*_args: Any) -> None:
    raise ValueError("non-standard JSON constant (NaN/Infinity) is rejected")


def load_strict_json(text: str) -> Any:
    """严格 JSON: 拒绝 NaN/Infinity/-Infinity 与重复 key。"""

    def _obj_hook(pairs):
        seen = set()
        for k, _ in pairs:
            if k in seen:
                raise ValueError(f"duplicate key {k!r}")
            seen.add(k)
        return dict(pairs)

    return json.loads(text, parse_constant=_no_nan, object_pairs_hook=_obj_hook)


def load_registry() -> dict:
    return json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))


def _is_nonneg_int(v: Any) -> bool:
    return isinstance(v, int) and not isinstance(v, bool) and v >= 0


class Validator:
    """自包含语义校验器。无第三方依赖。"""

    def __init__(self) -> None:
        self._registry_types: dict | None = None

    # ── registry 缓存 ──
    def _registry(self) -> dict:
        if self._registry_types is None:
            reg = load_registry()
            self._registry_types = {t["type_id"]: t for t in reg["types"]}
        return self._registry_types

    # ── 入口 ──
    def validate_text(self, text: str) -> tuple[bool, list[str]]:
        try:
            doc = load_strict_json(text)
        except Exception as exc:  # 含 NaN/重复 key/语法错误
            return False, [f"strict json parse failed: {exc}"]
        return self.validate_doc(doc)

    def validate_doc(self, doc: Any) -> tuple[bool, list[str]]:
        if not isinstance(doc, dict):
            return False, ["manifest must be a JSON object"]
        errors: list[str] = []
        self._check(doc, errors)
        return (len(errors) == 0), errors

    # ── 语义检查 ──
    def _check(self, d: dict, errors: list[str]) -> None:
        TOP_REQUIRED = {
            "manifest_schema", "manifest_version", "artifact_id", "type_id",
            "schema_version", "storage_uri", "content_digest", "size",
            "producer", "run", "node", "input_digests", "config_digest",
            "status", "created_utc",
        }
        TOP_ALLOWED = TOP_REQUIRED
        missing = sorted(TOP_REQUIRED - set(d.keys()))
        if missing:
            errors.append(f"missing required field(s): {missing}")
            return  # 缺字段直接拒绝, 不继续深查
        extra = sorted(set(d.keys()) - TOP_ALLOWED)
        if extra:
            errors.append(f"additional property not allowed: {extra}")

        # manifest const
        if d["manifest_schema"] != MANIFEST_SCHEMA_CONST:
            errors.append(f"manifest_schema must be {MANIFEST_SCHEMA_CONST!r}")
        if d["manifest_version"] != 1:
            errors.append("manifest_version must be 1")

        # artifact_id
        if not isinstance(d["artifact_id"], str) or not _ARTIFACT_ID_RE.match(d["artifact_id"]):
            errors.append(f"artifact_id invalid: {d['artifact_id']!r}")

        # type_id 词法 + 注册表
        tid = d["type_id"]
        if not isinstance(tid, str) or not _TYPE_ID_RE.match(tid):
            errors.append(f"type_id lexical invalid: {tid!r}")
        registry = self._registry()
        if tid not in registry:
            errors.append(f"unknown type_id {tid!r}: not in artifact_types.registry.json")
        else:
            sv = d["schema_version"]
            if not isinstance(sv, int) or isinstance(sv, bool):
                errors.append(f"schema_version must be integer: {sv!r}")
            elif sv != registry[tid]["schema_version"]:
                errors.append(
                    f"type_id/schema_version mismatch: {tid} expects v"
                    f"{registry[tid]['schema_version']}, manifest has v{sv}")

        # storage_uri: 禁裸路径
        su = d["storage_uri"]
        if not isinstance(su, str) or not _STORAGE_URI_RE.match(su):
            errors.append(f"storage_uri must be URI form (file:// https:// scheme:...), got {su!r}")

        # content_digest
        self._check_digest(d["content_digest"], "content_digest", errors)
        # config_digest
        self._check_digest(d["config_digest"], "config_digest", errors)

        # size: 非负 int
        if not _is_nonneg_int(d["size"]):
            errors.append(f"size must be non-negative integer: {d['size']!r}")

        # producer
        self._check_producer(d["producer"], errors)
        # run
        self._check_run(d["run"], errors)
        # node
        self._check_node(d["node"], errors)
        # input_digests
        self._check_inputs(d["input_digests"], errors)
        # status
        if d["status"] not in _STATUS_SET:
            errors.append(f"status invalid: {d['status']!r}")
        # created_utc
        cu = d["created_utc"]
        if not isinstance(cu, str) or not _UTC_RE.match(cu):
            errors.append(f"created_utc must be YYYY-MM-DDTHH:MM:SSZ: {cu!r}")

    @staticmethod
    def _check_digest(obj: Any, where: str, errors: list[str]) -> None:
        if not isinstance(obj, dict):
            errors.append(f"{where} must be an object")
            return
        if set(obj.keys()) != {"algorithm", "hex"}:
            errors.append(f"{where} must contain exactly algorithm+hex")
            return
        if obj.get("algorithm") != "sha256":
            errors.append(f"{where}.algorithm must be sha256")
        hx = obj.get("hex")
        if not isinstance(hx, str) or not _HEX64.match(hx):
            errors.append(f"{where}.hex must be 64 hex chars")

    @staticmethod
    def _check_producer(obj: Any, errors: list[str]) -> None:
        if not isinstance(obj, dict):
            errors.append("producer must be an object")
            return
        allowed = {"module_id", "module_build_id", "entry", "science_contract_ids"}
        extra = sorted(set(obj.keys()) - allowed)
        if extra:
            errors.append(f"producer additional property not allowed: {extra}")
        if "module_id" not in obj or not isinstance(obj["module_id"], str) or not _MODULE_ID_RE.match(obj["module_id"]):
            errors.append(f"producer.module_id required (astrocs.*): {obj.get('module_id')!r}")
        if "module_build_id" not in obj or not isinstance(obj["module_build_id"], str) or not obj["module_build_id"]:
            errors.append("producer.module_build_id required non-empty")

    @staticmethod
    def _check_run(obj: Any, errors: list[str]) -> None:
        if not isinstance(obj, dict):
            errors.append("run must be an object")
            return
        allowed = {"run_id", "phase", "session_id"}
        extra = sorted(set(obj.keys()) - allowed)
        if extra:
            errors.append(f"run additional property not allowed: {extra}")
        rid = obj.get("run_id")
        if not isinstance(rid, str) or not rid:
            errors.append("run.run_id required non-empty")
        ph = obj.get("phase")
        if ph not in _PHASE_SET:
            errors.append(f"run.phase must be in {sorted(_PHASE_SET)}: {ph!r}")

    @staticmethod
    def _check_node(obj: Any, errors: list[str]) -> None:
        if not isinstance(obj, dict):
            errors.append("node must be an object")
            return
        allowed = {"node_id", "pipeline_id", "input_port"}
        extra = sorted(set(obj.keys()) - allowed)
        if extra:
            errors.append(f"node additional property not allowed: {extra}")
        nid = obj.get("node_id")
        if not isinstance(nid, str) or not _NODE_ID_RE.match(nid):
            errors.append(f"node.node_id required ([a-z][a-z0-9_.-]*): {nid!r}")
        pid = obj.get("pipeline_id")
        if not isinstance(pid, str) or not pid:
            errors.append("node.pipeline_id required non-empty")

    @staticmethod
    def _check_inputs(obj: Any, errors: list[str]) -> None:
        if not isinstance(obj, list):
            errors.append("input_digests must be an array")
            return
        seen: set[str] = set()
        for i, item in enumerate(obj):
            if not isinstance(item, dict):
                errors.append(f"input_digests[{i}] must be an object")
                continue
            if set(item.keys()) != {"artifact_id", "digest"}:
                errors.append(f"input_digests[{i}] must contain exactly artifact_id+digest")
                continue
            aid = item.get("artifact_id")
            if not isinstance(aid, str) or not aid:
                errors.append(f"input_digests[{i}].artifact_id required non-empty")
            else:
                if aid in seen:
                    errors.append(f"duplicate input artifact_id: {aid!r}")
                seen.add(aid)
            dg = item.get("digest")
            if not isinstance(dg, str) or not _HEX64.match(dg):
                errors.append(f"input_digests[{i}].digest must be 64 hex chars")


def validate_file(path: pathlib.Path) -> tuple[bool, list[str]]:
    return Validator().validate_text(path.read_text(encoding="utf-8"))


def main(argv: list[str] | None = None) -> int:
    import argparse

    ap = argparse.ArgumentParser(description="DATA-001 artifact manifest validator")
    ap.add_argument("files", nargs="+", type=pathlib.Path)
    args = ap.parse_args(argv)
    ok = True
    for f in args.files:
        valid, errs = validate_file(f)
        if valid:
            print(f"PASS {f}")
        else:
            ok = False
            print(f"FAIL {f}")
            for e in errs:
                print(f"  - {e}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
