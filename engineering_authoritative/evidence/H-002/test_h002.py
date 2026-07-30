"""
H-002 单元测试

测试内容:
1. MemoryBudgetManager: 预约/释放/预算检查/安全余量
2. CPUBackpressure: 负载阈值/并发度计算/回滞判断
3. AdmissionController: 准入决策(ADMIT/DEFER/REJECT)/阶段兼容/释放
4. PressureHandler: 压力等级评估
5. 准入公式验证: reserved + predicted + uncertainty + os_margin + worst_next <= budget
"""

import sys
import os
import time
import json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "H-001"))

from admission_controller import (
    MemoryBudgetManager, CPUBackpressure, AdmissionController,
    PressureHandler, PressureLevel,
    AdmissionDecision, AdmissionResult,
    STAGE_COMPATIBILITY, stages_compatible,
)
from resource_monitor import ResourceMonitor, MockSampler, ResourceSnapshot
from cost_estimator import (
    FrameCostEstimator, FrameParams,
    STAGE_READ_FITS, STAGE_CALIBRATE, STAGE_PLATESOLVE,
    STAGE_PSF, STAGE_PHOTOMETRIC, STAGE_SNR, STAGE_DRIZZLE,
    ALL_STAGES, HIGH_MEMORY_STAGES,
)

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


def section(name):
    print(f"\n{'='*60}")
    print(f"  {name}")
    print(f"{'='*60}")


# ============================================================================
# Part 1: MemoryBudgetManager 测试
# ============================================================================

def test_memory_budget():
    section("Part 1: MemoryBudgetManager")

    # --- 测试 1: 基本预约与释放 ---
    print("\n[测试 1] 基本预约与释放")
    mgr = MemoryBudgetManager(total_budget_bytes=8 * 1024**3, os_margin_bytes=1 * 1024**3)
    assert_eq(mgr.get_reserved(), 0, "初始预约应为 0")

    mgr.reserve("frame1", STAGE_DRIZZLE, 500 * 1024**2)  # 500MB
    assert_eq(mgr.get_reserved(), 500 * 1024**2, "预约后应为 500MB")

    mgr.reserve("frame2", STAGE_PLATESOLVE, 300 * 1024**2)  # 300MB
    assert_eq(mgr.get_reserved(), 800 * 1024**2, "两帧预约后应为 800MB")

    released = mgr.release("frame1")
    assert_true(released is not None, "释放应返回预约记录")
    assert_eq(mgr.get_reserved(), 300 * 1024**2, "释放 frame1 后应为 300MB")
    print(f"  预约/释放正常, 当前预约: {mgr.get_reserved()/1024**2:.0f}MB")
    print("  -> PASS")

    # --- 测试 2: 预算检查 (准入公式) ---
    print("\n[测试 2] 预算检查 (准入公式)")
    mgr2 = MemoryBudgetManager(
        total_budget_bytes=8 * 1024**3,  # 8GB
        os_margin_bytes=2 * 1024**3,      # 2GB
    )
    # 预约 3GB
    mgr2.reserve("frame1", STAGE_DRIZZLE, 3 * 1024**3)

    # 检查: reserved(3G) + peak(2G) + unc(0.4G) + os_margin(2G) + worst_next(1G) = 8.4G > 8G → False
    can, total, avail = mgr2.can_allocate(
        predicted_peak=2 * 1024**3,
        uncertainty=400 * 1024**2,
        worst_next_frame=1 * 1024**3,
    )
    assert_true(not can, "8.4GB > 8GB 应拒绝")
    print(f"  8.4GB > 8GB: can_allocate={can}, total={total/1024**3:.2f}GB")

    # 检查: reserved(3G) + peak(1G) + unc(0.2G) + os_margin(2G) + worst_next(0.5G) = 6.7G < 8G → True
    can, total, avail = mgr2.can_allocate(
        predicted_peak=1 * 1024**3,
        uncertainty=200 * 1024**2,
        worst_next_frame=500 * 1024**2,
    )
    assert_true(can, "6.7GB < 8GB 应允许")
    print(f"  6.7GB < 8GB: can_allocate={can}, total={total/1024**3:.2f}GB")
    print("  -> PASS")

    # --- 测试 3: 安全余量 ---
    print("\n[测试 3] 安全余量")
    assert_eq(mgr2.os_margin, 2 * 1024**3, "OS 安全余量应为 2GB")
    summary = mgr2.get_summary()
    assert_eq(summary["n_reservations"], 1, "应有 1 个预约")
    assert_true("reservations" in summary, "摘要应含预约列表")
    print(f"  OS margin: {mgr2.os_margin/1024**3:.1f}GB")
    print(f"  活跃预约: {summary['n_reservations']}")
    print("  -> PASS")


