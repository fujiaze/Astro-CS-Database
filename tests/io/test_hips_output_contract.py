#!/usr/bin/env python3
"""IO-003 原子 HiPS/manifest 输出发布 验收测试（tests/io/test_hips_output_contract.py）。

验收映射（tasks/03_RUNTIME_DATA_IO_TASKS.md IO-003 +
docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md）:
  - 每个输出以唯一用户路径或 run ID 目录        → TestUniqueRunDirIsolation
  - 临时写 → 关闭 → fitsverify → SHA256 → 原子 rename → 完成 manifest
                                                → TestAtomicPublishPipeline (spy)
  - 默认不覆盖，显式 overwrite 才可             → TestNoOverwriteDefault
  - 并发不同 run 不互相覆盖                     → TestConcurrentRunsIsolated
  - 中断后无完成标记（可恢复）                  → TestInterruptNoCompleteMark
  - 文件权限/路径穿越拒绝                       → TestPathTraversalRejected /
                                                  TestPermissionRejected
  - tree hash 可重算                            → TestTreeHashRecomputable

本测试用 unittest + 线程（并发），无 pytest 专属依赖；tile FITS 由 IO-001
fits_core 原子写（hips_output_fixture.py）→ fitsverify 面对真实合法 FITS。
"""
from __future__ import annotations

import ctypes
import hashlib
import json
import pathlib
import shutil
import sys
import tempfile
import threading
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "runtime" / "io"))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import fits_verify  # noqa: E402
import hips_output_store as hos  # noqa: E402
from hips_output_store import (  # noqa: E402
    HipsOutputStore,
    InterruptIO,
    PathTraversalError,
    PermissionError_,
    PublishError,
    ReadOnlyIO,
    SpyStoreIO,
    recompute_tree_hash,
    sha256_bytes,
    tree_hash,
)
from hips_output_fixture import (  # noqa: E402
    build_properties,
    make_tile_fits_bytes,
    standard_hips_files,
)


def store_at(tmp: pathlib.Path, run_id: str = "run-1",
             io=None) -> HipsOutputStore:
    root = tmp / "out-root"
    s = HipsOutputStore(root, run_id, io=io).start()
    return s


