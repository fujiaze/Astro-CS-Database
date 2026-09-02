#!/usr/bin/env python3
"""DATA-002 phase product exchange 校验器（运行时 schema 层，自包含语义校验，无第三方依赖）。

职责（与 contracts/data/phase_product_exchange.schema.json 一一对应；schema 为权威文档形态，
本文件为执行形态；二者须同步修改——机器检查见 tests/artifact/test_phase_product_exchange.py）：

  1. 严格 JSON 解析: 拒绝 NaN/Infinity 字面量与重复 key（复用 DATA-001 load_strict_json）。
  2. 结构校验（自包含，不依赖 jsonschema 库）:
     - 顶层必填: exchange_schema/exchange_version/product_role/type_id/schema_version/
       origin/artifact_manifest/product_content；多余字段拒绝；
     - product_role ∈ {phase1_product_v1, phase2_mosaic_v1, phase3_planar_fits_v1}，
       role↔type_id↔schema_version 绑定对照本文件同目录 phase_product_exchange_matrix.json
       roles[]（role 不允许与 type 解耦）；
     - type_id 必须登记于 artifact_types.registry.json（复用 DATA-001 语义）且
       schema_version 与注册表一致（缺 schema 语义 → 拒绝）；
     - artifact_manifest: 完整 DATA-001 manifest，status=COMPLETE、content_digest=sha256/64hex
       （缺 manifest / 缺 hash → 拒绝）；
     - product_content: 显式声明 coordinate(icrs/deg)/geometry(format 与 role 匹配)/
       planes(每平面 units/dtype/invalid_policy 必填, units 非空)/invalid_policy
       （缺 units → 拒绝；缺 content → 拒绝）；
     - origin ∈ {astrocs, external_fixture}; external_fixture 允许 run.phase=phase3
       自述但不得携带内部 run 绑定要求。
  3. 无隐式 name binding：本校验器只读交换对象文档字段，绝不读/猜任何文件路径、
     storage_uri 尾段或 artifact_id 派生语义；artifact_id 仅唯一标识。
  4. run 字段仅溯源；绝不要求 producer.run.run_id / artifact_id 与接收方匹配
     （R-NO-RUN-BINDING：Phase2 不要求 Phase1 run ID；Phase3 不要求输入来自 Phase2）。

与 DATA-001 的关系: 本校验器调用 artifact_manifest_validator.Validator 校验内嵌 manifest
（DATA-001 冻结），在其上附加 DATA-002 交换层规则; DATA-001 拒绝项自动传播。

DATA-003 接入生产 ArtifactStore / RT-002 phase-isolated runtime 时，交换资格判定调用本模块
同一逻辑；store 只提供按 storage_uri+digest 打开已校验对象的 reader。
"""
from __future__ import annotations

import json
import pathlib
import re
import sys
from typing import Any

REPO = pathlib.Path(__file__).resolve().parents[2]
SCHEMA_PATH = REPO / "contracts" / "data" / "phase_product_exchange.schema.json"
MATRIX_PATH = REPO / "contracts" / "data" / "phase_product_exchange_matrix.json"
EXCHANGE_SCHEMA_CONST = "astrocs.phase-product-exchange/v1"
CONTENT_SCHEMA_CONST = "astrocs.phase-product-content/v1"

sys.path.insert(0, str(REPO / "runtime" / "artifact_store"))
from artifact_manifest_validator import load_strict_json, load_registry  # noqa: E402

_ROLE_SET = {"phase1_product_v1", "phase2_mosaic_v1", "phase3_planar_fits_v1"}
_ORIGIN_SET = {"astrocs", "external_fixture"}
_HEX64 = re.compile(r"^[0-9a-f]{64}$")
_TYPE_ID_RE = re.compile(r"^astrocs\.[a-z0-9]+\.[a-z0-9_]+\.[v][0-9]+$")
# 可接受 product_content.plane.plane_id 集合（与 schema $defs.plane enum 一致）
_PLANE_ID_SET = {"signal", "support", "variance", "ivar", "mask"}
_DTYPE_SET = {"float32", "float64", "u8"}
_INVALID_POLICY_SET = {"nan_or_support_le_0"}
_GEOMETRY_FORMAT_SET = {"hips", "fits"}

def load_matrix() -> dict:
    """载入兼容矩阵真源（结构简单，直接解析；机器校验在测试内做 schema 一致性）。"""
    return json.loads(MATRIX_PATH.read_text(encoding="utf-8"))