# ============================================================================
# Part 2: CPUBackpressure 测试
# ============================================================================

def test_cpu_backpressure():
    section("Part 2: CPUBackpressure")

    # --- 测试 4: 低负载全速 ---
    print("\n[测试 4] 低负载全速")
    bp = CPUBackpressure(max_concurrent=4)
    bp.update_load(30.0)
    bp.update_load(40.0)
    bp.update_load(50.0)
    assert_eq(bp.get_max_concurrent(), 4, "负载 40% 应全速 (4)")
    assert_true(not bp.is_throttled(), "40% 不应回滞")
    assert_true(not bp.is_feeding_stopped(), "40% 不应停止投喂")
    print(f"  负载 ~40%: max_concurrent={bp.get_max_concurrent()}, 全速")
    print("  -> PASS")

    # --- 测试 5: 中等负载回滞 ---
    print("\n[测试 5] 中等负载回滞 (70-90%)")
    bp2 = CPUBackpressure(max_concurrent=4)
    bp2.update_load(80.0)
    mc = bp2.get_max_concurrent()
    assert_true(bp2.is_throttled(), "80% 应回滞")
    assert_true(mc < 4, "80% 并发度应 < 4")
    assert_true(mc >= 1, "80% 并发度应 >= 1")
    print(f"  负载 80%: max_concurrent={mc}, 回滞中")
    print("  -> PASS")

    # --- 测试 6: 高负载停止投喂 ---
    print("\n[测试 6] 高负载停止投喂 (>= 90%)")
    bp3 = CPUBackpressure(max_concurrent=4)
    bp3.update_load(95.0)
    assert_true(bp3.is_feeding_stopped(), "95% 应停止投喂")
    assert_eq(bp3.get_max_concurrent(), 1, "95% 并发度应为 1 (允许完成)")
    print(f"  负载 95%: max_concurrent={bp3.get_max_concurrent()}, 停止投喂")
    print("  -> PASS")

    # --- 测试 7: 阈值边界 ---
    print("\n[测试 7] 阈值边界")
    # 每个阈值用独立对象, 避免历史累积
    bp_a = CPUBackpressure(max_concurrent=2)
    bp_a.update_load(89.9)
    assert_true(not bp_a.is_feeding_stopped(), "89.9% 不应停止投喂")

    bp_b = CPUBackpressure(max_concurrent=2)
    bp_b.update_load(90.0)
    assert_true(bp_b.is_feeding_stopped(), "90.0% 应停止投喂")

    bp_c = CPUBackpressure(max_concurrent=2)
    bp_c.update_load(69.9)
    assert_true(not bp_c.is_throttled(), "69.9% 不应回滞")

    bp_d = CPUBackpressure(max_concurrent=2)
    bp_d.update_load(70.0)
    assert_true(bp_d.is_throttled(), "70.0% 应回滞")
    print("  阈值边界正确 (90% 停止, 70% 回滞)")
    print("  -> PASS")


# ============================================================================
# Part 3: AdmissionController 测试
# ============================================================================

