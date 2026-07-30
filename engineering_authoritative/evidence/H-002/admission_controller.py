"""
H-002 内存预约、CPU回滞和准入控制

规范来源: engineering_authoritative/docs/04_RESOURCE_AWARE_ORCHESTRATOR_SPEC.md
  MemoryBudgetManager: 预约、释放、安全余量和误差系数
  AdmissionController: CPU回滞、内存门限、阶段兼容矩阵
  准入公式: reserved + predicted_peak + uncertainty + OS_margin + worst_next_frame <= budget
  压力处理: 停止准入 → 等待释放 → 清理可重建缓存 → 暂停 → 显式spill → 恢复 → 最后才允许OS swap

依赖: H-001 的 ResourceMonitor + FrameCostEstimator
"""

from __future__ import annotations

import threading
import time
import json
import os
from dataclasses import dataclass, field, asdict
from typing import Optional, List, Dict, Tuple
from enum import Enum

# 导入 H-001 组件
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "H-001"))

from resource_monitor import ResourceMonitor
from cost_estimator import (
    FrameCostEstimator, FrameParams, StageCost,
    ALL_STAGES, HIGH_MEMORY_STAGES, HIGH_CPU_STAGES,
    STAGE_READ_FITS, STAGE_CALIBRATE, STAGE_PLATESOLVE,
    STAGE_PSF, STAGE_PHOTOMETRIC, STAGE_SNR, STAGE_DRIZZLE,
)


# ============================================================================
# 准入决策结果
# ============================================================================

class AdmissionDecision(Enum):
    ADMIT = "ADMIT"            # 准入
    DEFER = "DEFER"            # 推迟 (资源不足, 等待释放)
    REJECT = "REJECT"          # 拒绝 (参数无效或预算超限)


@dataclass
class AdmissionResult:
    """准入控制决策结果"""
    decision: AdmissionDecision
    reason: str
    frame_id: str
    stage: str
    budget_bytes: int                # 总预算
    reserved_bytes: int              # 已预约
    predicted_peak_bytes: int        # 预测峰值
    uncertainty_bytes: int           # 不确定度
    os_margin_bytes: int             # OS 安全余量
    worst_next_frame_bytes: int      # 最坏下一帧
    total_required_bytes: int        # 总需求 = reserved + predicted + uncertainty + os_margin + worst_next
    available_bytes: int             # 可用 = budget - reserved
    cpu_load: float                  # 当前 CPU 负载
    max_concurrent: int              # 当前允许的最大并发

    def to_dict(self) -> dict:
        d = asdict(self)
        d["decision"] = self.decision.value
        return d


# ============================================================================
# 内存预约记录
# ============================================================================

@dataclass
class MemoryReservation:
    """单次内存预约记录"""
    frame_id: str
    stage: str
    reserved_bytes: int
    timestamp_sec: float
    # 实际使用峰值 (stage 完成后填入, 供模型校准)
    actual_peak_bytes: Optional[int] = None


# ============================================================================
# MemoryBudgetManager 内存预算管理器
# ============================================================================

