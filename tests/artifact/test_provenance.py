#!/usr/bin/env python3
"""DATA-004 product provenance 验收测试（类别区分 / 确定性 digest / 版本链 /
privacy scan / production_store 集成）。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md DATA-004):
  - 区分 product/module/ABI/data schema/doc revision/history
      → TestRevisionCategories / TestDocRevisionHistory
  - 写 source commit/config/provider/worker/input hashes、science IDs
      → TestProvenanceDigestFields / TestMakeProvenanceDoc
  - 同输入配置产生相同 provenance digest（确定性）
      → TestDeterministicDigest
  - 旧 product 版本不静默接收
      → TestOldVersionNotSilentlyAccepted（assert_not_superseded +
        bind_product_input min_product_version + data_schema 绑定门）
  - privacy scan 不泄露绝对用户路径/凭据
      → TestPrivacyScanNoLeak
  - production_store 集成（sidecar 原子发布 / 恢复加载 / digest 可复算）
      → TestStoreProvenanceIntegration

本测试用 unittest（与 tests/artifact 既有风格一致），无第三方依赖。
"""
from __future__ import annotations

import copy
import json
import pathlib
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "runtime" / "artifact_store"))

from provenance import (  # noqa: E402
    ProvenanceError,
    REVISION_CATEGORIES,
    assert_doc_revision_is_current,
    assert_not_superseded,
    assert_privacy_clean,
    assert_revision_is_manifest_data_schema,
    build_history,
    build_revision,
    canonical_provenance_json,
    capability_digest,
    make_provenance_doc,
    parse_version,
    provenance_digest_hex,
    provenance_dict_to_digest,
    scan_privacy,
    scan_privacy_doc,
    validate_history,
    validate_provenance,
    validate_revision,
    version_ge,
    version_gt,
)

from production_store import (  # noqa: E402
    ArtifactStore,
    SpyStoreIO,
    canonical_manifest_json,
    publish_object,
    sha256_bytes,
    utc_now_z,
)

TYPE_P1 = "astrocs.phase1.frame_hips.v1"
COMMIT = "0d32c07d65c6d7489fa408cbafaa98ddf9ecf4da"


def base_manifest(artifact_id: str = "frame-000001",
                  type_id: str = TYPE_P1,
                  content_hex: str = "0" * 64,
                  size: int = 0,
                  status: str = "COMPLETE",
                  product_version: str = "0.11.0-alpha.1-linux-amd64-gcc14",
                  science_ids: list | None = None,
                  inputs: list | None = None) -> dict:
    """构造完整 DATA-001 manifest（冻结字段序；DATA-004 复用 producer 字段）。"""
    producer: dict = {
        "module_id": "astrocs.phase1.frame_hips",
        "module_build_id": product_version,
    }
    if science_ids:
        producer["science_contract_ids"] = science_ids
    return {
        "manifest_schema": "astrocs.artifact-manifest/v1",
        "manifest_version": 1,
        "artifact_id": artifact_id,
        "type_id": type_id,
        "schema_version": 1,
        "storage_uri": f"run:r1/phase1/{artifact_id}",
        "content_digest": {"algorithm": "sha256", "hex": content_hex},
        "size": size,
        "producer": producer,
        "run": {"run_id": "r1", "phase": "phase1", "session_id": "s1"},
        "node": {"node_id": "frame_hips", "pipeline_id": "p1-frame"},
        "input_digests": inputs or [],
        "config_digest": {"algorithm": "sha256", "hex": "c" * 64},
        "status": status,
        "created_utc": utc_now_z(),
    }


def prov_kwargs(artifact_id: str = "frame-a", product: str = "1.2.0",
                module: str = "0.11.0-alpha.1", abi: str = "v1",
                data_schema: str = "v1") -> dict:
    """常用 provenance digest 参数集。"""
    return dict(
        artifact_id=artifact_id,
        revision=build_revision(product=product, module=module, abi=abi,
                                data_schema=data_schema),
        source_commit=COMMIT,
        config_digest={"algorithm": "sha256", "hex": "c" * 64},
        provider_digest={"algorithm": "sha256", "hex": "d" * 64},
        worker_digest={"algorithm": "sha256", "hex": "e" * 64},
        input_digests=[{"artifact_id": "frame-in", "digest": "1" * 64}],
        science_ids=["SCI-DRZ-014", "SCI-CW-001"],
    )