class PhaseProductExchangeValidator:
    """DATA-002 交换对象校验器。自包含，无第三方依赖。"""

    def __init__(self) -> None:
        self._registry_types: dict | None = None
        self._matrix: dict | None = None
        self._role_binding: dict[str, dict] | None = None

    # ── 真源缓存 ──
    def _registry(self) -> dict:
        if self._registry_types is None:
            reg = load_registry()
            self._registry_types = {t["type_id"]: t for t in reg["types"]}
        return self._registry_types

    def _matrix_roles(self) -> dict[str, dict]:
        if self._matrix is None:
            self._matrix = load_matrix()
            self._role_binding = {r["role"]: r for r in self._matrix["roles"]}
        assert self._role_binding is not None
        return self._role_binding

    # ── 入口 ──
    def validate_text(self, text: str) -> tuple[bool, list[str]]:
        try:
            doc = load_strict_json(text)
        except Exception as exc:
            return False, [f"strict json parse failed: {exc}"]
        return self.validate_doc(doc)

    def validate_doc(self, doc: Any) -> tuple[bool, list[str]]:
        if not isinstance(doc, dict):
            return False, ["exchange object must be a JSON object"]
        errors: list[str] = []
        self._check(doc, errors)
        return (len(errors) == 0), errors

    # ── 语义检查 ──
    def _check(self, d: dict, errors: list[str]) -> None:
        TOP_REQUIRED = {
            "exchange_schema", "exchange_version", "product_role", "type_id",
            "schema_version", "origin", "artifact_manifest", "product_content",
        }
        missing = sorted(TOP_REQUIRED - set(d.keys()))
        if missing:
            errors.append(f"missing required field(s): {missing}")
            return  # 缺字段直接拒绝
        extra = sorted(set(d.keys()) - TOP_REQUIRED)
        if extra:
            errors.append(f"additional property not allowed: {extra}")

        # exchange 标识
        if d["exchange_schema"] != EXCHANGE_SCHEMA_CONST:
            errors.append(f"exchange_schema must be {EXCHANGE_SCHEMA_CONST!r}")
        if d["exchange_version"] != 1:
            errors.append("exchange_version must be 1")

        # product_role / type_id / schema_version 绑定（对照矩阵真源）
        role = d["product_role"]
        if role not in _ROLE_SET:
            errors.append(f"product_role must be in {sorted(_ROLE_SET)}: {role!r}")
            return  # role 非法则无法继续绑定检查
        tid = d["type_id"]
        if not isinstance(tid, str) or not _TYPE_ID_RE.match(tid):
            errors.append(f"type_id lexical invalid: {tid!r}")
        sv = d["schema_version"]
        if not isinstance(sv, int) or isinstance(sv, bool) or sv < 1:
            errors.append(f"schema_version must be positive integer: {sv!r}")

        matrix = self._matrix_roles()
        if role not in matrix:
            errors.append(f"product_role {role!r} not bound in phase_product_exchange_matrix.json")
            return
        bind = matrix[role]
        if tid != bind["type_id"]:
            errors.append(
                f"product_role/type_id mismatch: {role} requires {bind['type_id']!r}, "
                f"manifest declares {tid!r} (role 不允许与 type 解耦)")
        # registry 登记 + schema_version 匹配（缺 schema 语义 → 拒绝）
        registry = self._registry()
        if tid in registry:
            if sv != registry[tid]["schema_version"]:
                errors.append(
                    f"type_id/schema_version mismatch: {tid} expects v{registry[tid]['schema_version']}, "
                    f"exchange has v{sv}")
        else:
            errors.append(f"unknown type_id {tid!r}: not in artifact_types.registry.json")

        # origin
        origin = d["origin"]
        if origin not in _ORIGIN_SET:
            errors.append(f"origin must be in {sorted(_ORIGIN_SET)}: {origin!r}")

        # manifest（DATA-001 完整校验 + COMPLETE + hash）
        self._check_manifest(d["artifact_manifest"], errors)

        # product_content（units/coordinate/dtype/planes 显式声明；缺 units 拒绝）
        self._check_content(d["product_content"], role, errors)

        # R-NO-RUN-BINDING: 本校验器从不要求 run_id 解析/匹配；role/format 判定只用字段
        # （无 name binding）。此处不产生任何 error —— 只保证上面各检查不读文件路径。
        # R-NO-NAME-BINDING: artifact_id / storage_uri 只做词法存在性校验（DATA-001），
        # 不作为语义/资格来源 —— 无代码路径会把它们当 role/schema 依据。

    # ── manifest 检查 ──
    @staticmethod
    def _check_manifest(m: Any, errors: list[str]) -> None:
        if not isinstance(m, dict):
            errors.append("artifact_manifest must be a DATA-001 artifact manifest object")
            return
        # 复用 DATA-001 完整校验（缺字段/NaN/重复 producer/未知 type 全传播）
        from artifact_manifest_validator import Validator as ManifestValidator
        ok, merrs = ManifestValidator().validate_doc(m)
        if not ok:
            for e in merrs:
                errors.append(f"artifact_manifest: {e}")
            return
        # 交换资格附加要求
        if m["status"] != "COMPLETE":
            errors.append(f"exchange requires artifact_manifest.status=COMPLETE, got {m['status']!r}")
        cd = m.get("content_digest", {})
        if not isinstance(cd, dict) or cd.get("algorithm") != "sha256" or not isinstance(cd.get("hex"), str) or not _HEX64.match(cd.get("hex", "")):
            errors.append("exchange requires content_digest sha256/64hex (缺 hash 拒绝)")

    # ── product_content 检查 ──
    def _check_content(self, c: Any, role: str, errors: list[str]) -> None:
        bind = self._matrix_roles().get(role, {})
        if not isinstance(c, dict):
            errors.append("product_content required (缺内容证据/units 声明拒绝)")
            return
        extra = sorted(set(c.keys()) - {"content_schema", "coordinate", "geometry", "planes", "invalid_policy"})
        if extra:
            errors.append(f"product_content additional property not allowed: {extra}")
        if c.get("content_schema") != CONTENT_SCHEMA_CONST:
            errors.append(f"product_content.content_schema must be {CONTENT_SCHEMA_CONST!r}")

        # coordinate
        coord = c.get("coordinate")
        if not isinstance(coord, dict):
            errors.append("product_content.coordinate required")
        else:
            if coord.get("frame") != "icrs":
                errors.append("product_content.coordinate.frame must be 'icrs' (唯一允许 frame)")
            if coord.get("ra_unit") != "deg" or coord.get("dec_unit") != "deg":
                errors.append("product_content.coordinate ra/dec unit must be 'deg'")
            cextra = sorted(set(coord.keys()) - {"frame", "ra_unit", "dec_unit"})
            if cextra:
                errors.append(f"coordinate additional property not allowed: {cextra}")

        # geometry: format 必须与 role 绑定一致
        geom = c.get("geometry")
        if not isinstance(geom, dict):
            errors.append("product_content.geometry required")
        else:
            fmt = geom.get("format")
            expected = bind.get("content_format")
            if fmt not in _GEOMETRY_FORMAT_SET:
                errors.append(f"geometry.format must be in {sorted(_GEOMETRY_FORMAT_SET)}: {fmt!r}")
            elif expected is not None and fmt != expected:
                errors.append(f"geometry.format/role mismatch: {role} requires {expected!r}, got {fmt!r}")
            if fmt == "hips":
                hips = geom.get("hips")
                if not isinstance(hips, dict) or hips.get("ordering") != "nested" or not isinstance(hips.get("tile_width"), int):
                    errors.append("geometry.hips requires ordering='nested' + integer tile_width")
            elif fmt == "fits":
                fits = geom.get("fits")
                if not isinstance(fits, dict) or fits.get("projection") != "TAN" or fits.get("wcs") != "present":
                    errors.append("geometry.fits requires projection='TAN' + wcs='present'")

        # planes: 每平面必填 units/dtype/invalid_policy；units 非空（缺 units 拒绝）
        planes = c.get("planes")
        if not isinstance(planes, list) or len(planes) == 0:
            errors.append("product_content.planes required non-empty (显式 units/plane 声明)")
        else:
            seen: set[str] = set()
            for i, pl in enumerate(planes):
                if not isinstance(pl, dict):
                    errors.append(f"planes[{i}] must be an object")
                    continue
                pextra = sorted(set(pl.keys()) - {"plane_id", "units", "dtype", "invalid_policy"})
                if pextra:
                    errors.append(f"planes[{i}] additional property not allowed: {pextra}")
                pid = pl.get("plane_id")
                if pid not in _PLANE_ID_SET:
                    errors.append(f"planes[{i}].plane_id must be in {sorted(_PLANE_ID_SET)}: {pid!r}")
                else:
                    if pid in seen:
                        errors.append(f"duplicate plane_id: {pid!r}")
                    seen.add(pid)
                units = pl.get("units")
                if not isinstance(units, str) or not units or not units.strip():
                    errors.append(f"planes[{i}].units required non-empty (缺 units 拒绝)")
                elif units != units.strip():
                    errors.append(f"planes[{i}].units must not have surrounding whitespace")
                dt = pl.get("dtype")
                if dt not in _DTYPE_SET:
                    errors.append(f"planes[{i}].dtype must be in {sorted(_DTYPE_SET)}: {dt!r}")
                ip = pl.get("invalid_policy")
                if not isinstance(ip, str) or not ip:
                    errors.append(f"planes[{i}].invalid_policy required non-empty")
            # 最小平面集（role 绑定；缺核心平面拒绝）
            need = set(bind.get("min_planes", []))
            have = seen
            miss = sorted(need - have)
            if miss:
                errors.append(f"{role} requires min planes {sorted(need)}, missing: {miss}")

        # invalid_policy 全局
        ipg = c.get("invalid_policy")
        if ipg not in _INVALID_POLICY_SET:
            errors.append(f"product_content.invalid_policy must be in {sorted(_INVALID_POLICY_SET)}: {ipg!r}")


def validate_file(path: pathlib.Path) -> tuple[bool, list[str]]:
    return PhaseProductExchangeValidator().validate_text(path.read_text(encoding="utf-8"))


def main(argv: list[str] | None = None) -> int:
    import argparse

    ap = argparse.ArgumentParser(description="DATA-002 phase product exchange validator")
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