class MemoryBudgetManager:
    """
    内存预算管理器: 预约、释放、安全余量。

    核心规则:
      reserved + predicted_peak + uncertainty + OS_margin + worst_next_frame <= budget

    设计要点:
      - 不依赖 OS swap, 显式预约
      - 为高内存阶段 (PLATESOLVE/DRIZZLE) 强制预约
      - 线程安全
    """

    def __init__(
        self,
        total_budget_bytes: int,
        os_margin_bytes: int = 2 * 1024 * 1024 * 1024,  # 默认 OS 安全余量 2GB
        uncertainty_factor: float = 1.0,  # 不确定度放大因子
    ):
        self._total_budget = total_budget_bytes
        self._os_margin = os_margin_bytes
        self._uncertainty_factor = uncertainty_factor
        self._reservations: Dict[str, MemoryReservation] = {}  # key: frame_id
        self._lock = threading.RLock()

    @property
    def total_budget(self) -> int:
        return self._total_budget

    @property
    def os_margin(self) -> int:
        return self._os_margin

    def get_reserved(self) -> int:
        """获取当前已预约内存"""
        with self._lock:
            return sum(r.reserved_bytes for r in self._reservations.values())

    def get_reservation(self, frame_id: str) -> Optional[MemoryReservation]:
        with self._lock:
            return self._reservations.get(frame_id)

    def get_active_reservations(self) -> List[MemoryReservation]:
        with self._lock:
            return list(self._reservations.values())

    def reserve(self, frame_id: str, stage: str, bytes_to_reserve: int) -> bool:
        """
        为某帧某阶段预约内存。

        返回: True 预约成功; False 预约失败 (超出预算)
        注意: 此方法仅记录预约, 不检查预算. 预算检查由 AdmissionController 完成.
        """
        with self._lock:
            if frame_id in self._reservations:
                # 更新已有预约 (stage 推进)
                self._reservations[frame_id].stage = stage
                self._reservations[frame_id].reserved_bytes = bytes_to_reserve
                self._reservations[frame_id].timestamp_sec = time.time()
            else:
                self._reservations[frame_id] = MemoryReservation(
                    frame_id=frame_id,
                    stage=stage,
                    reserved_bytes=bytes_to_reserve,
                    timestamp_sec=time.time(),
                )
        return True

    def release(self, frame_id: str) -> Optional[MemoryReservation]:
        """释放某帧的内存预约"""
        with self._lock:
            return self._reservations.pop(frame_id, None)

    def set_actual_peak(self, frame_id: str, actual_bytes: int):
        """记录实际峰值 (供模型校准)"""
        with self._lock:
            if frame_id in self._reservations:
                self._reservations[frame_id].actual_peak_bytes = actual_bytes

    def can_allocate(
        self,
        predicted_peak: int,
        uncertainty: int,
        worst_next_frame: int,
    ) -> Tuple[bool, int, int]:
        """
        检查是否能分配内存 (不实际预约).

        公式: reserved + predicted_peak + uncertainty*factor + os_margin + worst_next <= budget

        返回: (can_allocate, total_required, available)
        """
        with self._lock:
            reserved = self.get_reserved()
            adjusted_uncertainty = int(uncertainty * self._uncertainty_factor)
            total_required = (reserved + predicted_peak + adjusted_uncertainty
                              + self._os_margin + worst_next_frame)
            available = self._total_budget - reserved
            return (total_required <= self._total_budget, total_required, available)

    def get_summary(self) -> dict:
        with self._lock:
            return {
                "total_budget_bytes": self._total_budget,
                "os_margin_bytes": self._os_margin,
                "reserved_bytes": self.get_reserved(),
                "available_bytes": self._total_budget - self.get_reserved(),
                "n_reservations": len(self._reservations),
                "reservations": [asdict(r) for r in self._reservations.values()],
            }


# ============================================================================
# CPUBackpressure CPU回滞控制器
# ============================================================================

class CPUBackpressure:
    """
    CPU 回滞控制器: 系统负载高时自动降低并发度。

    策略:
      - CPU 负载 > 90% → 停止投喂 (max_concurrent = 0 或 1)
      - CPU 负载 70-90% → 限制并发 (max_concurrent = floor(max * (1 - (load-70)/100)))
      - CPU 负载 < 70% → 全速 (max_concurrent = configured_max)
      - 滚动 10s 平均, 避免瞬时抖动
    """

    LOAD_HIGH_THRESHOLD = 90.0    # 停止投喂阈值
    LOAD_THROTTLE_THRESHOLD = 70.0  # 开始回滞阈值
    LOAD_FULL_SPEED_THRESHOLD = 60.0  # 全速阈值

    def __init__(self, max_concurrent: int = 2, monitor: Optional[ResourceMonitor] = None):
        self._configured_max = max(1, max_concurrent)
        self._monitor = monitor
        self._lock = threading.Lock()
        self._load_history: List[float] = []
        self._history_max = 20  # 保留最近 20 次采样

    def set_monitor(self, monitor: ResourceMonitor):
        self._monitor = monitor

    def update_load(self, cpu_percent: float):
        """手动更新 CPU 负载 (无 monitor 时使用)"""
        with self._lock:
            self._load_history.append(cpu_percent)
            if len(self._load_history) > self._history_max:
                self._load_history.pop(0)

    def _get_current_load(self) -> float:
        """获取当前 CPU 负载 (滚动平均)"""
        with self._lock:
            if self._load_history:
                return sum(self._load_history) / len(self._load_history)
        # 从 monitor 获取
        if self._monitor:
            return self._monitor.get_cpu_load()
        return 0.0

    def get_max_concurrent(self) -> int:
        """
        获取当前允许的最大并发数.

        返回 0 表示停止投喂 (负载过高).
        """
        load = self._get_current_load()

        if load >= self.LOAD_HIGH_THRESHOLD:
            # 停止投喂 (但允许 1 个正在运行的完成)
            return 1
        elif load >= self.LOAD_THROTTLE_THRESHOLD:
            # 线性回滞: 90%→1, 70%→max
            ratio = (self.LOAD_HIGH_THRESHOLD - load) / (self.LOAD_HIGH_THRESHOLD - self.LOAD_THROTTLE_THRESHOLD)
            return max(1, int(self._configured_max * ratio))
        else:
            # 全速
            return self._configured_max

    def is_throttled(self) -> bool:
        """是否正在回滞"""
        return self._get_current_load() >= self.LOAD_THROTTLE_THRESHOLD

    def is_feeding_stopped(self) -> bool:
        """是否停止投喂"""
        return self._get_current_load() >= self.LOAD_HIGH_THRESHOLD

    def get_status(self) -> dict:
        load = self._get_current_load()
        return {
            "cpu_load": load,
            "max_concurrent": self.get_max_concurrent(),
            "configured_max": self._configured_max,
            "is_throttled": self.is_throttled(),
            "feeding_stopped": self.is_feeding_stopped(),
        }