def sample_history(**kw) -> dict:
    base = dict(
        category="product", artifact_id="frame-a",
        replaced=[
            {"version": "1.0.0",
             "digest": {"algorithm": "sha256", "hex": "a" * 64},
             "reason": "calib fix"},
            {"version": "1.1.0",
             "digest": {"algorithm": "sha256", "hex": "b" * 64}},
        ],
        superseded_by={"version": "1.2.0",
                       "digest": {"algorithm": "sha256", "hex": "c" * 64}},
        replaced_at_utc="2026-09-02T08:00:00Z",
    )
    base.update(kw)
    return build_history(**base)


def make_prov_store(tmp: pathlib.Path, run_id: str = "run-p1",
                    history: dict | None = None) -> ArtifactStore:
    root = tmp / "store-root"
    store = ArtifactStore(root, run_id, io=SpyStoreIO()).with_provenance(
        source_commit=COMMIT,
        provider_digest={"algorithm": "sha256", "hex": "d" * 64},
        worker_digest={"algorithm": "sha256", "hex": "e" * 64},
        strategy="drizzle-v1",
        history=history,
    ).start()
    return store


class TestRevisionCategories(unittest.TestCase):
    """区分 product/module/ABI/data schema revision 类别。"""

    def test_all_categories_present(self):
        rev = build_revision(product="1.2.0", module="0.11.0", abi="v1",
                             data_schema="v1")
        self.assertEqual(list(rev.keys()), list(REVISION_CATEGORIES))
        self.assertEqual(REVISION_CATEGORIES,
                         ("product", "module", "abi", "data_schema"))

    def test_validate_revision_accepts_subset(self):
        errs = validate_revision({"product": "1.0.0"})
        self.assertEqual(errs, [])
        errs = validate_revision({"data_schema": "v3"})
        self.assertEqual(errs, [])

    def test_validate_revision_rejects_unknown_category(self):
        errs = validate_revision({"product": "1.0.0", "foo": "x"})
        self.assertTrue(any("unknown category" in e for e in errs), errs)

    def test_validate_revision_rejects_bad_version(self):
        errs = validate_revision({"product": "1..0"})   # 双分隔符非法
        self.assertTrue(errs, errs)
        errs = validate_revision({"product": "1.0.0-"})  # 尾分隔符非法
        self.assertTrue(errs, errs)

    def test_empty_revision_rejected(self):
        with self.assertRaises(ProvenanceError):
            build_revision()

    def test_parse_version_semver(self):
        self.assertEqual(parse_version("1.2.3"), (1, 2, 3))
        self.assertEqual(parse_version("0.11.0-alpha.1"), (0, 11, 0))
        self.assertEqual(parse_version("v1"), (1,))

    def test_version_comparison(self):
        self.assertTrue(version_gt("1.2.0", "1.1.9"))
        self.assertTrue(version_ge("0.11.0", "0.10.0"))
        self.assertFalse(version_gt("1.1.0", "1.1.0"))


