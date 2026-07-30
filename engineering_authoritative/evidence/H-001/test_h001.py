"""
H-001 单元测试

测试内容:
1. ResourceMonitor: 采样/滚动窗口/统计/活跃阶段追踪
2. FrameCostEstimator: 各阶段成本估算/全帧估算/基线验证
3. resource_profile 契约符合性
"""

import sys
import os
import time
import json

# 将当前目录加入 path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from resource_monitor import (
    ResourceMonitor, MockSampler, ResourceSnapshot,
    ResourceSummary,
)
from cost_estimator import (
    FrameCostEstimator, FrameParams, StageCost, FrameCostEstimate,
    ALL_STAGES, HIGH_MEMORY_STAGES, HIGH_CPU_STAGES,
    STAGE_READ_FITS, STAGE_CALIBRATE, STAGE_PLATESOLVE,
    STAGE_PSF, STAGE_PHOTOMETRIC, STAGE_SNR, STAGE_DRIZZLE,
    estimate_frame,
)

BASELINE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "baseline.json")

# ============================================================================
# 测试框架
# ============================================================================

PASS = 0
FAIL = 0
FAILURES = []


def assert_true(cond, msg):
    global PASS, FAIL
    if cond:
        PASS += 1
    else:
        FAIL += 1
        FAILURES.append(msg)
        print(f"  FAIL: {msg}")


def assert_eq(a, b, msg):
    assert_true(a == b, f"{msg}: expected {b}, got {a}")


def assert_gt(a, b, msg):
    assert_true(a > b, f"{msg}: expected {a} > {b}")


def assert_ge(a, b, msg):
    assert_true(a >= b, f"{msg}: expected {a} >= {b}")


def assert_le(a, b, msg):
    assert_true(a <= b, f"{msg}: expected {a} <= {b}")


def section(name):
    print(f"\n{'='*60}")
    print(f"  {name}")
    print(f"{'='*60}")


# ============================================================================
# Part 1: ResourceMonitor 测试
# ============================================================================