# ============================================================================
# 阶段兼容矩阵 (哪些阶段可以并发执行)
# ============================================================================

# True = 两阶段可并发, False = 互斥
# 策略: 高内存阶段互斥; CPU 密集阶段尽量不并发; I/O 密集可与 CPU 密集并发
STAGE_COMPATIBILITY: Dict[str, set] = {
    STAGE_READ_FITS:    {STAGE_CALIBRATE, STAGE_PSF},  # I/O 密集, 可与 CPU 密集并发
    STAGE_CALIBRATE:    {STAGE_READ_FITS, STAGE_SNR},  # CPU 密集, 可与 I/O 并发
    STAGE_PLATESOLVE:   set(),                           # 高内存+高CPU, 独占
    STAGE_PSF:          {STAGE_READ_FITS, STAGE_SNR},   # CPU 中等
    STAGE_PHOTOMETRIC:  {STAGE_SNR},                     # 中等
    STAGE_SNR:          {STAGE_READ_FITS, STAGE_CALIBRATE, STAGE_PSF, STAGE_PHOTOMETRIC},
    STAGE_DRIZZLE:      set(),                           # 高内存+高CPU+高IO, 独占
}


def stages_compatible(stage_a: str, stage_b: str) -> bool:
    """检查两个阶段是否兼容 (可并发执行)"""
    if stage_a == stage_b:
        return False  # 同阶段不同帧不可并发 (避免资源竞争)
    return stage_b in STAGE_COMPATIBILITY.get(stage_a, set())


# ============================================================================
# AdmissionController 准入控制器
# ============================================================================