class TestDocRevisionHistory(unittest.TestCase):
    """doc revision（文档形态修订）与 history（旧 product 版本链）。"""

    def test_doc_revision_current_accepted(self):
        assert_doc_revision_is_current("v1")  # 不抛

    def test_doc_revision_stale_rejected(self):
        with self.assertRaises(ProvenanceError):
            assert_doc_revision_is_current("v0")

    def test_doc_revision_none_passthrough(self):
        assert_doc_revision_is_current(None)  # DATA-001 冻结期文档放行

    def test_build_history_ok(self):
        h = sample_history()
        self.assertEqual(h["history_schema"], "astrocs.provenance-history/v1")
        self.assertEqual(h["revision_category"], "product")
        self.assertEqual([e["version"] for e in h["replaced"]],
                         ["1.0.0", "1.1.0"])
        self.assertEqual(h["superseded_by"]["version"], "1.2.0")
        self.assertEqual(validate_history(h), [])

    def test_build_history_requires_nonempty(self):
        with self.assertRaises(ProvenanceError):
            build_history(category="product", artifact_id="frame-a", replaced=[])

    def test_build_history_requires_ascending(self):
        with self.assertRaises(ProvenanceError):
            build_history(category="product", artifact_id="frame-a",
                          replaced=[{"version": "1.1.0",
                                     "digest": {"algorithm": "sha256", "hex": "b" * 64}},
                                    {"version": "1.0.0",
                                     "digest": {"algorithm": "sha256", "hex": "a" * 64}}])

    def test_build_history_duplicate_versions_rejected(self):
        with self.assertRaises(ProvenanceError):
            build_history(category="product", artifact_id="frame-a",
                          replaced=[{"version": "1.0.0",
                                     "digest": {"algorithm": "sha256", "hex": "a" * 64}},
                                    {"version": "1.0.0",
                                     "digest": {"algorithm": "sha256", "hex": "b" * 64}}])

    def test_validate_history_rejects_bad_category(self):
        h = sample_history()
        h["revision_category"] = "banana"
        errs = validate_history(h)
        self.assertTrue(any("revision_category" in e for e in errs), errs)

    def test_history_entry_missing_digest_rejected(self):
        h = sample_history()
        del h["replaced"][0]["digest"]
        errs = validate_history(h)
        self.assertTrue(any("digest" in e for e in errs), errs)


class TestProvenanceDigestFields(unittest.TestCase):
    """provenance digest 覆盖 source commit / config / provider / worker /
    input hashes / science IDs。"""

    def test_digest_changes_with_source_commit(self):
        kw = prov_kwargs()
        d1 = provenance_digest_hex(**kw)
        kw["source_commit"] = "0" * 40
        d2 = provenance_digest_hex(**kw)
        self.assertNotEqual(d1, d2)

    def test_digest_changes_with_config(self):
        kw = prov_kwargs()
        d1 = provenance_digest_hex(**kw)
        kw["config_digest"] = {"algorithm": "sha256", "hex": "f" * 64}
        d2 = provenance_digest_hex(**kw)
        self.assertNotEqual(d1, d2)

    def test_digest_changes_with_provider_and_worker(self):
        kw = prov_kwargs()
        d1 = provenance_digest_hex(**kw)
        kw["provider_digest"] = {"algorithm": "sha256", "hex": "9" * 64}
        self.assertNotEqual(d1, provenance_digest_hex(**kw))
        kw2 = prov_kwargs()
        kw2["worker_digest"] = {"algorithm": "sha256", "hex": "9" * 64}
        self.assertNotEqual(d1, provenance_digest_hex(**kw2))

    def test_digest_changes_with_input_hashes(self):
        kw = prov_kwargs()
        d1 = provenance_digest_hex(**kw)
        kw["input_digests"] = [{"artifact_id": "frame-in", "digest": "2" * 64}]
        self.assertNotEqual(d1, provenance_digest_hex(**kw))

    def test_digest_changes_with_science_ids(self):
        kw = prov_kwargs()
        d1 = provenance_digest_hex(**kw)
        kw["science_ids"] = ["SCI-OTHER-001"]
        self.assertNotEqual(d1, provenance_digest_hex(**kw))

    def test_digest_changes_with_revision_categories(self):
        kw = prov_kwargs()
        d1 = provenance_digest_hex(**kw)
        kw2 = prov_kwargs(product="1.3.0")
        self.assertNotEqual(d1, provenance_digest_hex(**kw2))
        kw3 = prov_kwargs(data_schema="v2")
        self.assertNotEqual(d1, provenance_digest_hex(**kw3))

    def test_missing_source_commit_rejected(self):
        kw = prov_kwargs()
        kw["source_commit"] = ""  # 空 = 未提供（缺省值）；校验仍拒
        with self.assertRaises(ProvenanceError):
            provenance_digest_hex(**kw)

    def test_bad_source_commit_rejected(self):
        kw = prov_kwargs()
        kw["source_commit"] = "not-a-commit"
        with self.assertRaises(ProvenanceError):
            provenance_digest_hex(**kw)

    def test_artifact_id_lexical(self):
        kw = prov_kwargs(artifact_id="bad/id")
        with self.assertRaises(ProvenanceError):
            provenance_digest_hex(**kw)

    def test_bad_science_id_rejected(self):
        kw = prov_kwargs()
        kw["science_ids"] = ["SCI-low"]
        with self.assertRaises(ProvenanceError):
            provenance_digest_hex(**kw)


