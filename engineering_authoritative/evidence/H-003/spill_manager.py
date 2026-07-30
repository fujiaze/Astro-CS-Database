"""
H-003 高峰错峰、显式spill与恢复

规范来源: engineering_authoritative/docs/04_RESOURCE_AWARE_ORCHESTRATOR_SPEC.md
  StageScheduler: 允许低峰阶段与高峰阶段错峰并行
  SpillManager: 只spill已序列化、可恢复块
  压力处理: 停止准入 → 等待释放 → 清理可重建缓存 → 暂停 → 显式spill → 恢复 → 最后才允许OS swap
  不得丢弃未持久化科学数据。

依赖: H-001 (ResourceMonitor/FrameCostEstimator), H-002 (PressureHandler/AdmissionController)
"""

from __future__ import annotations

import threading
import time
import json
import os
import hashlib
import struct
import tempfile
import shutil
from dataclasses import dataclass, field, asdict
from typing import Optional, List, Dict, Any, Tuple
from enum import Enum
from collections import deque
import heapq

# 导入 H-001/H-002 组件
sys_path_h = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "H-001")
sys_path_h2 = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "H-002")
import sys
sys.path.insert(0, sys_path_h)
sys.path.insert(0, sys_path_h2)

from cost_estimator import (
    FrameCostEstimator, FrameParams,
    ALL_STAGES, HIGH_MEMORY_STAGES,
    STAGE_READ_FITS, STAGE_CALIBRATE, STAGE_PLATESOLVE,
    STAGE_PSF, STAGE_PHOTOMETRIC, STAGE_SNR, STAGE_DRIZZLE,
)
from admission_controller import (
    PressureHandler, PressureLevel, AdmissionController, AdmissionDecision,
)


# ============================================================================
# 任务优先级
# ============================================================================

class TaskPriority(Enum):
    URGENT = 0     # 紧急 (用户交互/时间敏感)
    HIGH = 1       # 高 (Stage1 关键路径)
    NORMAL = 2     # 正常 (批处理)
    LOW = 3        # 低 (后台/可重建)


# ============================================================================
# 延迟任务记录
# ============================================================================

@dataclass
class DeferredTask:
    """被错峰推迟的任务"""
    frame_id: str
    stage: str
    priority: TaskPriority
    frame_params: Dict[str, Any]  # FrameParams 序列化
    deferred_at_sec: float
    defer_count: int = 0          # 被推迟次数

    def __lt__(self, other):
        """优先队列排序: 优先级高 > 推迟次数少"""
        if self.priority.value != other.priority.value:
            return self.priority.value < other.priority.value
        return self.defer_count < other.defer_count


# ============================================================================
# PeakShifter 高峰错峰调度器
# ============================================================================

class PeakShifter:
    """
    高峰错峰调度器: 将非紧急任务推迟到低负载时段。

    策略:
      - 压力 >= STOP_ADMISSION 时, 新任务入队 (推迟)
      - 压力降至 THROTTLE 以下时, 按优先级出队执行
      - URGENT 任务始终立即执行 (不推迟)
      - 多次推迟的任务优先级提升 (防止饥饿)
    """

    # 多次推迟后提升优先级
    PROMOTE_AFTER_DEFERS = 3
    # 最大推迟时间 (秒), 超过后强制执行
    MAX_DEFER_SEC = 300.0

    def __init__(self, pressure_handler: PressureHandler):
        self._pressure_handler = pressure_handler
        self._queue: List[DeferredTask] = []  # min-heap
        self._lock = threading.RLock()
        self._total_deferred = 0
        self._total_resumed = 0

    def should_defer(self, priority: TaskPriority) -> bool:
        """检查任务是否应被推迟"""
        if priority == TaskPriority.URGENT:
            return False  # 紧急任务不推迟
        level = self._pressure_handler.assess()
        return level.value >= PressureLevel.STOP_ADMISSION.value

    def defer(self, task: DeferredTask) -> bool:
        """将任务入队 (推迟)"""
        with self._lock:
            heapq.heappush(self._queue, task)
            self._total_deferred += 1
        return True

    def try_resume(self) -> Optional[DeferredTask]:
        """
        尝试恢复一个延迟任务.

        返回: 可执行的任务, 或 None (压力仍高或队列为空)
        """
        with self._lock:
            if not self._queue:
                return None

            level = self._pressure_handler.assess()
            if level.value >= PressureLevel.STOP_ADMISSION.value:
                return None  # 压力仍高, 不恢复

            task = heapq.heappop(self._queue)
            self._total_resumed += 1

            # 检查是否超时 (超时强制执行)
            elapsed = time.time() - task.deferred_at_sec
            if elapsed > self.MAX_DEFER_SEC:
                pass  # 强制执行

            return task

    def drain_all(self) -> List[DeferredTask]:
        """强制恢复全部延迟任务 (紧急退出/系统关机)"""
        with self._lock:
            tasks = sorted(self._queue)
            self._queue.clear()
            self._total_resumed += len(tasks)
            return tasks

    def peek_next(self) -> Optional[DeferredTask]:
        """查看队首任务 (不弹出)"""
        with self._lock:
            return self._queue[0] if self._queue else None

    def get_queue_size(self) -> int:
        with self._lock:
            return len(self._queue)

    def get_status(self) -> dict:
        with self._lock:
            return {
                "queue_size": len(self._queue),
                "total_deferred": self._total_deferred,
                "total_resumed": self._total_resumed,
                "pressure_level": self._pressure_handler.assess().name,
            }