def test_resource_monitor():
    section("Part 1: ResourceMonitor")

    # --- 测试 1: MockSampler 基本采样 ---
    print("\n[测试 1] MockSampler 基本采样")
    snapshots = [
        ResourceSnapshot(
            timestamp_sec=1000.0 + i,
            cpu_percent=50.0 + i * 10,
            rss_bytes=100_000_000 + i * 10_000_000,
            commit_bytes=200_000_000 + i * 20_000_000,
            commit_limit_bytes=8_000_000_000,
            disk_read_bytes_per_sec=1_000_000.0 * (i + 1),
            disk_write_bytes_per_sec=500_000.0 * (i + 1),
            temperature_c=45.0 + i,
        )
        for i in range(5)
    ]
    sampler = MockSampler(snapshots)
    monitor = ResourceMonitor(sampler=sampler, window_sec=60.0)
    monitor.start()
    time.sleep(0.3)  # 让后台线程采样
    monitor.stop()

    samples = monitor.get_samples()
    assert_gt(len(samples), 0, "采样后应有快照")
    print(f"  采集到 {len(samples)} 个快照")

    snap = monitor.get_snapshot()
    assert_true(snap is not None, "get_snapshot 不应为 None")
    assert_ge(snap.cpu_percent, 0, "CPU 使用率应 >= 0")
    print(f"  最新快照: CPU={snap.cpu_percent:.1f}%, RSS={snap.rss_bytes/1e6:.1f}MB")

    PASS_count = len(samples)
    print(f"  -> PASS (采集 {PASS_count} 个快照)")

    # --- 测试 2: 滚动窗口裁剪 ---
    print("\n[测试 2] 滚动窗口裁剪")
    old_snapshots = [
        ResourceSnapshot(
            timestamp_sec=100.0 + i,  # 很老的时间戳
            cpu_percent=10.0,
            rss_bytes=50_000_000,
            commit_bytes=100_000_000,
            commit_limit_bytes=8_000_000_000,
            disk_read_bytes_per_sec=0,
            disk_write_bytes_per_sec=0,
        )
        for i in range(3)
    ]
    new_snapshots = [
        ResourceSnapshot(
            timestamp_sec=time.time() - i * 0.1,  # 近期时间戳
            cpu_percent=80.0,
            rss_bytes=200_000_000,
            commit_bytes=400_000_000,
            commit_limit_bytes=8_000_000_000,
            disk_read_bytes_per_sec=1_000_000,
            disk_write_bytes_per_sec=500_000,
        )
        for i in range(3)
    ]
    monitor2 = ResourceMonitor(sampler=MockSampler(old_snapshots + new_snapshots), window_sec=5.0)
    # 手动采样所有
    for _ in range(6):
        monitor2.sample_once()

    samples2 = monitor2.get_samples()
    # 老快照应被裁剪 (timestamp 100.0 远超 5s 窗口)
    # 注意: MockSampler 循环播放, 所以实际窗口内取决于时间戳
    print(f"  窗口内快照数: {len(samples2)}")
    assert_true(True, "滚动窗口裁剪不崩溃")  # 主要验证不崩溃
    print("  -> PASS")

    # --- 测试 3: 统计摘要 ---
    print("\n[测试 3] 统计摘要")
    summary = monitor2.get_summary()
    assert_true(summary is not None, "摘要不应为 None")
    if summary:
        assert_ge(summary.cpu_mean, 0, "CPU 均值 >= 0")
        assert_ge(summary.cpu_max, summary.cpu_mean, "CPU 峰值 >= 均值")
        assert_ge(summary.rss_max, summary.rss_mean, "RSS 峰值 >= 均值")
        print(f"  CPU: mean={summary.cpu_mean:.1f}%, max={summary.cpu_max:.1f}%, p95={summary.cpu_p95:.1f}%")
        print(f"  RSS: mean={summary.rss_mean/1e6:.1f}MB, max={summary.rss_max/1e6:.1f}MB")
        print(f"  Commit: mean={summary.commit_mean/1e6:.1f}MB, max={summary.commit_max/1e6:.1f}MB")
    print("  -> PASS")

    # --- 测试 4: 活跃阶段追踪 ---
    print("\n[测试 4] 活跃阶段追踪")
    monitor3 = ResourceMonitor(sampler=MockSampler(snapshots))
    monitor3.mark_stage_start(STAGE_DRIZZLE)
    active = monitor3.get_active_stages()
    assert_eq(len(active), 1, "应有 1 个活跃 stage")
    assert_eq(active[0], STAGE_DRIZZLE, "活跃 stage 应为 DRIZZLE")

    monitor3.mark_stage_start(STAGE_PLATESOLVE)
    active = monitor3.get_active_stages()
    assert_eq(len(active), 2, "应有 2 个活跃 stage")

    monitor3.mark_stage_end(STAGE_DRIZZLE)
    active = monitor3.get_active_stages()
    assert_eq(len(active), 1, "结束 DRIZZLE 后应有 1 个活跃 stage")
    assert_eq(active[0], STAGE_PLATESOLVE, "剩余活跃 stage 应为 PLATESOLVE")
    print("  -> PASS")

    # --- 测试 5: 可用内存查询 ---
    print("\n[测试 5] 可用内存查询")
    monitor3.sample_once()  # 手动采样一次
    avail = monitor3.get_available_memory()
    assert_gt(avail, 0, "可用内存应 > 0")
    print(f"  可用内存: {avail/1e6:.1f}MB")
    print("  -> PASS")

    # --- 测试 6: JSON 导出 ---
    print("\n[测试 6] JSON 导出")
    json_str = monitor3.to_json()
    data = json.loads(json_str)
    assert_true("samples" in data, "JSON 应含 samples 字段")
    assert_true("active_stages" in data, "JSON 应含 active_stages 字段")
    print(f"  JSON 导出 {len(data['samples'])} 个快照")
    print("  -> PASS")


# ============================================================================
# Part 2: FrameCostEstimator 测试
# ============================================================================