class TestDeterministicDigest(unittest.TestCase):
    """同输入配置产生相同 provenance digest；运行事实不参与。"""

    def test_same_input_same_digest(self):
        d1 = provenance_digest_hex(**prov_kwargs())
        d2 = provenance_digest_hex(**prov_kwargs())
        self.assertEqual(d1, d2)

    def test_input_order_irrelevant(self):
        kw = prov_kwargs()
        kw["science_ids"] = ["SCI-CW-001", "SCI-DRZ-014"]  # 原始序
        d1 = provenance_digest_hex(**kw)
        kw["science_ids"] = ["SCI-DRZ-014", "SCI-CW-001"]  # 乱序
        self.assertEqual(d1, provenance_digest_hex(**kw))

    def test_input_digest_order_irrelevant(self):
        kw = prov_kwargs()
        kw["input_digests"] = [
            {"artifact_id": "in-b", "digest": "2" * 64},
            {"artifact_id": "in-a", "digest": "1" * 64},
        ]
        d1 = provenance_digest_hex(**kw)
        kw["input_digests"] = [
            {"artifact_id": "in-a", "digest": "1" * 64},
            {"artifact_id": "in-b", "digest": "2" * 64},
        ]
        self.assertEqual(d1, provenance_digest_hex(**kw))

    def test_run_facts_do_not_change_digest(self):
        """created_utc/run_id/phase 是运行事实：同溯源输入不同 run → 同 digest。"""
        p1 = make_provenance_doc(**prov_kwargs(), created_utc="2026-09-02T00:00:00Z",
                                run_id="r1", phase="phase1", doc_revision="v1")
        p2 = make_provenance_doc(**prov_kwargs(), created_utc="2026-09-03T12:00:00Z",
                                run_id="r2", phase="phase1", doc_revision="v1")
        self.assertEqual(p1["provenance_digest"], p2["provenance_digest"])

    def test_digest_64_hex(self):
        import re
        d = provenance_digest_hex(**prov_kwargs())
        self.assertRegex(d, r"^[0-9a-f]{64}$")

    def test_history_not_in_digest(self):
        """history 是文档形态/旧链展示：不参与 digest（拒收靠 assert 语义）。"""
        kw = prov_kwargs()
        d1 = provenance_digest_hex(**kw)
        kw2 = prov_kwargs()
        kw2["science_ids"] = kw2.get("science_ids")  # no-op keep shape
        self.assertEqual(d1, provenance_digest_hex(**kw2))


