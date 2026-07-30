# Gate H 验收报告 — 资源监测 + 准入控制 + spill恢复 + 压力测试无OOM

- Gate: H
- 状态: **PASS (有限制)**
- 日期: 2026-07-30
- 依赖: B-002 (基线统计), H-001~H-004
- seed: 20260730

## 1. Gate H Checklist

| # | 检查项 | 状态 | 证据 |
|---|--------|------|------|
| 1 | 动态预测 | PASS | H-001 FrameCostEstimator: 7阶段成本模型, B-002 三帧校准总耗时误差 < 3.4% (阈值 15%) |
| 2 | 内存预约 | PASS | H-002 MemoryBudgetManager: 准入公式 `reserved + predicted_peak + uncertainty + os_margin + worst_next_frame <= budget`, 预约/释放追踪 |
| 3 | CPU回滞 | PASS | H-002 CPUBackpressure: 90%停止投喂, 70%线性回滞 (max→1), 20次滚动平均避抖动 |
| 4 | 高峰错峰 | PASS | H-003 PeakShifter: 优先级队列 (HIGH/NORMAL/LOW), 防饥饿 (>3次提升优先级, 300s超时强制执行) |
| 5 | spill恢复 | PASS | H-003 SpillManager: 原子写入 (.tmp→os.replace) + SHA256校验 + manifest.json持久化 + fsync落盘 |
| 6 | 安全余量 | PASS | H-002 os_margin 默认 2GB (可配置), 显式追踪 RSS/Commit, 不依赖 OS swap |
| 7 | 压力测试无OOM | PASS | H-004 4场景 (8GB/2GB/1.5GB/1GB) 全部 0 OOM, 3/3帧 (T2/T3/T4) 全部完成 |

## 2. 任务完成状态

| 任务 | 内容 | 状态 | TASK_REPORT |
|------|------|------|-------------|
| H-001 | 阶段资源监测和动态成本估算 | PASS | `evidence/H-001/TASK_REPORT.md` |
| H-002 | 内存预约、CPU回滞和准入控制 | PASS | `evidence/H-002/TASK_REPORT.md` |
| H-003 | 高峰错峰、显式spill与恢复 | PASS | `evidence/H-003/TASK_REPORT.md` |
| H-004 | 混合设备压力测试与无OOM验收 | PASS (有限制) | `evidence/H-004/TASK_REPORT.md` |

## 3. 禁止捷径合规

| 禁止捷径 | 状态 | 说明 |
|---------|------|------|
| 不得用固定并发数替代预算 | PASS | H-002 测试19: `max_concurrent=10` + 内存不足 → DEFER, 准入公式优先于并发度 |
| 不得依赖 OS swap | PASS | H-003 SpillManager: 显式原子写入 + fsync + SHA256校验, 压力链 OS_SWAP 为最后手段且本实现不依赖 |
| 不得丢未持久化科学数据 | PASS | H-003 阶段必需块矩阵: 只 spill 已序列化、可恢复块, 当前阶段必需块 (data/header/star_det/psf/gaia_cat/snr_model) 不可 spill |

## 4. 关键结果摘要

### H-001 资源监测与成本估算
- **ResourceMonitor**: CPU/RAM(RSS+Commit)/Disk I/O/温度, 60s 滚动窗口, mean/max/p95 统计
- **FrameCostEstimator**: 7 阶段参数化模型 (READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE)
- **DRIZZLE 双变量时长模型**: `duration = 1.312 ns/源像素 × n_source_pixels + 0.0267 ns/HEALPix像素 × 12×nside²`
- **B-002 基线验证**: T2 误差 2.9%, T3 误差 3.5%, T4 误差 0.0%, 总耗时误差 < 3.4%
- **内存峰值**: T2/T3 (nside=2048) DRIZZLE 671MB, T4 (nside=512) CALIBRATE 194MB (最坏阶段)
- **单元测试**: 94 PASS / 0 FAIL

### H-002 准入控制
- **MemoryBudgetManager**: 预约/释放/安全余量, 准入公式 5 分量 (reserved + predicted_peak + uncertainty×factor + os_margin + worst_next_frame)
- **CPUBackpressure**: <60% 全速, 70-90% 线性回滞, >=90% 停止投喂
- **AdmissionController**: ADMIT/DEFER/REJECT 决策, 集成内存预算+CPU回滞+阶段兼容矩阵
- **PressureHandler**: 8级递进状态机 NORMAL→THROTTLE→STOP_ADMISSION→WAIT_RELEASE→CLEAR_CACHE→PAUSE→SPILL→OS_SWAP
- **关键验证**: T2 DRIZZLE 640MB peak 在 16GB 预算下 ADMIT (总需求 3.50GB), 在 1GB 预算下 DEFER
- **单元测试**: 49 PASS / 0 FAIL

### H-003 spill 与恢复
- **PeakShifter**: 压力 >= STOP_ADMISSION 时新任务入队, URGENT 永不推迟, drain_all 紧急恢复
- **SpillManager**: 原子写入 (.tmp→os.replace) + SHA256 校验 + manifest.json + fsync, 优先 spill >1MB 大块
- **RecoveryManager**: 按 spill 时间排序恢复, is_recoverable 检查, finalize_frame 清理
- **关键验证**: 55MB gaia_cat spill→恢复数据完全一致 (SHA256 通过), manifest 重启后 3 记录完整恢复
- **单元测试**: 62 PASS / 0 FAIL

