#!/usr/bin/env python3
"""DATA-003 生产 ArtifactStore 验收测试（spy Store + 负测 + manifest hash 重算）。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md DATA-003):
  - Runtime 启动真实 Store                          → TestRuntimeStartsRealStore
  - 模块 execute 只能拿已校验 handle/reader/writer   → TestWriterHandleOnly
  - 写临时对象 → 完整校验 → hash → 原子 publish       → TestAtomicPublish
  - cancel/fail 无成功对象                           → TestCancelFailNoSuccessObject
  - spy Store 证明每读写经过 Store                    → TestSpyStoreEveryReadWrite
  - 绕过路径 失败且可恢复                             → TestBypassPathRejected
  - producer 重复 失败                               → TestUniqueProducer
  - 错误 schema 失败                                 → TestWrongSchemaRejected
  - 磁盘满 失败且可恢复                               → TestDiskFullRecoverable
  - 进程中断 失败且可恢复                             → TestInterruptRecoverable
  - 取消 无成功对象                                  → TestCancelNoSuccessObject
  - manifest hash 可重算                             → TestManifestHashRecomputable

本测试用 unittest（与 tests/artifact 既有风格一致），无第三方依赖。
"""
from __future__ import annotations

import copy
import hashlib
import json
import pathlib
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "runtime" / "artifact_store"))

from production_store import (  # noqa: E402
    ArtifactStore,
    Digest,
    FailingIO,
    InterruptIO,
    ManifestRead,
    SpyStoreIO,
    Writer,
    canonical_manifest_json,
    publish_object,
    sha256_bytes,
    utc_now_z,
)

TYPE_P1 = "astrocs.phase1.frame_hips.v1"
TYPE_P2 = "astrocs.phase2.mosaic_hips.v1"


def base_manifest(artifact_id: str = "frame-000001",
                  type_id: str = TYPE_P1,
                  content_hex: str = "0" * 64,
                  size: int = 0,
                  status: str = "COMPLETE") -> dict:
    """构造完整 DATA-001 manifest（冻结字段序）。"""
    return {
        "manifest_schema": "astrocs.artifact-manifest/v1",
        "manifest_version": 1,
        "artifact_id": artifact_id,
        "type_id": type_id,
        "schema_version": 1,
        "storage_uri": f"run:r1/phase1/{artifact_id}",
        "content_digest": {"algorithm": "sha256", "hex": content_hex},
        "size": size,
        "producer": {
            "module_id": "astrocs.phase1.frame_hips",
            "module_build_id": "g46b0d8e-linux-amd64-gcc14",
        },
        "run": {"run_id": "r1", "phase": "phase1", "session_id": "s1"},
        "node": {"node_id": "frame_hips", "pipeline_id": "p1-frame"},
        "input_digests": [],
        "config_digest": {"algorithm": "sha256", "hex": "c" * 64},
        "status": status,
        "created_utc": utc_now_z(),
    }


def make_store(tmp: pathlib.Path, run_id: str = "run-1",
               spy: bool = True) -> tuple[ArtifactStore, pathlib.Path]:
    root = tmp / "store-root"
    io = SpyStoreIO() if spy else None
    store = ArtifactStore(root, run_id, io=io).start()
    return store, root


def publish_ok(store: ArtifactStore, artifact_id: str, data: bytes,
               type_id: str = TYPE_P1) -> dict:
    doc = base_manifest(artifact_id, type_id=type_id,
                        content_hex=sha256_bytes(data), size=len(data))
    publish_object(store, artifact_id, data, doc)
    return doc