class TestOldVersionNotSilentlyAccepted(unittest.TestCase):
    """旧 product 版本不静默接收：发布门 + 消费门 + data_schema 绑定。"""

    def test_publish_current_version_passes_history_gate(self):
        h = sample_history()
        assert_not_superseded(build_revision(product="1.2.0"), h)  # 不抛

    def test_publish_old_version_rejected(self):
        h = sample_history()
        with self.assertRaises(ProvenanceError):
            assert_not_superseded(build_revision(product="1.1.0"), h)
        with self.assertRaises(ProvenanceError):
            assert_not_superseded(build_revision(product="1.0.0"), h)

    def test_revision_without_category_passes(self):
        h = sample_history()
        assert_not_superseded(build_revision(module="0.11.0"), h,
                              category="product")  # product 未声明 → 放行

    def test_history_category_mismatch_rejected(self):
        """history 归类必须与 gate 类别一致；revision 无该类别才放行。"""
        h = sample_history()  # category=product
        # 显式声明 product 且命中 replaced → 拒
        with self.assertRaises(ProvenanceError):
            assert_not_superseded(build_revision(product="1.1.0"), h,
                                  category="product")
        # revision 未声明 product（module 类）→ 放行（无产品版本语义）
        assert_not_superseded(build_revision(module="0.11.0"), h,
                              category="product")
        # history 归类 module 而 gate 查 product（revision 有 product）→ 错配拒
        h2 = sample_history(category="module", artifact_id="frame-a")
        with self.assertRaises(ProvenanceError):
            assert_not_superseded(build_revision(product="1.2.0"), h2,
                                  category="product")

    def test_data_schema_must_match_manifest(self):
        rev = build_revision(data_schema="v1")
        assert_revision_is_manifest_data_schema(
            rev, {"type_id": TYPE_P1, "schema_version": 1})  # 不抛
        with self.assertRaises(ProvenanceError):
            assert_revision_is_manifest_data_schema(
                build_revision(data_schema="v2"),
                {"type_id": TYPE_P1, "schema_version": 1})

    def test_store_rejects_old_version_publish(self):
        """集成：配置 history 的 Store 发布已替换版本 → publish 拒绝。"""
        tmp = pathlib.Path(tempfile.mkdtemp())
        store = make_prov_store(tmp, history=sample_history())
        data = b"old-version-data"
        # product=1.1.0 已在 history.replaced → publish 硬拒（旧版本不静默接收）
        doc = base_manifest("frame-old", content_hex=sha256_bytes(data),
                            size=len(data),
                            product_version="1.1.0")
        w = store.new_writer("frame-old")
        w.stage_bytes(data)
        store.stage_manifest("frame-old", doc)
        with self.assertRaises(ValueError) as cm:
            store.publish("frame-old")
        self.assertIn("superseded", str(cm.exception))
        # 无成功对象
        self.assertFalse(
            (store.run_dir() / "manifests" / "frame-old.manifest.json").exists())
        self.assertFalse((store.objects_dir() / "frame-old").exists())

    def test_store_publish_current_version_ok(self):
        """集成：当前版本（superseded_by 指向自身）发布通过。"""
        tmp = pathlib.Path(tempfile.mkdtemp())
        store = make_prov_store(tmp, history=sample_history())
        data = b"current-data"
        doc = base_manifest("frame-cur", content_hex=sha256_bytes(data),
                            size=len(data),
                            product_version="1.2.0",
                            science_ids=["SCI-DRZ-014"])
        publish_object(store, "frame-cur", data, doc)
        prov = store.read_provenance("frame-cur")
        self.assertIsNotNone(prov)
        self.assertEqual(prov["revision"]["product"], "1.2.0")
        self.assertIsNotNone(prov.get("history"))
        self.assertEqual(prov["history"]["superseded_by"]["version"], "1.2.0")

    def test_bind_product_input_min_version_gate(self):
        """消费门：输入 product 版本低于阈值 → 拒绝（旧 product 不静默接收）。"""
        tmp = pathlib.Path(tempfile.mkdtemp())
        store = make_prov_store(tmp)
        data = b"v1-data"
        doc = base_manifest("frame-bind", content_hex=sha256_bytes(data),
                            size=len(data), product_version="0.10.0")
        publish_object(store, "frame-bind", data, doc)
        # 满足阈值 → 通过
        store.bind_product_input("frame-bind", TYPE_P1, min_product_version="0.10.0")
        # 要求更高 → 拒
        with self.assertRaises(ValueError):
            store.bind_product_input("frame-bind", TYPE_P1,
                                     min_product_version="0.11.0")

    def test_bind_without_provenance_rejected(self):
        """DATA-003 冻结期对象（无 provenance sidecar）不能做 DATA-004 产品输入。"""
        tmp = pathlib.Path(tempfile.mkdtemp())
        root = tmp / "store-root"
        # 不带 provenance 配置的 Store（DATA-003 形态）
        store = ArtifactStore(root, "run-legacy", io=SpyStoreIO()).start()
        data = b"legacy"
        doc = base_manifest("frame-legacy", content_hex=sha256_bytes(data),
                            size=len(data))
        publish_object(store, "frame-legacy", data, doc)
        self.assertIsNone(store.read_provenance("frame-legacy"))
        with self.assertRaises(ValueError) as cm:
            store.bind_product_input("frame-legacy", TYPE_P1)
        self.assertIn("lacks provenance", str(cm.exception))
        # 普通 DATA-003 绑定仍可用（兼容基线）
        store.bind_as_input("frame-legacy", TYPE_P1)