class AdmissionController:
    """
    准入控制器: 新任务提交前检查资源是否足够。

    准入公式 (规范 §准入):
      reserved + predicted_peak + uncertainty + OS_margin + worst_next_frame <= budget

    压力处理链 (规范 §压力处理):
      停止准入 → 等待释放 → 清理可重建缓存 → 暂停 → 显式spill → 恢复 → 最后才允许OS swap
      不得丢弃未持久化科学数据。
    """

    def __init__(
        self,
        budget_manager: MemoryBudgetManager,
        cost_estimator: FrameCostEstimator,
        cpu_backpressure: CPUBackpressure,
        monitor: Optional[ResourceMonitor] = None,
    ):
        self._budget = budget_manager
        self._estimator = cost_estimator
        self._cpu_bp = cpu_backpressure
        self._monitor = monitor
        self._lock = threading.RLock()

    def admit(
        self,
        frame_params: FrameParams,
        stage: str = STAGE_DRIZZLE,
        worst_next_frame_params: Optional[FrameParams] = None,
    ) -> AdmissionResult:
        """
        准入决策: 检查帧在某阶段是否可以准入。

        参数:
          frame_params: 帧参数 (用于成本预测)
          stage: 要准入的阶段 (默认 DRIZZLE, 最严格)
          worst_next_frame_params: 最坏下一帧参数 (None=用当前帧参数)

        返回: AdmissionResult
        """
        with self._lock:
            # 1. 成本预测
            cost = self._estimator.estimate(frame_params)
            if stage not in cost.stages:
                return AdmissionResult(
                    decision=AdmissionDecision.REJECT,
                    reason=f"Unknown stage: {stage}",
                    frame_id=frame_params.frame_id, stage=stage,
                    budget_bytes=self._budget.total_budget,
                    reserved_bytes=self._budget.get_reserved(),
                    predicted_peak_bytes=0, uncertainty_bytes=0,
                    os_margin_bytes=self._budget.os_margin,
                    worst_next_frame_bytes=0, total_required_bytes=0,
                    available_bytes=0, cpu_load=0, max_concurrent=0,
                )

            stage_cost = cost.stages[stage]

            # 2. 最坏下一帧内存
            if worst_next_frame_params is None:
                worst_next = self._estimator.estimate_worst_next_frame(frame_params)
            else:
                worst_next = self._estimator.estimate_worst_next_frame(worst_next_frame_params)

            # 3. CPU 回滞检查
            cpu_load = self._cpu_bp._get_current_load()
            max_concurrent = self._cpu_bp.get_max_concurrent()

            if self._cpu_bp.is_feeding_stopped():
                return AdmissionResult(
                    decision=AdmissionDecision.DEFER,
                    reason=f"CPU load {cpu_load:.1f}% >= {CPUBackpressure.LOAD_HIGH_THRESHOLD}%, feeding stopped",
                    frame_id=frame_params.frame_id, stage=stage,
                    budget_bytes=self._budget.total_budget,
                    reserved_bytes=self._budget.get_reserved(),
                    predicted_peak_bytes=stage_cost.predicted_peak_bytes,
                    uncertainty_bytes=stage_cost.uncertainty_bytes,
                    os_margin_bytes=self._budget.os_margin,
                    worst_next_frame_bytes=worst_next,
                    total_required_bytes=0, available_bytes=0,
                    cpu_load=cpu_load, max_concurrent=max_concurrent,
                )

            # 4. 内存预算检查 (核心准入公式)
            can_alloc, total_req, available = self._budget.can_allocate(
                predicted_peak=stage_cost.predicted_peak_bytes,
                uncertainty=stage_cost.uncertainty_bytes,
                worst_next_frame=worst_next,
            )

            if not can_alloc:
                return AdmissionResult(
                    decision=AdmissionDecision.DEFER,
                    reason=(f"Memory budget exceeded: "
                            f"required {total_req/1e9:.2f}GB > budget {self._budget.total_budget/1e9:.2f}GB "
                            f"(reserved={self._budget.get_reserved()/1e9:.2f}GB, "
                            f"peak={stage_cost.predicted_peak_bytes/1e9:.2f}GB, "
                            f"unc={stage_cost.uncertainty_bytes/1e9:.2f}GB, "
                            f"os_margin={self._budget.os_margin/1e9:.2f}GB, "
                            f"worst_next={worst_next/1e9:.2f}GB)"),
                    frame_id=frame_params.frame_id, stage=stage,
                    budget_bytes=self._budget.total_budget,
                    reserved_bytes=self._budget.get_reserved(),
                    predicted_peak_bytes=stage_cost.predicted_peak_bytes,
                    uncertainty_bytes=stage_cost.uncertainty_bytes,
                    os_margin_bytes=self._budget.os_margin,
                    worst_next_frame_bytes=worst_next,
                    total_required_bytes=total_req,
                    available_bytes=available,
                    cpu_load=cpu_load, max_concurrent=max_concurrent,
                )

            # 5. 阶段兼容性检查 (与当前活跃阶段)
            if self._monitor:
                active_stages = self._monitor.get_active_stages()
                for active_stage in active_stages:
                    if not stages_compatible(stage, active_stage):
                        return AdmissionResult(
                            decision=AdmissionDecision.DEFER,
                            reason=f"Stage {stage} incompatible with active stage {active_stage}",
                            frame_id=frame_params.frame_id, stage=stage,
                            budget_bytes=self._budget.total_budget,
                            reserved_bytes=self._budget.get_reserved(),
                            predicted_peak_bytes=stage_cost.predicted_peak_bytes,
                            uncertainty_bytes=stage_cost.uncertainty_bytes,
                            os_margin_bytes=self._budget.os_margin,
                            worst_next_frame_bytes=worst_next,
                            total_required_bytes=total_req,
                            available_bytes=available,
                            cpu_load=cpu_load, max_concurrent=max_concurrent,
                        )

            # 6. 准入通过 — 预约内存
            self._budget.reserve(
                frame_id=frame_params.frame_id,
                stage=stage,
                bytes_to_reserve=stage_cost.predicted_peak_bytes + stage_cost.uncertainty_bytes,
            )

            return AdmissionResult(
                decision=AdmissionDecision.ADMIT,
                reason="All checks passed",
                frame_id=frame_params.frame_id, stage=stage,
                budget_bytes=self._budget.total_budget,
                reserved_bytes=self._budget.get_reserved(),
                predicted_peak_bytes=stage_cost.predicted_peak_bytes,
                uncertainty_bytes=stage_cost.uncertainty_bytes,
                os_margin_bytes=self._budget.os_margin,
                worst_next_frame_bytes=worst_next,
                total_required_bytes=total_req,
                available_bytes=available,
                cpu_load=cpu_load, max_concurrent=max_concurrent,
            )

    def release(self, frame_id: str, actual_peak_bytes: Optional[int] = None):
        """释放帧的内存预约"""
        with self._lock:
            if actual_peak_bytes is not None:
                self._budget.set_actual_peak(frame_id, actual_peak_bytes)
            return self._budget.release(frame_id)

    def get_status(self) -> dict:
        """获取准入控制器状态"""
        return {
            "budget": self._budget.get_summary(),
            "cpu_backpressure": self._cpu_bp.get_status(),
            "active_stages": self._monitor.get_active_stages() if self._monitor else [],
        }