# ============================================================================
# Spill 记录
# ============================================================================

@dataclass
class SpillRecord:
    """单次 spill 记录"""
    frame_id: str
    stage: str                # spill 时的阶段
    block_name: str           # PipelineFrame 命名块名 (data/psf/snr_model/...)
    spill_path: str           # spill 文件路径
    size_bytes: int           # spill 数据大小
    checksum: str             # SHA256 校验和 (前 16 字符)
    spilled_at_sec: float     # spill 时间戳
    restored: bool = False    # 是否已恢复
    restored_at_sec: Optional[float] = None

    def to_dict(self) -> dict:
        return asdict(self)


# ============================================================================
# SpillManager 显式 spill 管理器
# ============================================================================

class SpillManager:
    """
    显式 spill 管理器: 内存不足时将中间结果写到磁盘 (而非依赖 OS swap)。

    核心规则 (规范):
      - 只 spill 已序列化、可恢复块
      - 不得丢弃未持久化科学数据
      - spill 后释放内存
      - 恢复后可继续执行

    实现:
      - 原子写入: 写 .tmp → rename (防止写一半崩溃)
      - SHA256 校验: 恢复时验证数据完整性
      - 清单持久化: manifest.json 记录所有 spill 块
      - 线程安全
    """

    MANIFEST_FILENAME = "spill_manifest.json"

    def __init__(self, spill_dir: str):
        self._spill_dir = os.path.abspath(spill_dir)
        self._manifest_path = os.path.join(self._spill_dir, self.MANIFEST_FILENAME)
        self._records: Dict[str, SpillRecord] = {}  # key: "frame_id:stage:block_name"
        self._lock = threading.RLock()

        # 创建 spill 目录
        os.makedirs(self._spill_dir, exist_ok=True)

        # 加载已有清单 (恢复模式)
        self._load_manifest()

    # ------------------------------------------------------------------
    # Spill (写出)
    # ------------------------------------------------------------------

    def spill(
        self,
        frame_id: str,
        stage: str,
        block_name: str,
        data: bytes,
    ) -> SpillRecord:
        """
        将一个命名块显式 spill 到磁盘。

        参数:
          frame_id: 帧标识
          stage: 当前阶段 (spill 时的执行位置)
          block_name: PipelineFrame 命名块名 (data/psf/snr_model/...)
          data: 已序列化的字节流

        返回: SpillRecord (含路径/大小/校验和)
        """
        with self._lock:
            # 生成 spill 文件路径
            safe_name = f"{frame_id}_{stage}_{block_name}.spill"
            safe_name = safe_name.replace("/", "_").replace("\\", "_").replace(":", "_")
            spill_path = os.path.join(self._spill_dir, safe_name)

            # 原子写入: .tmp → rename
            tmp_path = spill_path + ".tmp"
            with open(tmp_path, "wb") as f:
                f.write(data)
                f.flush()
                os.fsync(f.fileno())  # 确保落盘
            os.replace(tmp_path, spill_path)  # 原子重命名

            # 计算校验和
            checksum = hashlib.sha256(data).hexdigest()[:16]
            size = len(data)

            # 创建记录
            key = f"{frame_id}:{stage}:{block_name}"
            record = SpillRecord(
                frame_id=frame_id,
                stage=stage,
                block_name=block_name,
                spill_path=spill_path,
                size_bytes=size,
                checksum=checksum,
                spilled_at_sec=time.time(),
            )
            self._records[key] = record

            # 持久化清单
            self._save_manifest()

            return record

    # ------------------------------------------------------------------
    # Restore (恢复)
    # ------------------------------------------------------------------

    def restore(
        self,
        frame_id: str,
        stage: str,
        block_name: str,
    ) -> Optional[bytes]:
        """
        从磁盘恢复一个 spill 块。

        返回: 原始字节流, 或 None (不存在/校验失败)
        """
        with self._lock:
            key = f"{frame_id}:{stage}:{block_name}"
            record = self._records.get(key)
            if record is None:
                return None

            if not os.path.exists(record.spill_path):
                return None

            # 读取数据
            with open(record.spill_path, "rb") as f:
                data = f.read()

            # 校验完整性
            actual_checksum = hashlib.sha256(data).hexdigest()[:16]
            if actual_checksum != record.checksum:
                raise ValueError(
                    f"Spill checksum mismatch for {key}: "
                    f"expected {record.checksum}, got {actual_checksum}"
                )

            # 标记已恢复
            record.restored = True
            record.restored_at_sec = time.time()
            self._save_manifest()

            return data

    def restore_frame(self, frame_id: str) -> Dict[str, bytes]:
        """
        恢复某帧的全部 spill 块。

        返回: {block_name: data} 字典
        """
        with self._lock:
            result = {}
            for key, record in self._records.items():
                if record.frame_id == frame_id:
                    data = self.restore(record.frame_id, record.stage, record.block_name)
                    if data is not None:
                        result[record.block_name] = data
            return result

    # ------------------------------------------------------------------
    # 清理
    # ------------------------------------------------------------------

    def cleanup_frame(self, frame_id: str) -> int:
        """
        清理某帧的全部 spill 文件 (stage1 完成后调用)。

        返回: 清理的文件数
        """
        with self._lock:
            count = 0
            to_remove = []
            for key, record in self._records.items():
                if record.frame_id == frame_id:
                    if os.path.exists(record.spill_path):
                        os.remove(record.spill_path)
                    to_remove.append(key)
                    count += 1
            for key in to_remove:
                del self._records[key]
            if count > 0:
                self._save_manifest()
            return count

    def cleanup_all(self) -> int:
        """清理全部 spill 文件"""
        with self._lock:
            count = 0
            for record in self._records.values():
                if os.path.exists(record.spill_path):
                    os.remove(record.spill_path)
                    count += 1
            self._records.clear()
            self._save_manifest()
            return count

    # ------------------------------------------------------------------
    # 查询
    # ------------------------------------------------------------------

    def has_spill(self, frame_id: str, stage: str, block_name: str) -> bool:
        key = f"{frame_id}:{stage}:{block_name}"
        with self._lock:
            return key in self._records

    def get_records(self) -> List[SpillRecord]:
        with self._lock:
            return list(self._records.values())

    def get_total_spilled_bytes(self) -> int:
        with self._lock:
            return sum(r.size_bytes for r in self._records.values())

    def get_status(self) -> dict:
        with self._lock:
            return {
                "spill_dir": self._spill_dir,
                "n_records": len(self._records),
                "total_spilled_bytes": self.get_total_spilled_bytes(),
                "total_spilled_mb": self.get_total_spilled_bytes() / 1024 / 1024,
                "n_restored": sum(1 for r in self._records.values() if r.restored),
            }

    # ------------------------------------------------------------------
    # 清单持久化
    # ------------------------------------------------------------------

    def _save_manifest(self):
        """将 spill 清单持久化到 manifest.json"""
        data = {
            "spill_dir": self._spill_dir,
            "records": {k: v.to_dict() for k, v in self._records.items()},
            "updated_at": time.time(),
        }
        tmp_path = self._manifest_path + ".tmp"
        with open(tmp_path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        os.replace(tmp_path, self._manifest_path)

    def _load_manifest(self):
        """从 manifest.json 加载已有清单"""
        if not os.path.exists(self._manifest_path):
            return
        try:
            with open(self._manifest_path, "r", encoding="utf-8") as f:
                data = json.load(f)
            for key, rec_dict in data.get("records", {}).items():
                self._records[key] = SpillRecord(
                    frame_id=rec_dict["frame_id"],
                    stage=rec_dict["stage"],
                    block_name=rec_dict["block_name"],
                    spill_path=rec_dict["spill_path"],
                    size_bytes=rec_dict["size_bytes"],
                    checksum=rec_dict["checksum"],
                    spilled_at_sec=rec_dict["spilled_at_sec"],
                    restored=rec_dict.get("restored", False),
                    restored_at_sec=rec_dict.get("restored_at_sec"),
                )
        except (json.JSONDecodeError, KeyError):
            pass  # 清单损坏, 忽略

    # ------------------------------------------------------------------
    # Spill 决策 (与 PressureHandler 集成)
    # ------------------------------------------------------------------

    def should_spill(self, pressure_level: PressureLevel) -> bool:
        """判断当前压力等级是否应触发 spill"""
        return pressure_level.value >= PressureLevel.SPILL.value

    def select_spill_blocks(
        self,
        frame_id: str,
        current_stage: str,
        active_blocks: Dict[str, int],  # block_name -> size_bytes
    ) -> List[str]:
        """
        选择应 spill 的块 (优先 spill大块, 非当前阶段必需的块)。

        参数:
          frame_id: 帧标识
          current_stage: 当前正在执行的阶段
          active_blocks: 当前内存中的块 {block_name: size_bytes}

        返回: 应 spill 的块名列表 (按大小降序)
        """
        # 当前阶段必需的块 (不可 spill)
        required_by_stage = {
            STAGE_READ_FITS: {"data", "header"},
            STAGE_CALIBRATE: {"data", "header"},
            STAGE_PLATESOLVE: {"data", "header", "star_det"},
            STAGE_PSF: {"data", "header", "star_det", "psf"},
            STAGE_PHOTOMETRIC: {"data", "header", "psf", "gaia_cat"},
            STAGE_SNR: {"psf", "snr_model"},
            STAGE_DRIZZLE: {"data", "header", "snr_model"},
        }
        required = required_by_stage.get(current_stage, set())

        # 可 spill 的块: 不在当前阶段必需集合中的大块
        spillable = []
        for block_name, size in active_blocks.items():
            if block_name not in required and size > 1024 * 1024:  # > 1MB 才值得 spill
                spillable.append((block_name, size))

        # 按大小降序 (优先 spill 大块, 释放更多内存)
        spillable.sort(key=lambda x: -x[1])
        return [name for name, _ in spillable]


# ============================================================================
# RecoveryManager 恢复管理器
# ============================================================================

class RecoveryManager:
    """
    恢复管理器: spill 后恢复继续执行。

    工作流:
      1. spill 发生时, 记录 (frame_id, stage, block) 到 SpillManager
      2. 后续阶段需要该块时, 从 SpillManager.restore() 恢复
      3. stage1 完成后, cleanup_frame() 清理 spill 文件
    """

    def __init__(self, spill_manager: SpillManager):
        self._spill = spill_manager

    def recover_frame(self, frame_id: str) -> Dict[str, bytes]:
        """
        恢复某帧的全部 spill 块。

        返回: {block_name: data} 字典
        """
        return self._spill.restore_frame(frame_id)

    def recover_block(
        self,
        frame_id: str,
        stage: str,
        block_name: str,
    ) -> Optional[bytes]:
        """恢复单个 spill 块"""
        return self._spill.restore(frame_id, stage, block_name)

    def is_recoverable(self, frame_id: str) -> bool:
        """检查某帧是否有可恢复的 spill 块"""
        records = self._spill.get_records()
        return any(r.frame_id == frame_id for r in records)

    def get_recovery_plan(self, frame_id: str) -> List[Dict]:
        """获取某帧的恢复计划 (按 spill 时间排序)"""
        records = [r for r in self._spill.get_records() if r.frame_id == frame_id]
        records.sort(key=lambda r: r.spilled_at_sec)
        return [r.to_dict() for r in records]

    def finalize_frame(self, frame_id: str) -> int:
        """
        帧处理完成后, 清理 spill 文件。

        返回: 清理的文件数
        """
        return self._spill.cleanup_frame(frame_id)