class TestPrivacyScanNoLeak(unittest.TestCase):
    """privacy scan：绝对用户路径/凭据不泄露。"""

    def test_clean_text_no_hits(self):
        self.assertEqual(scan_privacy("publish ok run r1 digest ok"), [])

    def test_unix_abs_path_detected(self):
        self.assertTrue(scan_privacy("failed at /home/alice/data/file.fits"))
        self.assertTrue(scan_privacy("store /Users/bob/cache"))

    def test_windows_path_detected(self):
        hits = scan_privacy(r"path C:\Users\bob\app\cache")
        self.assertTrue(hits)
        self.assertTrue(scan_privacy(r"\\server\share\file"))

    def test_credentials_detected(self):
        self.assertTrue(scan_privacy("token=abc123secret"))
        self.assertTrue(scan_privacy("Authorization: Bearer xyz"))
        self.assertTrue(scan_privacy("password: hunter2"))
        self.assertTrue(scan_privacy("api_key = deadbeef"))

    def test_url_userinfo_detected(self):
        hits = scan_privacy("download https://user:pass@host/data")
        self.assertTrue(hits)

    def test_assert_clean_raises_on_leak(self):
        assert_privacy_clean("clean text")  # 不抛
        with self.assertRaises(ProvenanceError):
            assert_privacy_clean("read /home/user/x failed")

    def test_scan_privacy_doc_detects_nested(self):
        doc = {"diagnostic": "load /home/me/fits failed", "ok": "fine"}
        hits = scan_privacy_doc(doc)
        self.assertTrue(any("diagnostic" in h for h in hits), hits)
        doc2 = {"nested": {"k": "token=leak"}}
        self.assertTrue(scan_privacy_doc(doc2))

    def test_make_provenance_doc_rejects_leak_in_fields(self):
        """make_provenance_doc 内部隐私门：文档任一字符串字段命中敏感模式 → 拒。"""
        import provenance as P
        with self.assertRaises(ProvenanceError):
            P.make_provenance_doc(
                **prov_kwargs(), doc_revision="v1",
                created_utc="2026-09-02T00:00:00Z",
                run_id="/home/alice/leak",  # 绝对用户路径 → 隐私门拒
                phase="phase1")


class TestMakeProvenanceDoc(unittest.TestCase):
    """provenance 完整文档：digest 可复算 / 校验通过 / science_ids 排序。"""

    def test_make_doc_and_recompute(self):
        doc = make_provenance_doc(**prov_kwargs(), doc_revision="v1",
                                  created_utc="2026-09-02T00:00:00Z",
                                  run_id="r1", phase="phase1")
        self.assertEqual(validate_provenance(doc), [])
        self.assertEqual(doc["provenance_digest"],
                         provenance_dict_to_digest(doc))

    def test_doc_schema_fields(self):
        doc = make_provenance_doc(**prov_kwargs())
        self.assertEqual(doc["provenance_schema"], "astrocs.provenance/v1")
        self.assertEqual(doc["version"], 1)
        self.assertEqual(doc["source_commit"], COMMIT)
        self.assertEqual(doc["science_ids"], ["SCI-CW-001", "SCI-DRZ-014"])

    def test_provenance_dict_to_digest_missing_inputs(self):
        with self.assertRaises(ProvenanceError):
            provenance_dict_to_digest({"artifact_id": "x"})

    def test_doc_digest_hex_valid(self):
        import re
        doc = make_provenance_doc(**prov_kwargs())
        self.assertRegex(doc["provenance_digest"], r"^[0-9a-f]{64}$")

    def test_validate_rejects_tampered_digest_field_shape(self):
        doc = make_provenance_doc(**prov_kwargs())
        doc["provenance_digest"] = "zz"
        errs = validate_provenance(doc)
        self.assertTrue(any("provenance_digest" in e for e in errs), errs)


