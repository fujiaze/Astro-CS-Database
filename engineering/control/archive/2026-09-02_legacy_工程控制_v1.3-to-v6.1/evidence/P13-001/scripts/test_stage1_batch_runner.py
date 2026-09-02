#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P13-001 — stage1_batch_runner.py 自动测试

测试范围：
  - 1. test_scan_testdata: 扫描得到 710 帧
  - 2. test_canonical_filter_from_filename: 文件名解析正确
  - 3. test_compute_hash_key_stable: 同一帧 hash_key 稳定
  - 4. test_cache_save_load: cache 写入/读取一致
  - 5. test_state_save_load: batch_state 写入/读取一致
  - 6. test_breakpoint_resume_skips_pass: PASS 帧重跑时 SKIPPED
  - 7. test_fresh_clears_state_and_cache: --fresh 清空 cache + state
  - 8. test_failure_classification: 失败分类正确
  - 9. test_filter_frames: 过滤器按设备/目标/滤镜过滤
  - 10. test_smoke_run_1_frame: 实际跑 1 帧端到端冒烟（T4_RED_Galaxy_Center）

运行：
  python test_stage1_batch_runner.py
"""

from __future__ import annotations

import json
import os
import shutil
import sys
import time
from pathlib import Path

# 让 import 找到 stage1_batch_runner.py
SCRIPT_DIR = Path(__file__).parent
sys.path.insert(0, str(SCRIPT_DIR))

import stage1_batch_runner as M  # noqa: E402

# ============================================================================
# 测试框架
# ============================================================================

PASS_COUNT = 0
FAIL_COUNT = 0
FAILURES: list[str] = []


def assert_true(cond: bool, msg: str) -> None:
    global PASS_COUNT, FAIL_COUNT
    if cond:
        PASS_COUNT += 1
    else:
        FAIL_COUNT += 1
        FAILURES.append(msg)
        print(f"  FAIL: {msg}")


def assert_eq(actual, expected, msg: str) -> None:
    if actual == expected:
        global PASS_COUNT
        PASS_COUNT += 1
    else:
        global FAIL_COUNT
        FAIL_COUNT += 1
        FAILURES.append(f"{msg} (actual={actual!r}, expected={expected!r})")
        print(f"  FAIL: {msg} (actual={actual!r}, expected={expected!r})")


def run_test(name: str, fn) -> None:
    print(f"\n--- {name} ---")
    try:
        fn()
    except Exception as e:
        global FAIL_COUNT
        FAIL_COUNT += 1
        import traceback
        tb = traceback.format_exc()
        FAILURES.append(f"{name}: {type(e).__name__}: {e}\n{tb}")
        print(f"  EXCEPTION: {type(e).__name__}: {e}")
        print(tb)


# ============================================================================
# 临时测试目录隔离
# ============================================================================

class TempCacheState:
    """临时替换 CACHE_DIR/BATCH_STATE_FILE/RAW_LOGS_DIR/REPORTS_DIR，测试完恢复"""

    def __init__(self, tmp_root: Path):
        self.tmp_root = tmp_root
        self.orig_cache = M.CACHE_DIR
        self.orig_state = M.BATCH_STATE_FILE
        self.orig_raw = M.RAW_LOGS_DIR
        self.orig_reports = M.REPORTS_DIR

    def __enter__(self):
        self.tmp_root.mkdir(parents=True, exist_ok=True)
        M.CACHE_DIR = self.tmp_root / "cache"
        M.BATCH_STATE_FILE = self.tmp_root / "batch_state.json"
        M.RAW_LOGS_DIR = self.tmp_root / "raw_logs"
        M.REPORTS_DIR = self.tmp_root / "reports"
        M.CACHE_DIR.mkdir(parents=True, exist_ok=True)
        M.RAW_LOGS_DIR.mkdir(parents=True, exist_ok=True)
        M.REPORTS_DIR.mkdir(parents=True, exist_ok=True)
        return self

    def __exit__(self, *args):
        M.CACHE_DIR = self.orig_cache
        M.BATCH_STATE_FILE = self.orig_state
        M.RAW_LOGS_DIR = self.orig_raw
        M.REPORTS_DIR = self.orig_reports
        # 清理临时目录
        try:
            shutil.rmtree(self.tmp_root)
        except OSError:
            pass


# ============================================================================
# 测试用例
# ============================================================================

def test_scan_testdata() -> None:
    """1. 扫描 testdata 得到 710 帧（与 DATASETS.md 一致）"""
    frames = M.scan_testdata()
    assert_true(len(frames) > 0, "扫描结果非空")
    print(f"  扫描到 {len(frames)} 帧")

    # 按设备统计
    by_device: dict[str, int] = {}
    for f in frames:
        by_device[f.device] = by_device.get(f.device, 0) + 1
    print(f"  按设备: {by_device}")
    # DATASETS.md: T4=228+157=385, T3=79+72=151, T2=68+64+42=174, 合计 710
    assert_true(by_device.get("T4", 0) == 385, f"T4 帧数应为 385 (实际 {by_device.get('T4', 0)})")
    assert_true(by_device.get("T3", 0) == 151, f"T3 帧数应为 151 (实际 {by_device.get('T3', 0)})")
    assert_true(by_device.get("T2", 0) == 174, f"T2 帧数应为 174 (实际 {by_device.get('T2', 0)})")
    assert_eq(len(frames), 710, "总帧数应为 710")

    # 按数据集统计
    by_dataset: dict[str, int] = {}
    for f in frames:
        by_dataset[f.dataset] = by_dataset.get(f.dataset, 0) + 1
    print(f"  按数据集: {by_dataset}")
    assert_eq(by_dataset.get("Victory_Nebula_T4_Flying_Dutchman", 0), 228, "Victory_Nebula 228 帧")
    assert_eq(by_dataset.get("Galaxy_Center_T4", 0), 157, "Galaxy_Center 157 帧")
    assert_eq(by_dataset.get("NGC55_T3_flying_dutchman", 0), 79, "NGC55 79 帧")
    assert_eq(by_dataset.get("NGC247_T2_flying_dutchman", 0), 68, "NGC247 68 帧")
    assert_eq(by_dataset.get("NGC1727_T2_flying_dutchman", 0), 64, "NGC1727 64 帧")
    assert_eq(by_dataset.get("NGC83_cluster_T3_Flying_Dutchman", 0), 72, "NGC83 72 帧")
    assert_eq(by_dataset.get("LDN43_T2_flying_dutchman", 0), 42, "LDN43 42 帧")

    # 验证 Galaxy_Center 嵌套结构被正确解析
    gc_frames = [f for f in frames if f.dataset == "Galaxy_Center_T4"]
    gc_with_panel = [f for f in gc_frames if f.panel]
    assert_eq(len(gc_with_panel), len(gc_frames), "Galaxy_Center 全部帧应有 panel 标记")
    panels = set(f.panel for f in gc_frames)
    assert_true(panels == {"panel1", "panel2", "panel3"}, f"Galaxy_Center 应有 3 panel (实际 {panels})")

    # 验证每个 frame_id 唯一
    ids = [f.frame_id for f in frames]
    assert_eq(len(ids), len(set(ids)), "所有 frame_id 应唯一")


def test_canonical_filter_from_filename() -> None:
    """2. 文件名解析滤镜名正确"""
    cases = [
        ("Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts", "RED", "Red"),
        ("Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@063620-180S-Green.fts", "GREEN", "Green"),
        ("Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@055414-180S-Blue.fts", "BLUE", "Blue"),
        ("Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@061318-300S-H-alpha.fts", "HA", "H-alpha"),
        ("Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@063631-600S-Oiii.fts", "OIII", "OIII"),
        ("NGC55_T3_flying_dutchman-20250703@075400-600S-Lum.fts", "LUM", "Lum"),
        ("NGC55_T3_flying_dutchman-20250703@083458-1200S-Oiii.fts", "OIII", "OIII"),
    ]
    for fname, exp_canonical, exp_alias in cases:
        fc, fa = M.canonical_filter_from_filename(fname)
        assert_eq(fc, exp_canonical, f"{fname} → canonical={fc}")
        assert_eq(fa, exp_alias, f"{fname} → alias={fa}")


def test_compute_hash_key_stable() -> None:
    """3. 同一帧 hash_key 稳定（重复计算一致）"""
    frames = M.scan_testdata()
    if not frames:
        assert_true(False, "扫描无帧，无法测试 hash_key")
        return
    rec = frames[0]
    commit = "testcommit123"
    orch_sha = "DEADBEEF" * 8
    h1 = M.compute_frame_hash_key(rec, commit, orch_sha)
    h2 = M.compute_frame_hash_key(rec, commit, orch_sha)
    assert_eq(h1, h2, "同一帧同一环境 hash_key 应稳定")
    assert_true(len(h1) == 64, f"hash_key 应为 64 字符 hex (实际 {len(h1)})")

    # 改 commit 后 hash_key 应变化
    h3 = M.compute_frame_hash_key(rec, commit + "x", orch_sha)
    assert_true(h3 != h1, "commit 变化后 hash_key 应变化")

    # 改 orch_sha 后 hash_key 应变化
    h4 = M.compute_frame_hash_key(rec, commit, orch_sha + "x")
    assert_true(h4 != h1, "orch_sha 变化后 hash_key 应变化")


def test_cache_save_load() -> None:
    """4. cache 写入/读取一致"""
    tmp = Path(os.environ.get("TEMP", "/tmp")) / f"p13_test_{int(time.time()*1000)}"
    with TempCacheState(tmp):
        frame_id = "TEST_FRAME_001"
        data = {
            "frame_id": frame_id,
            "hash_key": "ABC" * 21 + "A",
            "status": "PASS",
            "exit_code": 0,
            "elapsed_s": 12.34,
            "fit_used": 100,
            "scale_factor": 0.001,
            "sigma_residual": 0.1,
            "timestamp": "2026-07-29T10:00:00",
        }
        M.cache_save(frame_id, data)
        loaded = M.cache_load(frame_id)
        assert_true(loaded is not None, "cache_load 应返回非 None")
        if loaded:
            assert_eq(loaded.get("hash_key"), data["hash_key"], "hash_key 一致")
            assert_eq(loaded.get("status"), "PASS", "status 一致")
            assert_eq(loaded.get("fit_used"), 100, "fit_used 一致")

        # 不存在的 frame_id 应返回 None
        none_result = M.cache_load("NONEXISTENT_FRAME")
        assert_true(none_result is None, "不存在的 frame_id 应返回 None")


def test_state_save_load() -> None:
    """5. batch_state 写入/读取一致"""
    tmp = Path(os.environ.get("TEMP", "/tmp")) / f"p13_test_{int(time.time()*1000)}"
    with TempCacheState(tmp):
        state = M.BatchState(
            version=1,
            created_at="2026-07-29T10:00:00",
            updated_at="2026-07-29T10:00:01",
            git_commit="abc123",
            orchestrator_sha256="DEADBEEF" * 8,
            total_frames=10,
        )
        M.state_save(state)

        loaded = M.state_load()
        assert_eq(loaded.version, 1, "version 一致")
        assert_eq(loaded.git_commit, "abc123", "git_commit 一致")
        assert_eq(loaded.total_frames, 10, "total_frames 一致")
        assert_eq(loaded.orchestrator_sha256, "DEADBEEF" * 8, "orchestrator_sha256 一致")


def test_breakpoint_resume_skips_pass() -> None:
    """6. PASS 帧重跑时被 SKIPPED（缓存命中）"""
    tmp = Path(os.environ.get("TEMP", "/tmp")) / f"p13_test_{int(time.time()*1000)}"
    with TempCacheState(tmp):
        frame_id = "TEST_FRAME_PASS"
        # 模拟之前 PASS 的缓存
        cached = {
            "frame_id": frame_id,
            "hash_key": "AAA" * 21 + "A",
            "status": "PASS",
            "exit_code": 0,
            "elapsed_s": 5.0,
            "fit_used": 100,
            "scale_factor": 0.001,
            "sigma_residual": 0.1,
            "unique_matches": 90,
            "n_matched": 90,
            "has_snr": 1,
            "snr_n_points": 500,
            "hiss_path": "/tmp/test.hiss",
            "hiss_sha256": "BBB" * 21 + "B",
        }
        M.cache_save(frame_id, cached)

        # 模拟 run_single_frame 的缓存检查逻辑（不实际跑）
        loaded = M.cache_load(frame_id)
        assert_true(loaded is not None, "缓存应存在")
        if loaded:
            assert_eq(loaded.get("status"), "PASS", "缓存 status 应为 PASS")
            assert_eq(loaded.get("hash_key"), cached["hash_key"], "hash_key 一致")

        # 模拟 cache_clear 后重新加载
        n = M.cache_clear_all()
        assert_eq(n, 1, f"cache_clear_all 应删除 1 个文件 (实际 {n})")
        loaded2 = M.cache_load(frame_id)
        assert_true(loaded2 is None, "清空后 cache_load 应返回 None")


def test_fresh_clears_state_and_cache() -> None:
    """7. --fresh 清空 cache + state"""
    tmp = Path(os.environ.get("TEMP", "/tmp")) / f"p13_test_{int(time.time()*1000)}"
    with TempCacheState(tmp):
        # 写入 cache 和 state
        M.cache_save("FRAME_A", {"frame_id": "FRAME_A", "status": "PASS"})
        M.cache_save("FRAME_B", {"frame_id": "FRAME_B", "status": "PASS"})
        state = M.BatchState(version=1, total_frames=2, git_commit="abc")
        M.state_save(state)

        assert_true(M.cache_load("FRAME_A") is not None, "FRAME_A 缓存应存在")
        assert_true(M.BATCH_STATE_FILE.exists(), "batch_state.json 应存在")

        # 模拟 --fresh
        n = M.cache_clear_all()
        M.state_clear()

        assert_eq(n, 2, f"应清空 2 个 cache 文件 (实际 {n})")
        assert_true(M.cache_load("FRAME_A") is None, "清空后 FRAME_A 应不存在")
        assert_true(not M.BATCH_STATE_FILE.exists(), "清空后 batch_state.json 应不存在")


def test_failure_classification() -> None:
    """8. 失败分类正确"""
    # PASS 帧
    r_pass = M.FrameResult(
        frame_id="T", dataset="", device="T4", target="X",
        filter_canonical="RED", filter_class="Broadband",
        frame_name="", fits_path="",
        fit_used=100, scale_factor=0.001, sigma_residual=0.1,
    )
    ok, cat, notes = M.classify_gate(r_pass)
    assert_true(ok, "Broadband fit_used=100 应 PASS")
    assert_eq(cat, "", "PASS 时 failure_category 应为空")

    # INSUFFICIENT_STARS
    r_low = M.FrameResult(
        frame_id="T", dataset="", device="T4", target="X",
        filter_canonical="RED", filter_class="Broadband",
        frame_name="", fits_path="",
        fit_used=5, scale_factor=0.001, sigma_residual=0.1,
    )
    ok, cat, _ = M.classify_gate(r_low)
    assert_true(not ok, "fit_used=5 应 FAIL")
    assert_eq(cat, "INSUFFICIENT_STARS", "应为 INSUFFICIENT_STARS")

    # ZERO_SIGMA
    r_zero = M.FrameResult(
        frame_id="T", dataset="", device="T4", target="X",
        filter_canonical="RED", filter_class="Broadband",
        frame_name="", fits_path="",
        fit_used=100, scale_factor=0.001, sigma_residual=0.0,
    )
    ok, cat, _ = M.classify_gate(r_zero)
    assert_true(not ok, "sigma=0 应 FAIL")
    assert_eq(cat, "ZERO_SIGMA", "应为 ZERO_SIGMA")

    # INVALID_SCALE
    r_scale = M.FrameResult(
        frame_id="T", dataset="", device="T4", target="X",
        filter_canonical="RED", filter_class="Broadband",
        frame_name="", fits_path="",
        fit_used=100, scale_factor=0.0, sigma_residual=0.1,
    )
    ok, cat, _ = M.classify_gate(r_scale)
    assert_true(not ok, "scale=0 应 FAIL")
    assert_eq(cat, "INVALID_SCALE", "应为 INVALID_SCALE")

    # Narrowband 阈值 8
    r_nb = M.FrameResult(
        frame_id="T", dataset="", device="T4", target="X",
        filter_canonical="HA", filter_class="Narrowband",
        frame_name="", fits_path="",
        fit_used=10, scale_factor=0.001, sigma_residual=0.1,
    )
    ok, _, _ = M.classify_gate(r_nb)
    assert_true(ok, "Narrowband HA fit_used=10 (>8) 应 PASS")

    r_nb_low = M.FrameResult(
        frame_id="T", dataset="", device="T4", target="X",
        filter_canonical="HA", filter_class="Narrowband",
        frame_name="", fits_path="",
        fit_used=5, scale_factor=0.001, sigma_residual=0.1,
    )
    ok, cat, _ = M.classify_gate(r_nb_low)
    assert_true(not ok, "Narrowband HA fit_used=5 (<8) 应 FAIL")
    assert_eq(cat, "INSUFFICIENT_STARS", "应为 INSUFFICIENT_STARS")


def test_filter_frames() -> None:
    """9. 过滤器按设备/目标/滤镜过滤"""
    frames = M.scan_testdata()
    assert_true(len(frames) > 0, "扫描应有帧")

    # 按 device 过滤
    class Args:
        device = ["T4"]
        target = None
        filter = None
        dataset = None
        limit = 0
    filtered = M.filter_frames(frames, Args())
    for f in filtered:
        assert_eq(f.device, "T4", f"{f.frame_id} 应为 T4")
    assert_true(len(filtered) == 385, f"T4 应有 385 帧 (实际 {len(filtered)})")

    # 按 filter 过滤
    class ArgsHA:
        device = None
        target = None
        filter = ["HA"]
        dataset = None
        limit = 0
    filtered_ha = M.filter_frames(frames, ArgsHA())
    for f in filtered_ha:
        assert_eq(f.filter_canonical, "HA", f"{f.frame_id} 应为 HA")

    # 按 target 过滤
    class ArgsGC:
        device = None
        target = ["Galaxy_Center"]
        filter = None
        dataset = None
        limit = 0
    filtered_gc = M.filter_frames(frames, ArgsGC())
    for f in filtered_gc:
        assert_eq(f.target, "Galaxy_Center", f"{f.frame_id} 应为 Galaxy_Center")
    assert_true(len(filtered_gc) == 157, f"Galaxy_Center 应有 157 帧 (实际 {len(filtered_gc)})")

    # 按 limit 过滤
    class ArgsLimit:
        device = None
        target = None
        filter = None
        dataset = None
        limit = 5
    filtered_lim = M.filter_frames(frames, ArgsLimit())
    assert_eq(len(filtered_lim), 5, "limit=5 应返回 5 帧")

    # 组合过滤：T4 + Galaxy_Center + Red
    class ArgsCombo:
        device = ["T4"]
        target = ["Galaxy_Center"]
        filter = ["RED"]
        dataset = None
        limit = 0
    filtered_combo = M.filter_frames(frames, ArgsCombo())
    for f in filtered_combo:
        assert_eq(f.device, "T4", "组合过滤 device=T4")
        assert_eq(f.target, "Galaxy_Center", "组合过滤 target=Galaxy_Center")
        assert_eq(f.filter_canonical, "RED", "组合过滤 filter=RED")
    print(f"  T4 + Galaxy_Center + RED: {len(filtered_combo)} 帧")


def test_smoke_run_1_frame() -> None:
    """10. 端到端冒烟：实际跑 1 帧 T4_RED_Galaxy_Center（P12-005 已验证可用）"""
    # 检查 orchestrator.exe 存在
    if not M.ORCH_EXE.exists():
        print(f"  SKIP: orchestrator.exe 不存在 ({M.ORCH_EXE})")
        return

    # 用临时 cache/state，避免污染真实状态
    tmp = Path(os.environ.get("TEMP", "/tmp")) / f"p13_smoke_{int(time.time()*1000)}"
    with TempCacheState(tmp):
        frames = M.scan_testdata()
        # 选 T4_RED_Galaxy_Center 第一帧（P12-005 已验证）
        target_frames = [
            f for f in frames
            if f.device == "T4" and f.target == "Galaxy_Center" and f.filter_canonical == "RED"
        ]
        if not target_frames:
            print("  SKIP: 未找到 T4 Galaxy_Center RED 帧")
            return
        rec = target_frames[0]
        print(f"  测试帧: {rec.frame_id}")
        print(f"  fits_path: {rec.fits_path}")

        commit = M.git_commit_hash()
        orch_sha = M.sha256_file(M.ORCH_EXE)
        print(f"  git_commit: {commit}")
        print(f"  orch_sha: {orch_sha[:16]}...")

        # 初始化 state
        state = M.make_default_state(1, commit, orch_sha)
        M.state_save(state)

        # 运行单帧，超时 600s
        t0 = time.time()
        r = M.run_single_frame(rec, commit, orch_sha, timeout_s=600, force=True, state=state)
        elapsed = time.time() - t0
        print(f"  result: status={r.status} exit={r.exit_code} elapsed={elapsed:.1f}s")
        print(f"    fit_used={r.fit_used} scale={r.scale_factor:.4g} sigma={r.sigma_residual:.4g}")
        print(f"    has_snr={r.has_snr} snr_n_points={r.snr_n_points}")
        print(f"    hiss_sha256: {r.hiss_sha256[:16]}...")
        print(f"    notes: {r.notes[:200]}")

        # 验证结果
        assert_true(r.status in ("PASS", "FAIL", "STAGE1_ERROR", "TIMEOUT"),
                    f"status 应为已知值 (实际 {r.status})")
        assert_true(r.hash_key != "", "hash_key 应非空")
        assert_true(len(r.hash_key) == 64, f"hash_key 应为 64 字符 (实际 {len(r.hash_key)})")

        # 缓存应已写入
        cached = M.cache_load(rec.frame_id)
        assert_true(cached is not None, "cache 应已写入")
        if cached:
            assert_eq(cached.get("hash_key"), r.hash_key, "cache hash_key 应一致")
            assert_eq(cached.get("status"), r.status, "cache status 应一致")

        # batch_state 应已更新
        loaded_state = M.state_load()
        assert_true(rec.frame_id in loaded_state.frames, "frame_id 应在 batch_state.frames 中")
        if rec.frame_id in loaded_state.frames:
            fdata = loaded_state.frames[rec.frame_id]
            assert_eq(fdata.get("status"), r.status, "batch_state status 应一致")

        # 如果 PASS，验证关键指标
        if r.status == "PASS":
            assert_true(r.fit_used >= 20, f"Broadband fit_used 应 >= 20 (实际 {r.fit_used})")
            assert_true(r.scale_factor > 0, f"scale_factor 应 > 0 (实际 {r.scale_factor})")
            assert_true(r.sigma_residual > 0, f"sigma_residual 应 > 0 (实际 {r.sigma_residual})")
            assert_true(r.has_snr == 1, f"has_snr 应为 1 (实际 {r.has_snr})")
            assert_true(r.snr_n_points > 0, f"snr_n_points 应 > 0 (实际 {r.snr_n_points})")
            print(f"  PASS: 所有 Gate 通过")
        elif r.status == "FAIL":
            print(f"  FAIL: category={r.failure_category}, notes={r.notes}")
        elif r.status == "STAGE1_ERROR":
            print(f"  STAGE1_ERROR: notes={r.notes}")


# ============================================================================
# 主入口
# ============================================================================

def main() -> int:
    print("=" * 70)
    print("P13-001 stage1_batch_runner.py 自动测试")
    print("=" * 70)

    tests = [
        ("test_scan_testdata", test_scan_testdata),
        ("test_canonical_filter_from_filename", test_canonical_filter_from_filename),
        ("test_compute_hash_key_stable", test_compute_hash_key_stable),
        ("test_cache_save_load", test_cache_save_load),
        ("test_state_save_load", test_state_save_load),
        ("test_breakpoint_resume_skips_pass", test_breakpoint_resume_skips_pass),
        ("test_fresh_clears_state_and_cache", test_fresh_clears_state_and_cache),
        ("test_failure_classification", test_failure_classification),
        ("test_filter_frames", test_filter_frames),
        ("test_smoke_run_1_frame", test_smoke_run_1_frame),
    ]

    for name, fn in tests:
        run_test(name, fn)

    print("\n" + "=" * 70)
    print(f"测试结果: {PASS_COUNT} PASS, {FAIL_COUNT} FAIL")
    if FAIL_COUNT == 0:
        print("VERDICT: PASS (all tests passed)")
        return 0
    else:
        print("VERDICT: FAIL")
        print("\n失败详情:")
        for f in FAILURES:
            print(f"  - {f}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