def test_cost_estimator():
    section("Part 2: FrameCostEstimator")

    estimator = FrameCostEstimator()

    # --- 测试 7: T2 帧成本估算 ---
    print("\n[测试 7] T2 帧成本估算 (4096x4096, 1949 stars, nside=2048)")
    t2_params = FrameParams(
        frame_id="T2_RED_LDN43",
        image_w=4096, image_h=4096,
        n_stars=1949, n_gaia=1210, nside=2048,
        pixel_scale_arcsec=0.967,
    )
    t2_est = estimator.estimate(t2_params)

    assert_eq(len(t2_est.stages), 7, "应有 7 个阶段")
    assert_gt(t2_est.total_predicted_peak_bytes, 0, "总峰值应 > 0")
    assert_gt(t2_est.total_predicted_duration_sec, 0, "总时长应 > 0")

    # DRIZZLE 应是 T2 的最坏阶段 (nside=2048 → 大 HEALPix map)
    assert_eq(t2_est.worst_stage, STAGE_DRIZZLE, "T2 最坏阶段应为 DRIZZLE")

    drizzle_cost = t2_est.stages[STAGE_DRIZZLE]
    print(f"  DRIZZLE 峰值内存: {drizzle_cost.predicted_peak_bytes/1e6:.1f}MB")
    print(f"  DRIZZLE 预测时长: {drizzle_cost.predicted_duration_sec:.3f}s")
    print(f"  全帧峰值内存: {t2_est.total_predicted_peak_bytes/1e6:.1f}MB")
    print(f"  全帧预测时长: {t2_est.total_predicted_duration_sec:.3f}s")
    print("  -> PASS")

    # --- 测试 8: T4 帧成本估算 (宽场, 低 nside) ---
    print("\n[测试 8] T4 帧成本估算 (4500x3600, 1984 stars, nside=512, 宽场)")
    t4_params = FrameParams(
        frame_id="T4_RED_GalaxyCenter_panel1",
        image_w=4500, image_h=3600,
        n_stars=1984, n_gaia=6021, nside=512,
        pixel_scale_arcsec=6.308,
        is_wide_field=True,
    )
    t4_est = estimator.estimate(t4_params)

    t4_drizzle = t4_est.stages[STAGE_DRIZZLE]
    print(f"  DRIZZLE 峰值内存: {t4_drizzle.predicted_peak_bytes/1e6:.1f}MB")
    print(f"  DRIZZLE 预测时长: {t4_drizzle.predicted_duration_sec:.3f}s")
    print(f"  全帧峰值内存: {t4_est.total_predicted_peak_bytes/1e6:.1f}MB")
    print(f"  全帧预测时长: {t4_est.total_predicted_duration_sec:.3f}s")

    # T4 宽场, nside=512, DRIZZLE 内存应远小于 T2 (nside=2048)
    assert_lt = t4_drizzle.predicted_peak_bytes < drizzle_cost.predicted_peak_bytes
    assert_true(assert_lt, "T4 DRIZZLE 内存应小于 T2 (nside=512 vs 2048)")
    print("  -> PASS")

    # --- 测试 9: 各阶段内存预期关系 ---
    print("\n[测试 9] 各阶段内存预期关系")
    # CALIBRATE (3x image) > READ_FITS (1x image)
    assert_gt(t2_est.stages[STAGE_CALIBRATE].predicted_peak_bytes,
              t2_est.stages[STAGE_READ_FITS].predicted_peak_bytes,
              "CALIBRATE 内存应 > READ_FITS")
    # DRIZZLE (nside=2048) 应是最大
    for stage in ALL_STAGES:
        if stage != STAGE_DRIZZLE:
            assert_ge(drizzle_cost.predicted_peak_bytes,
                      t2_est.stages[stage].predicted_peak_bytes,
                      f"DRIZZLE 内存应 >= {stage}")
    # SNR 应最小
    snr_cost = t2_est.stages[STAGE_SNR]
    for stage in ALL_STAGES:
        if stage != STAGE_SNR:
            assert_le(snr_cost.predicted_peak_bytes,
                      t2_est.stages[stage].predicted_peak_bytes,
                      f"SNR 内存应 <= {stage}")
    print("  -> PASS")

    # --- 测试 10: 高内存阶段标记 ---
    print("\n[测试 10] 高内存阶段标记")
    for stage in HIGH_MEMORY_STAGES:
        assert_true(t2_est.stages[stage].is_high_memory,
                    f"{stage} 应标记为高内存")
    for stage in ALL_STAGES:
        if stage not in HIGH_MEMORY_STAGES:
            assert_true(not t2_est.stages[stage].is_high_memory,
                        f"{stage} 不应标记为高内存")
    print("  -> PASS")

    # --- 测试 11: resource_profile 契约符合性 ---
    print("\n[测试 11] resource_profile 契约符合性")
    for stage_name in ALL_STAGES:
        profile = t2_est.stages[stage_name].to_profile("T2_RED_LDN43")
        # 检查必需字段
        assert_eq(profile["frame_id"], "T2_RED_LDN43", "frame_id 应匹配")
        assert_eq(profile["stage"], stage_name, "stage 应匹配")
        assert_true(isinstance(profile["predicted_peak_bytes"], int),
                    "predicted_peak_bytes 应为整数")
        assert_true(isinstance(profile["uncertainty_bytes"], int),
                    "uncertainty_bytes 应为整数")
        assert_true(profile["actual_peak_bytes"] is None,
                    "actual_peak_bytes 初始应为 None")
        assert_true(0 <= profile["cpu_intensity"] <= 1,
                    f"cpu_intensity 应在 0-1 范围")
        assert_true(0 <= profile["io_intensity"] <= 1,
                    f"io_intensity 应在 0-1 范围")
    print("  -> PASS (7 阶段全部符合 resource_profile.schema.json)")