def test_admission_controller():
    section("Part 3: AdmissionController")

    estimator = FrameCostEstimator()

    # --- 测试 8: 正常准入 (资源充足) ---
    print("\n[测试 8] 正常准入 (资源充足)")
    budget = MemoryBudgetManager(total_budget_bytes=16 * 1024**3, os_margin_bytes=2 * 1024**3)
    bp = CPUBackpressure(max_concurrent=2)
    bp.update_load(30.0)
    controller = AdmissionController(budget, estimator, bp)

    t2_params = FrameParams(
        frame_id="T2", image_w=4096, image_h=4096,
        n_stars=1949, n_gaia=1210, nside=2048, pixel_scale_arcsec=0.967,
    )
    result = controller.admit(t2_params, stage=STAGE_DRIZZLE)
    assert_eq(result.decision, AdmissionDecision.ADMIT, "16GB 预算应准入 T2 DRIZZLE")
    print(f"  决策: {result.decision.value}")
    print(f"  预测峰值: {result.predicted_peak_bytes/1024**2:.1f}MB")
    print(f"  总需求: {result.total_required_bytes/1024**3:.2f}GB / {result.budget_bytes/1024**3:.0f}GB")
    print("  -> PASS")

    # --- 测试 9: 内存不足推迟 ---
    print("\n[测试 9] 内存不足推迟")
    budget2 = MemoryBudgetManager(total_budget_bytes=1 * 1024**3, os_margin_bytes=256 * 1024**2)  # 1GB 总, 256MB 余量
    bp2 = CPUBackpressure(max_concurrent=2)
    bp2.update_load(30.0)
    controller2 = AdmissionController(budget2, estimator, bp2)

    result2 = controller2.admit(t2_params, stage=STAGE_DRIZZLE)
    assert_eq(result2.decision, AdmissionDecision.DEFER, "1GB 预算应推迟 T2 DRIZZLE (~671MB)")
    print(f"  决策: {result2.decision.value}")
    print(f"  原因: {result2.reason[:80]}...")
    print("  -> PASS")

    # --- 测试 10: CPU 过载推迟 ---
    print("\n[测试 10] CPU 过载推迟")
    budget3 = MemoryBudgetManager(total_budget_bytes=16 * 1024**3, os_margin_bytes=2 * 1024**3)
    bp3 = CPUBackpressure(max_concurrent=2)
    bp3.update_load(95.0)  # CPU 过载
    controller3 = AdmissionController(budget3, estimator, bp3)

    result3 = controller3.admit(t2_params, stage=STAGE_DRIZZLE)
    assert_eq(result3.decision, AdmissionDecision.DEFER, "CPU 95% 应推迟")
    print(f"  决策: {result3.decision.value}")
    print(f"  原因: {result3.reason}")
    print("  -> PASS")

    # --- 测试 11: 释放后重新准入 ---
    print("\n[测试 11] 释放后重新准入")
    released = controller.release("T2")
    assert_true(released is not None, "释放应返回预约记录")
    assert_eq(budget.get_reserved(), 0, "释放后预约应为 0")

    result4 = controller.admit(t2_params, stage=STAGE_DRIZZLE)
    assert_eq(result4.decision, AdmissionDecision.ADMIT, "释放后应可重新准入")
    print(f"  释放后重新准入: {result4.decision.value}")
    print("  -> PASS")

    # --- 测试 12: 阶段兼容性 ---
    print("\n[测试 12] 阶段兼容性")
    assert_true(stages_compatible(STAGE_READ_FITS, STAGE_CALIBRATE), "READ_FITS 与 CALIBRATE 应兼容")
    assert_true(not stages_compatible(STAGE_DRIZZLE, STAGE_PLATESOLVE), "DRIZZLE 与 PLATESOLVE 应互斥")
    assert_true(not stages_compatible(STAGE_DRIZZLE, STAGE_DRIZZLE), "同阶段应互斥")
    assert_true(stages_compatible(STAGE_SNR, STAGE_READ_FITS), "SNR 与 READ_FITS 应兼容")
    print("  阶段兼容矩阵正确")
    print("  -> PASS")

    # --- 测试 13: 多帧并发准入 ---
    print("\n[测试 13] 多帧并发准入")
    budget4 = MemoryBudgetManager(total_budget_bytes=16 * 1024**3, os_margin_bytes=2 * 1024**3)
    bp4 = CPUBackpressure(max_concurrent=2)
    bp4.update_load(30.0)
    controller4 = AdmissionController(budget4, estimator, bp4)

    # T2 DRIZZLE (~671MB peak)
    r1 = controller4.admit(t2_params, stage=STAGE_DRIZZLE)
    assert_eq(r1.decision, AdmissionDecision.ADMIT, "T2 应准入")

    # T4 DRIZZLE (~103MB peak) - 应也能准入 (16GB 预算)
    t4_params = FrameParams(
        frame_id="T4", image_w=4500, image_h=3600,
        n_stars=1984, n_gaia=6021, nside=512, pixel_scale_arcsec=6.308,
        is_wide_field=True,
    )
    r2 = controller4.admit(t4_params, stage=STAGE_DRIZZLE)
    assert_eq(r2.decision, AdmissionDecision.ADMIT, "T4 应准入 (16GB 足够两帧)")

    print(f"  T2 DRIZZLE: {r1.decision.value}, peak={r1.predicted_peak_bytes/1024**2:.0f}MB")
    print(f"  T4 DRIZZLE: {r2.decision.value}, peak={r2.predicted_peak_bytes/1024**2:.0f}MB")
    print(f"  当前预约: {budget4.get_reserved()/1024**2:.0f}MB")

    controller4.release("T2")
    controller4.release("T4")
    assert_eq(budget4.get_reserved(), 0, "全部释放后预约应为 0")
    print("  -> PASS")


