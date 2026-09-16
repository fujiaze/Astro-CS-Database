"""Windows PDH/ETW 适配 —— 显式未实现隔离 stub（LOG-002）。

Linux 控制节点当前只要求 Linux procfs 路径（真实实现见 linux_procfs.py）。
Windows PDH（性能计数器）与 ETW（事件追踪）采集在 AstroCS 上**尚未实现**，
本模块把该适配路径与 Linux 路径**物理分离**，避免任何人误报"Windows 已支持"。

本文件是**隔离的显式未实现接口**（owner 允许路径内的 known_limits），不是
TODO 代替任务完成：LOG-002 验收明确 Linux procfs 与 Windows PDH/ETW 适配分开，
Windows 侧只保留隔离 stub 或明确未实现接口。

接口形态（未来 FATDUCK/Windows 控制节点实现时填充，保持签名与
`linux_procfs.collect()` / `linux_procfs.is_available()` 对齐）：
  is_available() -> bool     恒 False（非 Windows 宿主或未实现）
  collect()      -> 抛 NotImplementedError（显式，不返回假数据）

绝不返回半真数据；绝不静默 0。known_limits 记录于
docs/architecture/observability/RESOURCE_MONITORING_CONTRACT.md。
"""
from __future__ import annotations

from typing import Any, Dict


def is_available() -> bool:
    """Windows PDH/ETW 后端当前不可用（显式未实现）。"""
    return False


def collect() -> Dict[str, Any]:
    """Windows PDH/ETW 采集未实现 → 显式失败（禁止返回伪造观测）。

    未来实现要点（记录于此，非 TODO 占位）：用 PDH 查询 Process/Processor
    计数器簇获取 CPU/RSS/IO bytes，用 ETW 的 Kernel/FileIO/Thread 事件获取
    queue/lock/io wait 与上下文切换；字段键与 linux_procfs.collect() 对齐。
    """
    raise NotImplementedError(
        "windows_pdh_etw: PDH/ETW 采集尚未实现（LOG-002 仅 Linux procfs 真实路径；"
        "Windows 侧为隔离显式未实现 stub，known_limits 见监控合同文档）")


__all__ = ["collect", "is_available"]