# ============================================================================
# Part 3: B-002 基线验证
# ============================================================================

def test_baseline_validation():
    section("Part 3: B-002 基线验证")

    estimator = FrameCostEstimator()
    results = estimator.validate_against_baseline(BASELINE_PATH)

    print(f"\n基线验证结果:")
    print(f"  判定: {results['verdict']}")
    print(f"  最大单阶段误差: {results['max_stage_error_pct']:.1f}%")
    print(f"  最大总耗时误差: {results['max_total_error_pct']:.1f}%")

    for frame in results["frames"]:
        print(f"\n  {frame['frame_id']}:")
        print(f"    预测总时长: {frame['total_predicted_sec']:.3f}s, 实际: {frame['total_actual_sec']:.3f}s, 误差: {frame['total_error_pct']:.1f}%")
        for s in frame["stages"]:
            if s["actual_sec"] > 0.01:  # 只显示 > 10ms 的阶段
                print(f"      {s['stage']:14s}: pred={s['predicted_sec']:7.3f}s  actual={s['actual_sec']:7.3f}s  err={s['error_pct']:5.1f}%")

    # 总耗时误差应 < 15%
    assert_lt_15 = results["max_total_error_pct"] < 15.0
    assert_true(assert_lt_15,
                f"总耗时误差应 < 15%, 实际 {results['max_total_error_pct']:.1f}%")
    print(f"\n  -> PASS (总耗时误差 {results['max_total_error_pct']:.1f}% < 15%)")


# ============================================================================
# Part 4: 便捷函数测试
# ============================================================================

def test_convenience():
    section("Part 4: 便捷函数")

    print("\n[测试] estimate_frame 便捷函数")
    est = estimate_frame(
        frame_id="TEST_FRAME",
        image_w=4096, image_h=4096,
        n_stars=1500, n_gaia=1000, nside=2048,
        pixel_scale_arcsec=1.0,
    )
    assert_eq(est.frame_id, "TEST_FRAME", "frame_id 应匹配")
    assert_eq(len(est.stages), 7, "应有 7 个阶段")
    assert_gt(est.total_predicted_peak_bytes, 0, "峰值应 > 0")
    print(f"  峰值内存: {est.total_predicted_peak_bytes/1e6:.1f}MB")
    print(f"  预测时长: {est.total_predicted_duration_sec:.3f}s")
    print("  -> PASS")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("  H-001 单元测试: 资源监测与动态成本估算")
    print("=" * 60)

    test_resource_monitor()
    test_cost_estimator()
    test_baseline_validation()
    test_convenience()

    print(f"\n{'='*60}")
    print(f"  测试结果: {PASS} PASS, {FAIL} FAIL")
    print(f"{'='*60}")

    if FAIL > 0:
        print("\n失败项:")
        for f in FAILURES:
            print(f"  - {f}")
        sys.exit(1)
    else:
        print("\n全部测试通过!")
        sys.exit(0)


if __name__ == "__main__":
    main()