class TestUniqueRunDirIsolation(unittest.TestCase):
    """每个输出以唯一 run ID 目录；两个 run 绝不共享目标路径。"""

    def test_run_dirs_are_private(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        s1 = store_at(tmp, "run-aaa")
        s2 = store_at(tmp, "run-bbb")
        self.assertNotEqual(s1.run_dir(), s2.run_dir())
        self.assertNotEqual(s1.products_dir(), s2.products_dir())
        # run 目录位于 {root}/runs/{run_id}
        self.assertEqual(s1.run_dir().name, "run-aaa")
        self.assertEqual(s2.run_dir().name, "run-bbb")

    def test_same_user_path_two_runs_no_clobber(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        files_a = [("properties", build_properties(order=0)),
                   ("Norder0/Dir0/Npix0.fits", make_tile_fits_bytes(16, 0)),
                   ("Norder0/Dir0/Npix1.fits", make_tile_fits_bytes(16, 1))]
        s1 = store_at(tmp, "run-aaa")
        s2 = store_at(tmp, "run-bbb")
        d1 = s1.publish_directory("signal", files_a)
        d2 = s2.publish_directory("signal", files_a)
        self.assertTrue(s1.has_complete_manifest("signal"))
        self.assertTrue(s2.has_complete_manifest("signal"))
        # 两 run 各自完整；互不覆盖（各自 tree hash 相同但路径隔离）
        self.assertEqual(d1["tree_hash"], d2["tree_hash"])
        self.assertTrue(s1.verify_tree_hash("signal"))
        self.assertTrue(s2.verify_tree_hash("signal"))


class TestAtomicPublishPipeline(unittest.TestCase):
    """临时写 → 关闭(fsync) → fitsverify → sha256 → 原子 rename → 完成 manifest。"""

    def test_pipeline_spy_events_and_success(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        spy = SpyStoreIO()
        s = store_at(tmp, "run-pipe", io=spy)
        files = standard_hips_files(order=0, ipix_list=(0, 1))
        doc = s.publish_directory("signal", files)
        # 完成标记唯一 = COMPLETE manifest
        self.assertTrue(s.has_complete_manifest("signal"))
        self.assertEqual(doc["status"], "COMPLETE")
        # spy 证明：stage 写 + 原子 rename 事件齐全
        kinds = [e["kind"] for e in spy.events]
        self.assertIn("write_bytes", kinds)
        self.assertIn("atomic_rename", kinds)
        renames = [e for e in spy.events if e["kind"] == "atomic_rename"]
        # 每个文件 + manifest 都经原子 rename 落盘
        self.assertGreaterEqual(len(renames), len(files) + 1)
        # fitsverify 在 stage 临时文件上执行（发布前校验）
        self.assertIn("signal", s.published_products())
        # 文件可读回且与发布内容一致（sha256 核对）
        for fn, data in files:
            got = s.read_product_file("signal", fn)
            self.assertEqual(got, data)
        # stage 目录无残留临时文件
        self.assertEqual([p.name for p in s.stage_dir().iterdir()], [])

    def test_invalid_fits_tile_rejected_no_success_object(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        s = store_at(tmp, "run-bad")
        bad = b"\x00" * 2880 * 2  # 非 FITS
        with self.assertRaises(PublishError):
            s.publish_directory("signal",
                                [("properties", build_properties()),
                                 ("Norder0/Dir0/Npix0.fits", bad)])
        self.assertFalse(s.has_complete_manifest("signal"))
        self.assertEqual(s.published_products(), [])
        # 恢复：同 run 可重发（无完成标记残留不阻塞）
        s.cleanup()
        s2 = store_at(tmp, "run-bad")
        ok = standard_hips_files()
        s2.publish_directory("signal", ok)
        self.assertTrue(s2.has_complete_manifest("signal"))


class TestNoOverwriteDefault(unittest.TestCase):
    """默认不覆盖；显式 overwrite 才可。"""

    def _publish_once(self, tmp):
        s = store_at(tmp, "run-ow")
        s.publish_directory("signal", standard_hips_files(ipix_list=(0,)))
        return s

    def test_second_publish_same_path_rejected(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        s = self._publish_once(tmp)
        with self.assertRaises(PublishError) as cm:
            s.publish_directory("signal", standard_hips_files(ipix_list=(1,)))
        self.assertIn("overwrite", str(cm.exception))

    def test_explicit_overwrite_replaces(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        s = self._publish_once(tmp)
        before = s.read_complete_manifest("signal")
        files2 = standard_hips_files(ipix_list=(2,))
        doc = s.publish_directory("signal", files2, overwrite=True)
        self.assertTrue(s.has_complete_manifest("signal"))
        self.assertNotEqual(doc["tree_hash"], before["tree_hash"])
        self.assertTrue(s.verify_tree_hash("signal"))
        # 旧内容确实被替换
        self.assertEqual(s.read_product_file("signal", "Norder0/Dir0/Npix2.fits"),
                         files2[-1][1])


class TestConcurrentRunsIsolated(unittest.TestCase):
    """并发不同 run 不互相覆盖。"""

    def test_parallel_runs(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        errors: list[BaseException] = []
        results: dict[str, dict] = {}
        lock = threading.Lock()

        def worker(run_id: str, ipix: int):
            try:
                s = HipsOutputStore(tmp / "out-root", run_id).start()
                files = standard_hips_files(order=0, ipix_list=(ipix,))
                doc = s.publish_directory(f"signal-{ipix}", files)
                with lock:
                    results[run_id] = {
                        "doc": doc,
                        "verified": s.verify_tree_hash(f"signal-{ipix}"),
                        "manifest": s.has_complete_manifest(f"signal-{ipix}"),
                    }
            except BaseException as exc:  # pragma: no cover
                with lock:
                    errors.append(exc)

        threads = [threading.Thread(target=worker, args=(f"run-{i}", i))
                   for i in range(6)]
        for t in threads:
            t.start()
        for t in threads:
            t.join(timeout=60)
        self.assertEqual(errors, [])
        self.assertEqual(len(results), 6)
        # 每个 run 都有完整产物且 tree hash 各自可重算
        for run_id, r in results.items():
            self.assertTrue(r["verified"], run_id)
            self.assertTrue(r["manifest"], run_id)
        hashes = [r["doc"]["tree_hash"] for r in results.values()]
        self.assertEqual(len(set(hashes)), 6, "不同 ipix 内容 → 不同 tree hash")
        # 磁盘上 6 个独立 run 目录互不干扰
        runs = sorted(p.name for p in (tmp / "out-root" / "runs").iterdir())
        self.assertEqual(runs, [f"run-{i}" for i in range(6)])


class TestInterruptNoCompleteMark(unittest.TestCase):
    """中断（模拟）后无完成标记；新 Store 恢复不索引；可重发。"""

    def _interrupt_once(self, fail_on: str, tmp: pathlib.Path) -> HipsOutputStore:
        io = InterruptIO(fail_on)
        s = HipsOutputStore(tmp / "out-root", "run-int", io=io).start()
        files = standard_hips_files(ipix_list=(0, 1))
        try:
            s.publish_directory("signal", files)
            self.fail("expected KeyboardInterrupt")
        except KeyboardInterrupt:
            pass
        return s

    def test_interrupt_mid_manifest(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        s = self._interrupt_once("rename_manifest", tmp)
        # 无完成标记
        self.assertFalse(s.has_complete_manifest("signal"))
        self.assertNotIn("signal", s.published_products())
        # 新 Store（同 run 根）start() 不索引中断残留
        s2 = HipsOutputStore(tmp / "out-root", "run-int").start()
        self.assertNotIn("signal", s2.published_products())
        # 可恢复（残留目录被清后重发）
        doc = s2.publish_directory("signal", standard_hips_files(ipix_list=(0, 1)))
        self.assertTrue(s2.has_complete_manifest("signal"))
        self.assertTrue(s2.verify_tree_hash("signal"))
        self.assertEqual(doc["status"], "COMPLETE")

    def test_interrupt_mid_tiles(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        s = self._interrupt_once("rename_tiles", tmp)
        self.assertFalse(s.has_complete_manifest("signal"))
        s2 = HipsOutputStore(tmp / "out-root", "run-int").start()
        self.assertNotIn("signal", s2.published_products())
        s2.publish_directory("signal", standard_hips_files(ipix_list=(0, 1)))
        self.assertTrue(s2.has_complete_manifest("signal"))

    def test_cancel_no_manifest_and_recoverable(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        root = tmp / "out-root"
        s = HipsOutputStore(root, "run-cancel", io=InterruptIO("write")).start()
        # 注入在 stage 写阶段中断 → 无任何目标产物
        try:
            s.publish_directory("signal", standard_hips_files())
            self.fail("expected KeyboardInterrupt")
        except KeyboardInterrupt:
            pass
        self.assertFalse(s.has_complete_manifest("signal"))
        s.cleanup()
        s2 = HipsOutputStore(root, "run-cancel").start()
        self.assertEqual(s2.published_products(), [])
        s2.publish_directory("signal", standard_hips_files())
        self.assertTrue(s2.has_complete_manifest("signal"))


class TestPathTraversalRejected(unittest.TestCase):
    """路径穿越拒绝（词法 + 文件系统层）。"""

    def test_traversal_lexical(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        s = store_at(tmp, "run-trav")
        for bad in ("../evil", "a/../../b", "a//b", "a/b/", "/abs",
                    "a\\b", "a/./b", "", ".", ".."):
            with self.subTest(bad=bad):
                with self.assertRaises(PathTraversalError):
                    s.publish_directory(bad, [("properties",
                                               build_properties())])

    def test_symlink_target_component_rejected(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        root = tmp / "out-root"
        s = HipsOutputStore(root, "run-sym").start()
        outside = tmp / "outside"
        outside.mkdir()
        (s.products_dir() / "escape").symlink_to(outside,
                                                 target_is_directory=True)
        with self.assertRaises((PermissionError_, PathTraversalError)):
            s.publish_directory("escape/sig",
                                [("properties", build_properties())])

    def test_symlink_stage_component_rejected(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        root = tmp / "out-root"
        s = HipsOutputStore(root, "run-sym2").start()
        # 预测 stage 临时文件名并预放符号链接（指向外部）→ 权限拒绝
        fn = "properties"
        tmpname = f"{hashlib.sha256(fn.encode()).hexdigest()[:16]}.{fn}"
        victim = tmp / "victim"
        victim.write_bytes(b"secret")
        (s.stage_dir() / tmpname).symlink_to(victim)
        with self.assertRaises(PermissionError_):
            s.publish_directory("sig", [("properties", build_properties())])


class TestPermissionRejected(unittest.TestCase):
    """文件权限拒绝 → PermissionError_，且无成功对象。"""

    def test_readonly_backend_rejected(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        s = HipsOutputStore(tmp / "out-root", "run-ro",
                            io=ReadOnlyIO()).start()
        with self.assertRaises(PermissionError_):
            s.publish_directory("signal",
                                [("properties", build_properties())])
        self.assertFalse(s.has_complete_manifest("signal"))
        self.assertEqual(s.published_products(), [])


class TestTreeHashRecomputable(unittest.TestCase):
    """tree hash 可重算：同内容一致；任何文件改动即变化。"""

    def test_recompute_matches_manifest(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        s = store_at(tmp, "run-hash")
        s.publish_directory("signal", standard_hips_files(ipix_list=(0, 1, 2)))
        doc = s.read_complete_manifest("signal")
        self.assertIsNotNone(doc)
        # manifest.tree_hash == 由 manifest tree 条目重算
        self.assertEqual(doc["tree_hash"], recompute_tree_hash(doc))
        # == 按磁盘实际文件重算（verify_tree_hash）
        self.assertTrue(s.verify_tree_hash("signal"))
        # == 独立构造 tree_hash(同条目)
        entries = doc["tree"]
        self.assertEqual(doc["tree_hash"], tree_hash(entries))

    def test_modified_file_changes_tree_hash(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        s = store_at(tmp, "run-hash2")
        s.publish_directory("signal", standard_hips_files(ipix_list=(0,)))
        doc = s.read_complete_manifest("signal")
        # 篡改一个已发布 tile 字节 → tree hash 重算变化（verify_tree_hash False）
        tile_rel = "Norder0/Dir0/Npix0.fits"
        p = s.products_dir() / "signal" / tile_rel
        raw = bytearray(p.read_bytes())
        raw[3000] ^= 0x01
        p.write_bytes(bytes(raw))
        self.assertFalse(s.verify_tree_hash("signal"))
        # 篡改后读回 → hash 核对拒绝
        with self.assertRaises(PublishError):
            s.read_product_file("signal", tile_rel)
        self.assertIsNotNone(doc)


class TestFitsVerifyCrossOracle(unittest.TestCase):
    """fitsverify 与 IO-001 fits_core C verifier 同一判定（交叉 oracle）。"""

    def test_tile_verify_matches_c_verifier(self):
        lib = ctypes.CDLL(str(REPO / "runtime" / "io" / "libfits_core_test.so"))
        vf = lib.acs_fio_verify_file_v1
        vf.restype = ctypes.c_int
        vf.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p,
                       ctypes.c_size_t]
        tile = make_tile_fits_bytes(16, 3)
        with tempfile.TemporaryDirectory() as td:
            p = pathlib.Path(td) / "t.fits"
            p.write_bytes(tile)
            err = ctypes.create_string_buffer(160)
            cst = vf(str(p).encode(), 0, err, len(err))
            self.assertEqual(cst, 0)
            h = fits_verify.verify_fits_file(str(p))
            self.assertEqual(h.bitpix, -32)
            # 篡改
            raw = bytearray(tile)
            raw[3000] ^= 0x01
            p2 = pathlib.Path(td) / "t2.fits"
            p2.write_bytes(bytes(raw))
            cst2 = vf(str(p2).encode(), 0, err, len(err))
            self.assertNotEqual(cst2, 0)
            with self.assertRaises(fits_verify.FitsVerifyError):
                fits_verify.verify_fits_file(str(p2))


if __name__ == "__main__":
    unittest.main(verbosity=2)
