#!/usr/bin/env python3
"""DATA-002 phase product exchange schema 正/负测（含验收映射）。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md DATA-002):
  - D1 三阶段产品矩阵            → test_example_products_accept / schema+matrix 结构
  - D2 Phase3 可接受外部 fixture → test_external_fixture_phase3_accepted
  - D3 Phase2 不要求 Phase1 run ID → test_phase2_input_no_run_id_dependency
  - D4a 缺 manifest 拒绝        → test_missing_manifest_rejected
  - D4b 缺 hash 拒绝            → test_missing_hash_rejected
  - D4c 缺 schema 拒绝          → test_missing_schema_rejected (role↔type 解耦/未登记)
  - D4d 缺 units 拒绝           → test_missing_units_rejected
  - D5 无隐式 artifact name binding → test_no_implicit_name_binding
  - 结构附加: status!=COMPLETE/重复 plane/最小平面集/format-role 不匹配/非 icrs/多余字段/非法 origin
"""
from __future__ import annotations

import copy
import json
import pathlib
import sys
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "runtime" / "artifact_store"))

from phase_product_exchange_validator import PhaseProductExchangeValidator  # noqa: E402

EX = REPO / "contracts" / "data" / "examples"
EX_P1 = EX / "phase1_product_v1.example.json"
EX_P2 = EX / "phase2_mosaic_v1.example.json"
EX_P3 = EX / "phase3_planar_fits_v1.example.json"
EX_FX = EX / "external_fixture_hips.example.json"

_ROLE_TYPE = {
    "phase1_product_v1": "astrocs.phase1.frame_hips.v1",
    "phase2_mosaic_v1": "astrocs.phase2.mosaic_hips.v1",
    "phase3_planar_fits_v1": "astrocs.phase3.planar_fits.v1",
}


def load(p: pathlib.Path) -> dict:
    return json.loads(p.read_text(encoding="utf-8"))


def base(role: str = "phase2_mosaic_v1") -> dict:
    """从 phase2 example 构造可改 base（phase2 min planes=3，便于做负测精简）。"""
    d = load(EX_P2)
    if role != "phase2_mosaic_v1":
        d["product_role"] = role
        d["type_id"] = _ROLE_TYPE[role]
        # phase1 需 variance 平面；phase3 需 fits 几何
        if role == "phase1_product_v1":
            d["product_content"]["planes"].append({
                "plane_id": "variance", "units": "ADU^2", "dtype": "float32",
                "invalid_policy": "nan_or_support_le_0"})
        elif role == "phase3_planar_fits_v1":
            d["product_content"]["geometry"] = {
                "format": "fits", "fits": {"projection": "TAN", "wcs": "present"}}
    return d


def validate(doc) -> tuple[bool, list[str]]:
    return PhaseProductExchangeValidator().validate_doc(doc)


class TestExamplesAccept(unittest.TestCase):
    """D1/D2 正测：4 个示例（含外部 fixture）全部通过。"""

    def test_example_phase1_product_accept(self):
        ok, errs = validate(load(EX_P1))
        self.assertTrue(ok, errs)

    def test_example_phase2_mosaic_accept(self):
        ok, errs = validate(load(EX_P2))
        self.assertTrue(ok, errs)

    def test_example_phase3_planar_fits_accept(self):
        ok, errs = validate(load(EX_P3))
        self.assertTrue(ok, errs)

    def test_external_fixture_phase3_accepted(self):
        """D2: Phase3 可接受外部 fixture（origin=external_fixture 完整证据对象）。"""
        ok, errs = validate(load(EX_FX))
        self.assertTrue(ok, f"外部 fixture 应通过: {errs}")
        doc = load(EX_FX)
        self.assertEqual(doc["origin"], "external_fixture")
        # fixture 是 phase3 输入形态的 hips（phase2 mosaic content shape）
        self.assertEqual(doc["product_content"]["geometry"]["format"], "hips")


class TestRunIdIndependence(unittest.TestCase):
    """D3: Phase2 不要求 Phase1 run ID；Phase3 不要求输入来自 Phase2。"""

    def test_phase2_input_no_run_id_dependency(self):
        """phase1 产品 manifest.run.run_id 与接收方无关 → 仍 PASS。"""
        d = base("phase1_product_v1")
        # 换成与 phase2 完全无关的 run_id/session（模拟另一进程/时间产生）
        d["artifact_manifest"]["run"] = {
            "run_id": "unrelated-run-9f3c", "phase": "phase1", "session_id": "s-other"}
        d["artifact_manifest"]["artifact_id"] = "frame-from-another-run-000042"
        ok, errs = validate(d)
        self.assertTrue(ok, f"Phase2 输入不得要求 Phase1 run ID 匹配: {errs}")

    def test_phase3_input_accepts_phase1_run_context(self):
        """phase3 输入可带 phase1 producer run（E-P1-OUT-P3-IN）→ 仍 PASS。"""
        d = base("phase1_product_v1")
        # 保持 phase1 run 上下文（phase=phase1），作为 phase3 输入形态验证
        ok, errs = validate(d)
        self.assertTrue(ok, f"phase3 输入不要求 phase2 run 上下文: {errs}")

    def test_phase3_input_accepts_mosaic_with_arbitrary_run(self):
        d = base("phase2_mosaic_v1")
        d["artifact_manifest"]["run"] = {"run_id": "zzz-999", "phase": "phase2", "session_id": "s9"}
        ok, errs = validate(d)
        self.assertTrue(ok, errs)