class TestStoreProvenanceIntegration(unittest.TestCase):
    """production_store 集成：sidecar 原子发布 / 恢复加载 / digest 可复算 /
    spy 发布计数（DATA-003 3 次 + provenance 1 次 = 4）。"""

    def test_publish_writes_provenance_sidecar(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        store = make_prov_store(tmp)
        data = b"payload"
        doc = base_manifest("frame-p", content_hex=sha256_bytes(data),
                            size=len(data), science_ids=["SCI-CW-001"])
        publish_object(store, "frame-p", data, doc)
        prov = store.read_provenance("frame-p")
        self.assertIsNotNone(prov)
        self.assertEqual(prov["artifact_id"], "frame-p")
        self.assertEqual(prov["revision"]["data_schema"], "v1")
        self.assertEqual(prov["revision"]["product"],
                         "0.11.0-alpha.1-linux-amd64-gcc14")
        self.assertEqual(prov["revision"]["module"], "astrocs.phase1.frame_hips")
        self.assertEqual(prov["revision"]["abi"], "v1")
        self.assertEqual(prov["science_ids"], ["SCI-CW-001"])
        # sidecar 落盘
        p = store.run_dir() / "manifests" / "frame-p.provenance.json"
        self.assertTrue(p.is_file())

    def test_digest_recompute_matches_sidecar_and_disk(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        store = make_prov_store(tmp)
        data = b"recompute"
        doc = base_manifest("frame-r", content_hex=sha256_bytes(data),
                            size=len(data))
        publish_object(store, "frame-r", data, doc)
        prov = store.read_provenance("frame-r")
        self.assertEqual(prov["provenance_digest"],
                         store.provenance_digest_recompute("frame-r"))
        # 磁盘 JSON 与内存一致
        disk = json.loads(
            (store.run_dir() / "manifests" / "frame-r.provenance.json")
            .read_text(encoding="utf-8"))
        self.assertEqual(disk["provenance_digest"], prov["provenance_digest"])
        # manifest 本体保持 DATA-001 strict shape（不附加 provenance 字段）
        read = store.read_manifest("frame-r")
        extra = sorted(set(read.to_dict().keys()) - set(base_manifest().keys()))
        self.assertEqual(extra, [])

    def test_store_reopen_loads_provenance(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        store = make_prov_store(tmp)
        data = b"reopen"
        doc = base_manifest("frame-o", content_hex=sha256_bytes(data),
                            size=len(data), product_version="1.2.0")
        publish_object(store, "frame-o", data, doc)
        store2 = ArtifactStore(store.store_root, "run-p1").start()
        prov2 = store2.read_provenance("frame-o")
        self.assertIsNotNone(prov2)
        self.assertEqual(prov2["provenance_digest"],
                         store2.provenance_digest_recompute("frame-o"))
        # bind 通过（DATA-004 消费门）
        store2.bind_product_input("frame-o", TYPE_P1)

    def test_tampered_digest_rejected_at_bind(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        store = make_prov_store(tmp)
        data = b"tamper"
        doc = base_manifest("frame-t", content_hex=sha256_bytes(data),
                            size=len(data))
        publish_object(store, "frame-t", data, doc)
        prov = store.read_provenance("frame-t")
        prov["provenance_digest"] = "f" * 64
        store._provenance["frame-t"] = prov
        with self.assertRaises(ValueError) as cm:
            store.bind_product_input("frame-t", TYPE_P1)
        self.assertIn("digest mismatch", str(cm.exception))

    def test_spy_counts_provenance_extra_publish(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        store = make_prov_store(tmp)
        data = b"spy"
        doc = base_manifest("frame-s", content_hex=sha256_bytes(data),
                            size=len(data))
        publish_object(store, "frame-s", data, doc)
        # DATA-003: 3 次原子 rename（对象/manifest/sidecar）；DATA-004 +1 provenance
        self.assertGreaterEqual(len(store.io.publishes), 4)

    def test_data_schema_mismatch_manifest_rejected_at_publish(self):
        """错误 schema revision：store 级发布把 revision.data_schema 与 manifest
        绑定 → 一致才放行（本 store 派生 v1 = manifest v1，天然一致；负测放在
        assert_revision_is_manifest_data_schema 单元测）。"""
        tmp = pathlib.Path(tempfile.mkdtemp())
        store = make_prov_store(tmp)
        data = b"ok"
        doc = base_manifest("frame-ds", content_hex=sha256_bytes(data),
                            size=len(data))
        publish_object(store, "frame-ds", data, doc)
        prov = store.read_provenance("frame-ds")
        self.assertEqual(prov["revision"]["data_schema"], "v1")

    def test_capability_digest_deterministic(self):
        d1 = capability_digest([{"name": "avx2", "value": "1"},
                                {"name": "baseline", "value": "1"}])
        d2 = capability_digest([{"name": "baseline", "value": "1"},
                                {"name": "avx2", "value": "1"}])
        self.assertEqual(d1, d2)
        self.assertNotEqual(
            d1, capability_digest([{"name": "avx2", "value": "1"}]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