# ============================================================================
# Part 4: PressureHandler 测试
# ============================================================================

def test_pressure_handler():
    section("Part 4: PressureHandler")

    estimator = FrameCostEstimator()

    # --- 测试 14: 正常状态 ---
    print("\n[测试 14] 正常状态")
    budget14 = MemoryBudgetManager(total_budget_bytes=8 * 1024**3, os_margin_bytes=2 * 1024**3)
    bp14 = CPUBackpressure(max_concurrent=4)
    bp14.update_load(30.0)
    controller14 = AdmissionController(budget14, estimator, bp14)
    handler14 = PressureHandler(controller14)
    level = handler14.assess()
    assert_eq(level, PressureLevel.NORMAL, "30% CPU + 充足内存应为 NORMAL")
    print(f"  压力等级: {level.name}, 动作: {handler14.get_action()}")
    print("  -> PASS")

    # --- 测试 15: CPU 回滞 ---
    print("\n[测试 15] CPU 回滞")
    budget15 = MemoryBudgetManager(total_budget_bytes=8 * 1024**3, os_margin_bytes=2 * 1024**3)
    bp15 = CPUBackpressure(max_concurrent=4)
    bp15.update_load(80.0)
    controller15 = AdmissionController(budget15, estimator, bp15)
    handler15 = PressureHandler(controller15)
    level = handler15.assess()
    assert_eq(level, PressureLevel.THROTTLE, "80% CPU 应为 THROTTLE")
    print(f"  压力等级: {level.name}, 动作: {handler15.get_action()}")
    print("  -> PASS")

    # --- 测试 16: 停止准入 ---
    print("\n[测试 16] 停止准入")
    budget16 = MemoryBudgetManager(total_budget_bytes=8 * 1024**3, os_margin_bytes=2 * 1024**3)
    bp16 = CPUBackpressure(max_concurrent=4)
    bp16.update_load(95.0)
    controller16 = AdmissionController(budget16, estimator, bp16)
    handler16 = PressureHandler(controller16)
    level = handler16.assess()
    assert_eq(level, PressureLevel.STOP_ADMISSION, "95% CPU 应为 STOP_ADMISSION")
    print(f"  压力等级: {level.name}, 动作: {handler16.get_action()}")
    print("  -> PASS")

    # --- 测试 17: 内存压力 ---
    print("\n[测试 17] 内存压力")
    budget17 = MemoryBudgetManager(total_budget_bytes=8 * 1024**3, os_margin_bytes=2 * 1024**3)
    bp17 = CPUBackpressure(max_concurrent=4)
    bp17.update_load(30.0)
    controller17 = AdmissionController(budget17, estimator, bp17)
    handler17 = PressureHandler(controller17)
    # 预约大量内存 (使 available < 5%: 8GB * 5% = 400MB, 需预约 > 7.6GB)
    budget17.reserve("big_frame", STAGE_DRIZZLE, int(7.7 * 1024**3))  # 7.7GB
    level = handler17.assess()
    assert_eq(level, PressureLevel.STOP_ADMISSION, "available < 5% 应为 STOP_ADMISSION")
    print(f"  压力等级: {level.name}, 动作: {handler17.get_action()}")
    budget17.release("big_frame")
    print("  -> PASS")