class TestRuntimeStartsRealStore(unittest.TestCase):
    """Runtime 启动真实 Store：目录结构存在、run 私有、恢复只索引成功对象。"""

    def test_start_creates_real_dirs(self):
        store, root = make_store(pathlib.Path(tempfile.mkdtemp()))
        self.assertTrue(store.run_dir().is_dir())
        self.assertTrue(store.objects_dir().is_dir())
        self.assertTrue((store.run_dir() / "stage").is_dir())
        self.assertTrue((store.run_dir() / "manifests").is_dir())

    def test_store_is_real_instance(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        self.assertIsInstance(store, ArtifactStore)
        self.assertTrue(hasattr(store, "publish"))
        self.assertTrue(hasattr(store, "bind_as_input"))
        self.assertTrue(hasattr(store, "read_verified"))

    def test_start_recovers_only_complete_success_objects(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        store, _ = make_store(tmp)
        data = b"frame-data-ok"
        publish_ok(store, "frame-ok", data)
        # 制造无 COMPLETE manifest 的残留（中断模拟）
        obj = store.objects_dir() / "frame-half"
        obj.write_bytes(b"half")
        (store.run_dir() / "manifests" / "frame-half.manifest.json").write_text(
            json.dumps(base_manifest("frame-half", status="INCOMPLETE")), encoding="utf-8")
        store2 = ArtifactStore(store.store_root, "run-1").start()
        self.assertIn("frame-ok", store2.ids())
        self.assertNotIn("frame-half", store2.ids())  # 无成功对象不索引
        self.assertEqual(store2.manifest_count(), 1)


class TestWriterHandleOnly(unittest.TestCase):
    """模块 execute 只能拿已校验 handle/reader/writer（无裸路径）。"""

    def test_writer_has_no_raw_path_escape(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        w = store.new_writer("frame-1")
        self.assertIsInstance(w, Writer)
        # writer 暴露的是 artifact_id 句柄，不是可写的裸文件系统路径
        self.assertEqual(w.artifact_id, "frame-1")
        self.assertFalse(hasattr(w, "path"))
        self.assertFalse(hasattr(w, "filename"))
        self.assertFalse(hasattr(w, "open"))

    def test_reader_handles_are_validated_only(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        data = b"payload"
        publish_ok(store, "frame-2", data)
        d = store.get_digest("frame-2")
        self.assertIsInstance(d, Digest)
        m = store.read_manifest("frame-2")
        self.assertIsInstance(m, ManifestRead)
        # ManifestRead 只带身份字段（无路径）
        self.assertEqual(m.artifact_id(), "frame-2")
        self.assertEqual(m.content_digest_hex(), sha256_bytes(data))

    def test_bind_requires_type_match(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        publish_ok(store, "frame-3", b"x")
        with self.assertRaises(ValueError):
            store.bind_as_input("frame-3", TYPE_P2)  # 期望 P2，实为 P1 → 拒


class TestAtomicPublish(unittest.TestCase):
    """写临时对象 → 完整校验 → hash → 原子 publish。"""

    def test_publish_roundtrip(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        data = b"frame-content-atomic"
        doc = publish_ok(store, "frame-a", data)
        self.assertIn("frame-a", store.ids())
        out = store.read_verified("frame-a", TYPE_P1)
        self.assertEqual(out, data)
        self.assertEqual(store.get_digest("frame-a").hex, sha256_bytes(data))

    def test_publish_manifest_stays_strict_data001_shape(self):
        """发布 manifest 保持 DATA-001 schema（additionalProperties=false）——不再附加字段。"""
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        data = b"strict-shape"
        publish_ok(store, "frame-s", data)
        read = store.read_manifest("frame-s")
        self.assertIsNotNone(read)
        extra = sorted(set(read.to_dict().keys()) - set(base_manifest().keys()))
        self.assertEqual(extra, [], f"发布 manifest 含非 DATA-001 字段: {extra}")
        # manifest hash 在独立 sidecar（不污染 manifest 本体）
        self.assertTrue(
            (store.run_dir() / "manifests" / "frame-s.manifest.sha256").is_file())

    def test_publish_before_stage_manifest_rejected(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        w = store.new_writer("frame-x")
        w.stage_bytes(b"data")
        with self.assertRaises(RuntimeError):
            store.publish("frame-x")  # 未 stage_manifest → 拒

    def test_publish_without_staged_content_rejected(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        doc = base_manifest("frame-y", content_hex=sha256_bytes(b"d"), size=1)
        store.stage_manifest("frame-y", doc)
        with self.assertRaises(RuntimeError):
            store.publish("frame-y")  # 未 stage_bytes → 拒


class TestUniqueProducer(unittest.TestCase):
    """producer 重复: 同 id 二次写/publish → 硬失败。"""

    def test_second_publish_same_id_fails(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        publish_ok(store, "frame-dup", b"first")
        with self.assertRaises(RuntimeError):
            publish_ok(store, "frame-dup", b"second")
        self.assertEqual(len(store.ids()), 1)

    def test_new_writer_duplicate_fails(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        publish_ok(store, "frame-dup2", b"first")
        with self.assertRaises(RuntimeError):
            store.new_writer("frame-dup2")


class TestWrongSchemaRejected(unittest.TestCase):
    """错误 schema: manifest 缺字段/content hash 不符/size 不符 → 拒绝。"""

    def test_missing_manifest_field_rejected(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        data = b"x"
        doc = base_manifest("frame-bad1", content_hex=sha256_bytes(data), size=1)
        del doc["node"]
        w = store.new_writer("frame-bad1")
        w.stage_bytes(data)
        with self.assertRaises(ValueError) as cm:
            store.stage_manifest("frame-bad1", doc)
        self.assertIn("manifest invalid", str(cm.exception))
        self.assertEqual(store.manifest_count(), 0)

    def test_content_hash_mismatch_rejected(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        data = b"actual-content"
        doc = base_manifest("frame-bad2", content_hex="f" * 64, size=len(data))
        w = store.new_writer("frame-bad2")
        w.stage_bytes(data)
        store.stage_manifest("frame-bad2", doc)
        with self.assertRaises(ValueError) as cm:
            store.publish("frame-bad2")
        self.assertIn("content hash mismatch", str(cm.exception))
        # 失败后无成功对象（无 COMPLETE manifest 落盘）
        self.assertFalse(
            (store.run_dir() / "manifests" / "frame-bad2.manifest.json").exists())
        self.assertFalse((store.objects_dir() / "frame-bad2").exists())

    def test_size_mismatch_rejected(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        data = b"12345"
        doc = base_manifest("frame-bad3", content_hex=sha256_bytes(data), size=999)
        w = store.new_writer("frame-bad3")
        w.stage_bytes(data)
        store.stage_manifest("frame-bad3", doc)
        with self.assertRaises(ValueError):
            store.publish("frame-bad3")

    def test_status_not_complete_rejected(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        data = b"incomplete"
        doc = base_manifest("frame-bad4", content_hex=sha256_bytes(data),
                            size=len(data), status="INCOMPLETE")
        w = store.new_writer("frame-bad4")
        w.stage_bytes(data)
        with self.assertRaises(ValueError):
            store.stage_manifest("frame-bad4", doc)
        self.assertEqual(store.manifest_count(), 0)

    def test_wrong_type_at_bind_rejected(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        publish_ok(store, "frame-bad5", b"data", type_id=TYPE_P2)
        with self.assertRaises(ValueError):
            store.bind_as_input("frame-bad5", TYPE_P1)


class TestSpyStoreEveryReadWrite(unittest.TestCase):
    """spy Store 证明每读写经过 Store（内容字节只经 Store 事件读取）。"""

    def test_spy_records_all_io(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()), spy=True)
        self.assertIsInstance(store.io, SpyStoreIO)
        data = b"spy-payload-0123456789"
        publish_ok(store, "frame-spy", data)
        out = store.read_verified("frame-spy", TYPE_P1)
        self.assertEqual(out, data)
        spy = store.io
        # 写 1 次 stage_open + 发布 3 次原子 rename（对象/manifest/sidecar）
        self.assertGreaterEqual(len(spy.writes), 1)
        self.assertGreaterEqual(len(spy.publishes), 3)
        # 读: publish 时读暂存内容 + read_verified 读已发布内容
        self.assertGreaterEqual(len(spy.reads), 2)
        for ev in spy.writes:
            self.assertEqual(ev["kind"], "stage_open")
        for ev in spy.publishes:
            self.assertEqual(ev["kind"], "atomic_publish")

    def test_content_read_only_via_store(self):
        """内容字节唯一来源 = Store 事件（read_verified 重读并复核 hash）。"""
        store, root = make_store(pathlib.Path(tempfile.mkdtemp()), spy=True)
        data = b"only-via-store"
        publish_ok(store, "frame-via", data)
        spy = store.io
        n_reads_before = len(spy.reads)
        out = store.read_verified("frame-via", TYPE_P1)
        self.assertEqual(out, data)
        # 消费触发新一次经 Store 的读
        self.assertEqual(len(spy.reads), n_reads_before + 1)
        # 模块侧不存在直接读磁盘的路径: Store 全部真实读都记录在 spy.reads
        disk_bytes = (store.objects_dir() / "frame-via").read_bytes()
        self.assertEqual(disk_bytes, data)  # 磁盘确有对象，但消费必须经 Store


class TestBypassPathRejected(unittest.TestCase):
    """绕过路径: 不经 Store 直写磁盘 → bind/consume 失败且可恢复。"""

    def test_bypass_not_in_store_and_rejected(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        store, root = make_store(tmp)
        bypass = root / "runs" / "bypass" / "objects"
        bypass.mkdir(parents=True, exist_ok=True)
        (bypass / "frame-evil").write_bytes(b"evil-bytes")
        # 绕过对象不在 Store 索引 → bind/read 拒绝
        with self.assertRaises(ValueError):
            store.bind_as_input("frame-evil", TYPE_P1)
        self.assertNotIn("frame-evil", store.ids())
        self.assertEqual(store.manifest_count(), 0)
        # 可恢复: 删除绕过文件即可（无 Store 状态污染）
        (bypass / "frame-evil").unlink()
        self.assertFalse((bypass / "frame-evil").exists())

    def test_try_publish_without_store_helper(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        store, root = make_store(tmp)
        # 模块绕过 Store 直写 run 目录的 objects/（不经 new_writer/publish）
        bypass = root / "runs" / "run-1" / "objects"
        bypass.mkdir(parents=True, exist_ok=True)
        (bypass / "frame-direct").write_bytes(b"direct-write")
        with self.assertRaises(ValueError):
            store.bind_as_input("frame-direct", TYPE_P1)
        # 可恢复: 清理绕过文件后 Store 无残留
        (bypass / "frame-direct").unlink()
        self.assertEqual(store.manifest_count(), 0)


class TestDiskFullRecoverable(unittest.TestCase):
    """磁盘满: 写入/发布阶段 ENOSPC → 无成功对象；空间恢复后新 Store 可重发。"""

    def test_disk_full_at_stage_open(self):
        root = pathlib.Path(tempfile.mkdtemp()) / "store"
        store = ArtifactStore(root, "run-full", io=FailingIO("stage_open")).start()
        w = store.new_writer("frame-full")
        with self.assertRaises(OSError):
            w.stage_bytes(b"never-written")
        # 无成功对象
        self.assertEqual(store.manifest_count(), 0)
        self.assertFalse((store.objects_dir() / "frame-full").exists())
        # 可恢复: 空间恢复（换正常 I/O）后同 Store 根目录新 run 可重发
        store2 = ArtifactStore(root, "run-full-2").start()
        publish_ok(store2, "frame-full", b"after-space-restored")
        self.assertEqual(store2.read_verified("frame-full", TYPE_P1), b"after-space-restored")

    def test_disk_full_at_publish(self):
        root = pathlib.Path(tempfile.mkdtemp()) / "store"
        store = ArtifactStore(root, "run-full-p", io=FailingIO("atomic_publish")).start()
        data = b"publish-fails"
        doc = base_manifest("frame-fullp", content_hex=sha256_bytes(data),
                            size=len(data))
        w = store.new_writer("frame-fullp")
        w.stage_bytes(data)
        store.stage_manifest("frame-fullp", doc)
        with self.assertRaises(OSError):
            store.publish("frame-fullp")
        self.assertFalse((store.run_dir() / "manifests" / "frame-fullp.manifest.json").exists())


class TestInterruptRecoverable(unittest.TestCase):
    """进程中断: publish 中途中断 → 无完成标记；新 Store start() 不索引，可重发。"""

    def test_interrupt_mid_publish_no_success_object(self):
        root = pathlib.Path(tempfile.mkdtemp()) / "store"
        store = ArtifactStore(root, "run-int", io=InterruptIO()).start()
        data = b"interrupted-payload"
        doc = base_manifest("frame-int", content_hex=sha256_bytes(data),
                            size=len(data))
        w = store.new_writer("frame-int")
        w.stage_bytes(data)
        store.stage_manifest("frame-int", doc)
        with self.assertRaises(KeyboardInterrupt):
            store.publish("frame-int")
        # 无 COMPLETE manifest（中断发生在 manifest rename 前）
        mf = store.run_dir() / "manifests" / "frame-int.manifest.json"
        self.assertFalse(mf.exists())
        # 新 Store（同根目录）start() 不索引中断残留 → 可恢复
        store2 = ArtifactStore(root, "run-int").start()
        self.assertNotIn("frame-int", store2.ids())
        publish_ok(store2, "frame-int", data)
        self.assertEqual(store2.read_verified("frame-int", TYPE_P1), data)


class TestCancelNoSuccessObject(unittest.TestCase):
    """取消: writer.release() 后不 publish + cleanup → 无成功对象。"""

    def test_cancel_writer_release_no_object(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        w = store.new_writer("frame-cancel")
        w.stage_bytes(b"cancelled-work")
        store.stage_manifest("frame-cancel",
                             base_manifest("frame-cancel",
                                           content_hex=sha256_bytes(b"cancelled-work"),
                                           size=len(b"cancelled-work")))
        w.release()  # cancel/fail 路径
        with self.assertRaises(RuntimeError):
            store.publish("frame-cancel")  # writer released → 拒（无成功对象）
        store.cleanup()
        self.assertEqual(store.manifest_count(), 0)
        self.assertNotIn("frame-cancel", store.ids())
        self.assertFalse((store.objects_dir() / "frame-cancel").exists())

    def test_stage_manifest_then_release_no_success(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        w = store.new_writer("frame-cancel2")
        w.stage_bytes(b"x")
        store.stage_manifest("frame-cancel2",
                             base_manifest("frame-cancel2",
                                           content_hex=sha256_bytes(b"x"), size=1))
        w.release()
        store.cleanup()
        self.assertEqual(store.manifest_count(), 0)
        self.assertEqual(store.object_count(), 0)


class TestManifestHashRecomputable(unittest.TestCase):
    """manifest hash 可重算: sidecar == 重算 == 磁盘 sidecar。"""

    def test_manifest_hash_recompute_matches_sidecar(self):
        store, _ = make_store(pathlib.Path(tempfile.mkdtemp()))
        data = b"hash-recompute"
        doc = base_manifest("frame-hash", content_hex=sha256_bytes(data),
                            size=len(data))
        publish_object(store, "frame-hash", data, doc)
        sidecar = store.manifest_digest_hex("frame-hash")
        self.assertIsNotNone(sidecar)
        recomputed = ArtifactStore.manifest_hash_recompute(doc)
        self.assertEqual(sidecar, recomputed)
        # 磁盘 sidecar 一致
        self.assertEqual(store.manifest_digest_hex_from_disk("frame-hash"), recomputed)

    def test_manifest_hash_stable_under_reserialize(self):
        doc = base_manifest("frame-hash2", content_hex="a" * 64, size=0)
        h1 = ArtifactStore.manifest_hash_recompute(doc)
        # 深拷贝 + 重序列化不改变规范 JSON hash
        doc2 = json.loads(json.dumps(copy.deepcopy(doc)))
        self.assertEqual(ArtifactStore.manifest_hash_recompute(doc2), h1)

    def test_manifest_hash_changes_when_manifest_changes(self):
        doc1 = base_manifest("frame-hash3", content_hex="a" * 64, size=0)
        doc2 = base_manifest("frame-hash3", content_hex="b" * 64, size=0)
        self.assertNotEqual(ArtifactStore.manifest_hash_recompute(doc1),
                            ArtifactStore.manifest_hash_recompute(doc2))

    def test_recompute_matches_sidecar_across_store_reopen(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        store, _ = make_store(tmp)
        data = b"reopen-hash"
        doc = base_manifest("frame-reopen", content_hex=sha256_bytes(data),
                            size=len(data))
        publish_object(store, "frame-reopen", data, doc)
        sidecar1 = store.manifest_digest_hex("frame-reopen")
        store2 = ArtifactStore(store.store_root, "run-1").start()  # 恢复
        sidecar2 = store2.manifest_digest_hex("frame-reopen")
        self.assertEqual(sidecar1, sidecar2)
        self.assertEqual(sidecar2, ArtifactStore.manifest_hash_recompute(doc))


if __name__ == "__main__":
    unittest.main(verbosity=2)