class TestNoImplicitNameBinding(unittest.TestCase):
    """D5: 无隐式 artifact name binding。"""

    def test_artifact_id_arbitrary_does_not_affect_qualification(self):
        """artifact_id 只是稳定标识，不参与资格/语义判定。"""
        d = base("phase2_mosaic_v1")
        for aid in ["completely-unrelated-id-42", "0", "x" * 128]:
            d["artifact_manifest"]["artifact_id"] = aid
            ok, errs = validate(d)
            self.assertTrue(ok, f"artifact_id={aid!r} 不得影响资格: {errs}")

    def test_storage_uri_does_not_drive_role_or_semantics(self):
        """storage_uri 尾段/目录名不参与角色或单位判定；换合法 URI 仍 PASS。"""
        d = base("phase2_mosaic_v1")
        for uri in ["file:///data/some/other/dir/mosaic", "https://cdn.example.org/x/y/hips"]:
            d["artifact_manifest"]["storage_uri"] = uri
            ok, errs = validate(d)
            self.assertTrue(ok, f"storage_uri={uri!r} 不得影响资格: {errs}")

    def test_type_id_drives_role_not_manifest_producer_name(self):
        """角色判定依据 product_role+type_id，而非 producer.module_id 名称。"""
        d = base("phase2_mosaic_v1")
        d["artifact_manifest"]["producer"]["module_id"] = "astrocs.renamed.module"
        ok, errs = validate(d)
        self.assertTrue(ok, f"producer 名称不得影响角色: {errs}")

    def test_role_switch_rejected_without_type_switch(self):
        """role 变而 type 不变 → 拒（name/role 不允许与 type 解耦的反向）。"""
        d = base("phase2_mosaic_v1")
        d["product_role"] = "phase3_planar_fits_v1"  # type_id 仍 mosaic → mismatch
        ok, errs = validate(d)
        self.assertFalse(ok, "role/type 解耦应被拒")
        self.assertTrue(any("mismatch" in e for e in errs), errs)