# ============================================================================
# Part 5: 准入公式完整性验证
# ============================================================================

def test_admission_formula():
    section("Part 5: 准入公式完整性验证")

    estimator = FrameCostEstimator()

    # --- 测试 18: 公式各分量非负 ---
    print("\n[测试 18] 公式各分量非负")
    budget = MemoryBudgetManager(total_budget_bytes=16 * 1024**3, os_margin_bytes=2 * 1024**3)
    bp = CPUBackpressure(max_concurrent=2)
    bp.update_load(40.0)
    controller = AdmissionController(budget, estimator, bp)

    params = FrameParams(
        frame_id="T2", image_w=4096, image_h=4096,
        n_stars=1949, n_gaia=1210, nside=2048, pixel_scale_arcsec=0.967,
    )
    result = controller.admit(params, stage=STAGE_DRIZZLE)
    assert_eq(result.decision, AdmissionDecision.ADMIT, "应准入")

    # 验证各分量
    assert_ge(result.budget_bytes, 0, "budget >= 0")
    assert_ge(result.reserved_bytes, 0, "reserved >= 0")
    assert_ge(result.predicted_peak_bytes, 0, "predicted_peak >= 0")
    assert_ge(result.uncertainty_bytes, 0, "uncertainty >= 0")
    assert_ge(result.os_margin_bytes, 0, "os_margin >= 0")
    assert_ge(result.worst_next_frame_bytes, 0, "worst_next >= 0")
    assert_ge(result.total_required_bytes, 0, "total_required >= 0")
    assert_ge(result.available_bytes, 0, "available >= 0")

    # 验证公式: total_required = reserved + predicted + uncertainty + os_margin + worst_next
    expected_total = (result.reserved_bytes + result.predicted_peak_bytes
                      + result.uncertainty_bytes + result.os_margin_bytes
                      + result.worst_next_frame_bytes)
    # 注意: reserved 在 admit 时已包含当前帧的预约, 所以 total_required 可能略大于 expected
    # 因为 can_allocate 在 reserve 之前调用, 所以 total_required 不含当前帧的 reserved
    print(f"  budget:      {result.budget_bytes/1024**3:.2f}GB")
    print(f"  reserved:    {result.reserved_bytes/1024**2:.0f}MB")
    print(f"  predicted:   {result.predicted_peak_bytes/1024**2:.0f}MB")
    print(f"  uncertainty: {result.uncertainty_bytes/1024**2:.0f}MB")
    print(f"  os_margin:   {result.os_margin_bytes/1024**3:.2f}GB")
    print(f"  worst_next:  {result.worst_next_frame_bytes/1024**2:.0f}MB")
    print(f"  total_req:   {result.total_required_bytes/1024**3:.2f}GB")
    print(f"  available:   {result.available_bytes/1024**3:.2f}GB")
    print("  -> PASS")

    # --- 测试 19: 不使用固定并发数替代预算 ---
    print("\n[测试 19] 不使用固定并发数替代预算")
    # 即使 max_concurrent=10, 内存不足时仍应 DEFER
    budget2 = MemoryBudgetManager(total_budget_bytes=1 * 1024**3, os_margin_bytes=256 * 1024**2)
    bp2 = CPUBackpressure(max_concurrent=10)  # 高并发
    bp2.update_load(30.0)
    controller2 = AdmissionController(budget2, estimator, bp2)

    result2 = controller2.admit(params, stage=STAGE_DRIZZLE)
    assert_eq(result2.decision, AdmissionDecision.DEFER, "内存不足时即使 max_concurrent=10 也应 DEFER")
    print(f"  max_concurrent=10, 内存不足: {result2.decision.value} (预算优先于并发数)")
    print("  -> PASS")


# ============================================================================
# 主函数
# ============================================================================

def main():
    print("=" * 60)
    print("  H-002 单元测试: 内存预约/CPU回滞/准入控制")
    print("=" * 60)

    test_memory_budget()
    test_cpu_backpressure()
    test_admission_controller()
    test_pressure_handler()
    test_admission_formula()

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