# ============================================================================
# 压力处理状态机 (规范 §压力处理)
# ============================================================================

class PressureLevel(Enum):
    """压力等级 (递增)"""
    NORMAL = 0          # 正常运行
    THROTTLE = 1        # CPU 回滞 (降低并发)
    STOP_ADMISSION = 2  # 停止准入
    WAIT_RELEASE = 3    # 等待内存释放
    CLEAR_CACHE = 4     # 清理可重建缓存
    PAUSE = 5           # 暂停管线
    SPILL = 6           # 显式 spill (H-003)
    OS_SWAP = 7         # 最后手段: 允许 OS swap (应避免)


class PressureHandler:
    """
    压力处理器: 根据资源压力等级执行递进式降级。

    规范 §压力处理:
      停止准入 → 等待释放 → 清理可重建缓存 → 暂停 → 显式spill → 恢复 → 最后才允许OS swap
      不得丢弃未持久化科学数据。
    """

    def __init__(self, admission_controller: AdmissionController):
        self._controller = admission_controller
        self._level = PressureLevel.NORMAL
        self._lock = threading.Lock()

    def assess(self) -> PressureLevel:
        """评估当前压力等级"""
        with self._lock:
            cpu_status = self._controller._cpu_bp.get_status()
            budget_summary = self._controller._budget.get_summary()

            available_ratio = budget_summary["available_bytes"] / max(1, budget_summary["total_budget_bytes"])

            if cpu_status["feeding_stopped"] or available_ratio < 0.05:
                self._level = PressureLevel.STOP_ADMISSION
            elif cpu_status["is_throttled"] or available_ratio < 0.15:
                self._level = PressureLevel.THROTTLE
            else:
                self._level = PressureLevel.NORMAL

            return self._level

    def get_level(self) -> PressureLevel:
        with self._lock:
            return self._level

    def get_action(self) -> str:
        """获取当前压力等级对应的动作"""
        level = self.assess()
        actions = {
            PressureLevel.NORMAL: "正常运行, 全速投喂",
            PressureLevel.THROTTLE: "CPU回滞, 降低并发度",
            PressureLevel.STOP_ADMISSION: "停止准入, 等待在运行任务完成",
            PressureLevel.WAIT_RELEASE: "等待内存释放 (正在运行阶段完成)",
            PressureLevel.CLEAR_CACHE: "清理可重建缓存 (非科学数据)",
            PressureLevel.PAUSE: "暂停管线 (保留已持久化数据)",
            PressureLevel.SPILL: "显式spill中间结果到磁盘 (H-003)",
            PressureLevel.OS_SWAP: "最后手段: 允许OS swap (应避免)",
        }
        return actions.get(level, "未知")