class TestMissingEvidenceRejected(unittest.TestCase):
    """D4: 缺 manifest / hash / schema / units 拒绝。"""

    def test_missing_manifest_rejected(self):
        """D4a 缺 manifest 拒绝。"""
        d = base()
        del d["artifact_manifest"]
        ok, errs = validate(d)
        self.assertFalse(ok, "缺 artifact_manifest 应被拒")
        self.assertTrue(any("missing required" in e for e in errs), errs)

    def test_manifest_missing_digest_rejected(self):
        """D4b-1 manifest 缺 content_digest → 拒绝。"""
        d = base()
        del d["artifact_manifest"]["content_digest"]
        ok, errs = validate(d)
        self.assertFalse(ok, "缺 content_digest 应被拒")
        self.assertTrue(any("content_digest" in e for e in errs), errs)

    def test_manifest_bad_hash_rejected(self):
        """D4b-2 content_digest 非 sha256/64hex → 拒绝。"""
        d = base()
        d["artifact_manifest"]["content_digest"] = {"algorithm": "sha256", "hex": "zz"}
        ok, errs = validate(d)
        self.assertFalse(ok, "非法 hash 应被拒")

    def test_manifest_not_complete_rejected(self):
        d = base()
        d["artifact_manifest"]["status"] = "INCOMPLETE"
        ok, errs = validate(d)
        self.assertFalse(ok, "非 COMPLETE manifest 不得作为交换输入")
        self.assertTrue(any("COMPLETE" in e for e in errs), errs)

    def test_missing_schema_rejected_role_type_decouple(self):
        """D4c-1 role↔type 解耦 → 拒绝。"""
        d = base()
        d["type_id"] = "astrocs.phase1.frame_hips.v1"  # role=phase2_mosaic_v1 但 type=phase1
        ok, errs = validate(d)
        self.assertFalse(ok, "role/type 解耦应被拒")
        self.assertTrue(any("mismatch" in e for e in errs), errs)

    def test_missing_schema_rejected_unregistered_type(self):
        """D4c-2 未登记 type_id → 拒绝（缺 schema 语义）。"""
        d = base()
        d["type_id"] = "astrocs.phase9.mystery.v1"
        ok, errs = validate(d)
        self.assertFalse(ok, "未登记 type_id 应被拒")
        self.assertTrue(any("unknown type_id" in e for e in errs), errs)

    def test_schema_version_mismatch_rejected(self):
        d = base()
        d["schema_version"] = 2
        ok, errs = validate(d)
        self.assertFalse(ok, "schema_version 与 registry 不一致应被拒")

    def test_missing_units_rejected(self):
        """D4d 任一 plane 缺 units → 拒绝。"""
        d = base()
        del d["product_content"]["planes"][0]["units"]
        ok, errs = validate(d)
        self.assertFalse(ok, "plane 缺 units 应被拒")
        self.assertTrue(any("units required" in e for e in errs), errs)

    def test_empty_units_rejected(self):
        d = base()
        d["product_content"]["planes"][0]["units"] = ""
        ok, errs = validate(d)
        self.assertFalse(ok, "空 units 应被拒")

    def test_whitespace_units_rejected(self):
        d = base()
        d["product_content"]["planes"][0]["units"] = "  ADU  "
        ok, errs = validate(d)
        self.assertFalse(ok, "带首尾空白的 units 应被拒")

    def test_missing_product_content_rejected(self):
        d = base()
        del d["product_content"]
        ok, errs = validate(d)
        self.assertFalse(ok, "缺 product_content（units 载体）应被拒")

    def test_manifest_missing_required_field_rejected(self):
        """DATA-001 缺字段经传播拒绝（交换对象内 manifest 不完整）。"""
        d = base()
        del d["artifact_manifest"]["created_utc"]
        ok, errs = validate(d)
        self.assertFalse(ok, "manifest 缺 DATA-001 必填字段应被拒")
        self.assertTrue(any("missing required" in e for e in errs), errs)


class TestStructuralRejections(unittest.TestCase):
    """结构附加负测。"""

    def test_extra_top_field_rejected(self):
        d = base()
        d["sneaky"] = 1
        ok, errs = validate(d)
        self.assertFalse(ok, "多余顶层字段应被拒")

    def test_bad_origin_rejected(self):
        d = base()
        d["origin"] = "mystery"
        ok, errs = validate(d)
        self.assertFalse(ok, "非法 origin 应被拒")

    def test_non_icrs_rejected(self):
        d = base()
        d["product_content"]["coordinate"]["frame"] = "galactic"
        ok, errs = validate(d)
        self.assertFalse(ok, "非 icrs frame 应被拒")

    def test_format_role_mismatch_rejected(self):
        d = base()  # phase2 → hips
        d["product_content"]["geometry"] = {
            "format": "fits", "fits": {"projection": "TAN", "wcs": "present"}}
        ok, errs = validate(d)
        self.assertFalse(ok, "geometry.format 与 role 不匹配应被拒")

    def test_min_planes_missing_rejected(self):
        d = base("phase1_product_v1")
        d["product_content"]["planes"] = [
            p for p in d["product_content"]["planes"] if p["plane_id"] != "variance"]
        ok, errs = validate(d)
        self.assertFalse(ok, "phase1 缺 variance 最小平面应被拒")
        self.assertTrue(any("min planes" in e for e in errs), errs)

    def test_duplicate_plane_rejected(self):
        d = base()
        d["product_content"]["planes"].append(copy.deepcopy(d["product_content"]["planes"][0]))
        ok, errs = validate(d)
        self.assertFalse(ok, "重复 plane_id 应被拒")

    def test_hips_geometry_requires_nested(self):
        d = base()
        d["product_content"]["geometry"]["hips"]["ordering"] = "ring"
        ok, errs = validate(d)
        self.assertFalse(ok, "HiPS ordering 非 nested 应被拒")

    def test_fits_geometry_requires_tan(self):
        d = base("phase3_planar_fits_v1")
        d["product_content"]["geometry"]["fits"]["projection"] = "CAR"
        ok, errs = validate(d)
        self.assertFalse(ok, "非 TAN 投影应被拒")

    def test_strict_json_rejects_nan(self):
        text = load(EX_P2)
        raw = json.dumps(text).replace('"size": 524288000', '"size": NaN')
        ok, errs = PhaseProductExchangeValidator().validate_text(raw)
        self.assertFalse(ok, "NaN 字面量应被拒（严格解析）")
        self.assertTrue(any("parse" in e for e in errs), errs)


if __name__ == "__main__":
    unittest.main(verbosity=2)