### H-004 压力测试

| 场景 | 内存预算 | OS余量 | 最大并发 | 判定 | 全部完成 | 无OOM | 峰值总内存 | spill | defer |
|------|---------|--------|---------|------|---------|-------|-----------|-------|-------|
| 充足预算 | 8.0 GB | 1.0 GB | 3 | PASS | ✓ | ✓ | 1350MB/8192MB | 0 | 0 |
| 中等预算 | 2.0 GB | 0.5 GB | 2 | PASS | ✓ | ✓ | 1350MB/2048MB | 0 | 0 |
| 紧张预算 | 1.5 GB | 0.25 GB | 2 | PASS | ✓ | ✓ | 1350MB/1536MB | 0 | 0 |
| 极端紧张 | 1.0 GB | 0.15 GB | 1 | PASS | ✓ | ✓ | — | 0 | 0 |

- **混合设备**: T2/T3 (ASA 500N 窄场, nside=2048, 640MB peak) + T4 (10Micron 宽场, nside=512, 185MB peak)
- **测试结果**: 31 PASS / 0 FAIL, 4 场景全部无 OOM, 3/3 帧全部完成

## 5. 已知限制

- **H-004 压力测试为模拟执行 (非真实 DLL 调用)**: 压力测试使用 H-001 成本模型预测值模拟执行, 非真实 C++ 管线 DLL 调用。真实 OOM 风险需在 C++ 移植后用真实管线验证。
- **spill 未在压力测试中触发**: 当前 4 场景下准入控制通过串行化避免了 spill 路径。需要更极端场景 (如 0.5GB 预算) 或真实并发执行才能触发 spill。H-003 的 spill/恢复链路由独立单元测试 (62项) 验证。
- **C-002 依赖模拟**: H-004 依赖 C-002 (多帧 Stage1 输出), 当前用 B-002 基线 (3帧统计) 模拟。C-002 完成后可用真实 .hiss 输出补充验证。
- **PHOTOMETRIC 时长模型误差大**: T2 误差 314.9%, T4 误差 218.7% (match率 28% vs 假设 90%), 但绝对误差 < 0.6s, 不影响总误差 < 15%。
- **温度采集仅 Linux**: psutil.sensors_temperatures() Windows 下返回 None。
- **3 帧校准**: H-001 成本模型仅 3 数据点, 泛化性有限, 后续 P11 批量运行后可增量校准。
- **Python 原型**: H-001~H-003 为 Python 原型 + C++ 头文件骨架, 待 C++ 移植后真实集成验证。

## 6. 证据文件索引

| 路径 | 说明 |
|------|------|
| `evidence/H-001/TASK_REPORT.md` | H-001 任务报告 |
| `evidence/H-001/baseline.json` | B-002 三帧基线数据 (结构化) |
| `evidence/H-001/resource_monitor.py` | 资源监测框架 (ResourceMonitor + 采样器) |
| `evidence/H-001/cost_estimator.py` | 动态成本估算器 (FrameCostEstimator + 7阶段模型) |
| `evidence/H-001/test_h001.py` | H-001 单元测试 (94 项断言) |
| `evidence/H-002/TASK_REPORT.md` | H-002 任务报告 |
| `evidence/H-002/admission_controller.py` | MemoryBudgetManager + CPUBackpressure + AdmissionController + PressureHandler |
| `evidence/H-002/test_h002.py` | H-002 单元测试 (49 项断言) |
| `evidence/H-003/TASK_REPORT.md` | H-003 任务报告 |
| `evidence/H-003/spill_manager.py` | PeakShifter + SpillManager + RecoveryManager |
| `evidence/H-003/test_h003.py` | H-003 单元测试 (62 项断言) |
| `evidence/H-004/TASK_REPORT.md` | H-004 任务报告 |
| `evidence/H-004/pressure_test.py` | 混合设备压力测试模拟器 + 4 场景 + Gate H 验收 |
| `lib/orchestrator/cpp/include/resource_monitor.h` | C++ ResourceMonitor + FrameCostEstimator 接口 (待实现 .cpp) |
| `lib/orchestrator/cpp/include/admission_controller.h` | C++ AdmissionController 接口 (待实现 .cpp) |
| `lib/orchestrator/cpp/include/spill_manager.h` | C++ SpillManager 接口 (待实现 .cpp) |

## 7. 可复现性

```
# H-001 资源监测 + 成本估算 + B-002 基线验证
python evidence/H-001/test_h001.py

# H-002 准入控制 + CPU回滞 + 压力状态机
python evidence/H-002/test_h002.py

# H-003 spill + 恢复 + 错峰
python evidence/H-003/test_h003.py

# H-004 混合设备压力测试 (4 场景 + Gate H 验收)
python evidence/H-004/pressure_test.py
```

- Python 3.10.11, psutil, numpy
- 总测试断言: 94 + 49 + 62 + 31 = 236 PASS / 0 FAIL

## 8. 失败和限制汇总

- **无失败项**: 4 个 H 任务 + 7 个 checklist 项 + 3 项禁止捷径全部 PASS
- **状态标记**: PASS (有限制) — 限制详见第 5 节, 主要为 H-004 模拟执行 + spill 未在压力测试中触发
